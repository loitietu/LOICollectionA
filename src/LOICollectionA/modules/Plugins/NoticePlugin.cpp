#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include <nlohmann/json.hpp>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/utils/SystemUtils.h"
#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/data/JsonStorage.h"
#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/NoticePlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    struct NoticePlugin::operation {
        std::string Id;
    };

    struct NoticePlugin::Impl {
        bool ModuleEnabled = false;

        std::unique_ptr<JsonStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;
    };

    NoticePlugin::NoticePlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    NoticePlugin::~NoticePlugin() = default;
    
    JsonStorage* NoticePlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* NoticePlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void NoticePlugin::gui::setting(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
        form.appendToggle("Toggle1", tr(mObjectLanguage, "notice.gui.setting.switch1"), this->mParent.isClose(player));
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return;

            std::string mObject = pl.getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            this->mParent.mImpl->db2->set("OBJECT$" + mObject, "Notice_Toggle1", 
                std::get<uint64>(dt->at("Toggle1")) ? "true" : "false"
            );
        });
    }

    void NoticePlugin::gui::content(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
        form.appendInput("Input", tr(mObjectLanguage, "notice.gui.edit.title"), "", this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title", ""));

        std::string mObjectLine = tr(mObjectLanguage, "notice.gui.edit.line");

        auto content = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content");
        for (int i = 0; i < (int) content.size(); i++) {
            std::string mLine = fmt::format(fmt::runtime(mObjectLine), i + 1);
            form.appendInput("Content" + std::to_string(i), mLine, "", content.at(i));
        }

        form.appendToggle("Toggle", tr(mObjectLanguage, "notice.gui.edit.show"), this->mParent.getDatabase()->get_ptr<bool>("/" + id + "/poiontout", false));
        form.appendStepSlider("StepSlider", tr(mObjectLanguage, "notice.gui.edit.operation"), { "no", "add", "remove" });
        form.sendTo(player, [this, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->edit(pl);

            this->mParent.getDatabase()->set_ptr("/" + id + "/title", std::get<std::string>(dt->at("Input")));
            this->mParent.getDatabase()->set_ptr("/" + id + "/poiontout", (bool) std::get<uint64>(dt->at("Toggle")));

            auto content = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content");
            switch (ll::hash_utils::doHash(std::get<std::string>(dt->at("StepSlider")))) {
                case ll::hash_utils::doHash("add"): 
                    content.push_back("");
                    break;
                case ll::hash_utils::doHash("remove"): 
                    content.erase(content.end() - 1);
                    break;
                default:
                    for (int i = 0; i < (int) content.size(); i++)
                        content.at(i) = std::get<std::string>(dt->at("Content" + std::to_string(i)));
            }

            this->mParent.getDatabase()->set_ptr("/" + id + "/content", content);
            this->mParent.getDatabase()->save();

            this->content(pl, id);

            this->mParent.getLogger()->info(LOICollectionAPI::getVariableString(tr({}, "notice.log1"), pl));
        });
    }

    void NoticePlugin::gui::contentAdd(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "notice.gui.add.input1"), tr(mObjectLanguage, "notice.gui.add.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "notice.gui.add.input2"), tr(mObjectLanguage, "notice.gui.add.input2.placeholder"));
        form.appendInput("Input3", tr(mObjectLanguage, "notice.gui.add.input3"), tr(mObjectLanguage, "notice.gui.add.input3.placeholder"));
        form.appendToggle("Toggle1", tr(mObjectLanguage, "notice.gui.add.switch"), false);
        form.sendTo(player, [this, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->edit(pl);

            std::string mObjectId = std::get<std::string>(dt->at("Input1"));
            std::string mObjectTitle = std::get<std::string>(dt->at("Input2"));

            if (mObjectId.empty() || mObjectTitle.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->edit(pl);
            }

            nlohmann::ordered_json mObject = {
                { "title", mObjectTitle },
                { "content", nlohmann::ordered_json::array() },
                { "priority", SystemUtils::toInt(std::get<std::string>(dt->at("Input3")), 0) },
                { "poiontout", (bool)std::get<uint64>(dt->at("Toggle1")) }
            };

            this->mParent.getDatabase()->set(mObjectId, mObject);
            this->mParent.getDatabase()->save();

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "notice.log2"), pl)), mObjectId);
        });
    }
    
    void NoticePlugin::gui::contentRemoveInfo(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::ModalForm form(tr(mObjectLanguage, "notice.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "notice.gui.remove.content")), id),
            tr(mObjectLanguage, "notice.gui.remove.yes"), tr(mObjectLanguage, "notice.gui.remove.no")
        );
        form.sendTo(player, [this, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
            if (result != ll::form::ModalFormSelectedButton::Upper) 
                return;

            this->mParent.getDatabase()->remove(id);
            this->mParent.getDatabase()->save();

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::getVariableString(tr({}, "notice.log3"), pl)), id);
        });
    }

    void NoticePlugin::gui::contentRemove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "notice.gui.title"), tr(mObjectLanguage, "notice.gui.label"));
        for (const std::string& key : this->mParent.getDatabase()->keys()) {
            form.appendButton(key, [this, key](Player& pl) {
                this->contentRemoveInfo(pl, key);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->edit(pl);
        });
    }

    void NoticePlugin::gui::edit(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "notice.gui.title"), tr(mObjectLanguage, "notice.gui.label"));
        form.appendButton(tr(mObjectLanguage, "notice.gui.add"), "textures/ui/book_addtextpage_default", "path", [this](Player& pl) -> void {
            this->contentAdd(pl);
        });
        form.appendButton(tr(mObjectLanguage, "notice.gui.remove"), "textures/ui/book_trash_default", "path", [this](Player& pl) -> void {
            this->contentRemove(pl);
        });
        for (const std::string& key : this->mParent.getDatabase()->keys()) {
            form.appendButton(key, [this, key](Player& pl) {
                this->content(pl, key);
            });
        }
        form.sendTo(player);
    }

    void NoticePlugin::gui::notice(Player& player) {
        nlohmann::ordered_json data = this->mParent.getDatabase()->get();

        std::vector<std::pair<std::string, int>> mContent;
        for (auto it = data.begin(); it != data.end(); ++it) {
            if (!it.value().value("poiontout", false))
                continue;
            mContent.emplace_back(it.key(), it.value().value("priority", 0));
        }

        std::sort(mContent.begin(), mContent.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

        for (const auto& pair : mContent) {
            nlohmann::ordered_json mObject = data.at(pair.first);
            
            ll::form::CustomForm form(LOICollectionAPI::translateString(mObject.value("title", ""), player));
            for (const auto& line : mObject.value("content", nlohmann::ordered_json::array()))
                form.appendLabel(LOICollectionAPI::translateString(line, player));
            form.sendTo(player);
        }
    }

    void NoticePlugin::gui::notice(Player& player, const std::string& id) {
        if (!this->mParent.getDatabase()->has(id)) {
            this->mParent.getLogger()->error("NoticeUI {} reading failed.", id);
            return;
        }

        ll::form::CustomForm form(LOICollectionAPI::translateString(this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title", ""), player));
        for (const auto& line : this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content"))
            form.appendLabel(LOICollectionAPI::translateString(line, player));
        form.sendTo(player);
    }

    void NoticePlugin::gui::open(Player& player) {
        nlohmann::ordered_json data = this->mParent.getDatabase()->get();

        std::vector<std::pair<std::string, int>> mContent;
        for (auto it = data.begin(); it != data.end(); ++it)
            mContent.emplace_back(it.key(), it.value().value("priority", 0));

        std::sort(mContent.begin(), mContent.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        ll::form::SimpleForm form(tr(mObjectLanguage, "notice.gui.title"), tr(mObjectLanguage, "notice.gui.label"));
        for (const auto& pair : mContent) {
            form.appendButton(LOICollectionAPI::translateString(data.at(pair.first).value("title", ""), player), [this, id = pair.first](Player& pl) {
                this->notice(pl, id);
            });
        }
        form.sendTo(player);
    }

    void NoticePlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("notice", tr({}, "commands.notice.description"), CommandPermissionLevel::Any);
        command.overload<operation>().text("gui").optional("Id").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            param.Id.empty() ? this->mGui->open(player) : this->mGui->notice(player, param.Id);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("edit").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr({}, "commands.generic.permission"));

            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->edit(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("setting").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->setting(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
    }

    void NoticePlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');

            if (!this->mImpl->db2->has("OBJECT$" + mObject, "Notice_Toggle1"))
                this->mImpl->db2->set("OBJECT$" + mObject, "Notice_Toggle1", "false");
            
            if (this->isClose(event.self()))
                return;

            this->mGui->notice(event.self());
        });
    }

    void NoticePlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
    }

    bool NoticePlugin::isClose(Player& player) {
        if (!this->isValid()) 
            return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (this->mImpl->db2->has("OBJECT$" + mObject, "Notice_Toggle1"))
            return this->mImpl->db2->get("OBJECT$" + mObject, "Notice_Toggle1") == "true";
        return false;
    }

    bool NoticePlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    bool NoticePlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Notice)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_unique<JsonStorage>(mDataPath / "notice.json");
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool NoticePlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->registeryCommand();
        this->listenEvent();

        return true;
    }

    bool NoticePlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->save();

        return true;
    }
}

REGISTRY_HELPER("NoticePlugin", LOICollection::Plugins::NoticePlugin, LOICollection::Plugins::NoticePlugin::getInstance())
