#include <atomic>
#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <filesystem>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/EnumName.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/Form/PaginatedForm.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/include/ServerEvents/modules/NoticeEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/JsonStorage.h"
#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/NoticePlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    enum class NoticeObject;

    constexpr inline auto NoticeObjectName = ll::command::enum_name_v<NoticeObject>;

    struct NoticePlugin::operation {
        ll::command::SoftEnum<NoticeObject> Object;
    };

    struct NoticePlugin::Impl {
        LRUKCache<std::string, bool> CloseCache;

        std::atomic<bool> mRegistered{ false };

        bool ModuleEnabled = false;

        std::shared_ptr<JsonStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr NoticeCreateEventListener;
        ll::event::ListenerPtr NoticeDeleteEventListener;

        Impl() : CloseCache(100, 100) {}
    };

    NoticePlugin::NoticePlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    NoticePlugin::~NoticePlugin() = default;

    NoticePlugin& NoticePlugin::getInstance() {
        static NoticePlugin instance;
        return instance;
    }
    
    std::shared_ptr<JsonStorage> NoticePlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> NoticePlugin::getLogger() {
        return this->mImpl->logger;
    }

    void NoticePlugin::gui::setting(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
        form.appendToggle("Toggle1", tr(mObjectLanguage, "notice.gui.setting.switch1"), this->mParent.isClose(player));
        form.sendTo(player, [this](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return;

            this->mParent.mImpl->db2->set("Notice", pl.getUuid().asString(), "close", 
                std::get<uint64>(dt->at("Toggle1")) ? "true" : "false"
            );
        });
    }

    void NoticePlugin::gui::content(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "notice.gui.error"));
            
            return;
        }

        ll::form::CustomForm form(tr(mObjectLanguage, "notice.gui.title"));
        form.appendLabel(tr(mObjectLanguage, "notice.gui.label"));
        form.appendInput("Input", tr(mObjectLanguage, "notice.gui.edit.title"), "", this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title", ""));

        std::string mObjectLine = tr(mObjectLanguage, "notice.gui.edit.line");

        auto content = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content");
        for (const auto& [index, line] : std::views::enumerate(content))
            form.appendInput("Content" + std::to_string(index), fmt::format(fmt::runtime(mObjectLine), index + 1), "", line);

        form.appendToggle("Toggle", tr(mObjectLanguage, "notice.gui.edit.show"), this->mParent.getDatabase()->get_ptr<bool>("/" + id + "/poiontout", false));
        form.appendStepSlider("StepSlider", tr(mObjectLanguage, "notice.gui.edit.operation"), { "no", "add", "remove" });
        form.sendTo(player, [this, id](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) mutable -> void {
            if (!dt) return this->edit(pl);

            this->mParent.getDatabase()->set_ptr("/" + id + "/title", std::get<std::string>(dt->at("Input")));
            this->mParent.getDatabase()->set_ptr("/" + id + "/poiontout", static_cast<bool>(std::get<uint64>(dt->at("Toggle"))));

            auto content = this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content");
            switch (ll::hash_utils::doHash(std::get<std::string>(dt->at("StepSlider")))) {
                case ll::hash_utils::doHash("add"): 
                    content.push_back("");
                    break;
                case ll::hash_utils::doHash("remove"): 
                    content.erase(content.end() - 1);
                    break;
                default:
                    for (auto&& [index, line] : std::views::enumerate(content))
                        line = std::get<std::string>(dt->at("Content" + std::to_string(index)));
            }

            this->mParent.getDatabase()->set_ptr("/" + id + "/content", content);
            this->mParent.getDatabase()->save();

            this->content(pl, id);

            this->mParent.getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "notice.log1"), pl));
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

            this->mParent.create(mObjectId, mObjectTitle, 
                SystemUtils::toInt(std::get<std::string>(dt->at("Input3")), 0), static_cast<bool>(std::get<uint64>(dt->at("Toggle1")))
            );

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "notice.log2"), pl)), mObjectId);
        });
    }
    
    void NoticePlugin::gui::contentRemoveInfo(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "notice.gui.error"));
            
            this->edit(player);
            return;
        }

        ll::form::ModalForm form(tr(mObjectLanguage, "notice.gui.title"), 
            fmt::format(fmt::runtime(tr(mObjectLanguage, "notice.gui.remove.content")), id),
            tr(mObjectLanguage, "notice.gui.remove.yes"), tr(mObjectLanguage, "notice.gui.remove.no")
        );
        form.sendTo(player, [this, id](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) mutable -> void {
            if (result != ll::form::ModalFormSelectedButton::Upper) 
                return this->edit(pl);

            this->mParent.remove(id);

            this->mParent.getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "notice.log3"), pl)), id);
        });
    }

    void NoticePlugin::gui::contentRemove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "notice.gui.title"),
            tr(mObjectLanguage, "notice.gui.label"),
            this->mParent.getDatabase()->keys()
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->contentRemoveInfo(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->edit(pl);
        });

        form->sendPage(player, 1);
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
        form.appendDivider();
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
            const nlohmann::ordered_json& mObject = data.at(pair.first);
            
            ll::form::CustomForm form(LOICollectionAPI::APIUtils::getInstance().translate(mObject.value("title", ""), player));
            for (const auto& line : mObject.value("content", nlohmann::ordered_json::array()))
                form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(line, player));

            form.sendTo(player);
        }
    }

    void NoticePlugin::gui::notice(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.has(id)) {
            player.sendMessage(tr(mObjectLanguage, "notice.gui.error"));
            
            return;
        }

        ll::form::CustomForm form(LOICollectionAPI::APIUtils::getInstance().translate(this->mParent.getDatabase()->get_ptr<std::string>("/" + id + "/title", ""), player));
        for (const auto& line : this->mParent.getDatabase()->get_ptr<nlohmann::ordered_json>("/" + id + "/content"))
            form.appendLabel(LOICollectionAPI::APIUtils::getInstance().translate(line, player));
        
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

        std::vector<std::string> mNoticeNames;
        std::vector<std::string> mNoticeIds;

        for (const auto& pair : mContent) {
            mNoticeNames.push_back(LOICollectionAPI::APIUtils::getInstance().translate(data.at(pair.first).value("title", ""), player));
            mNoticeIds.push_back(pair.first);
        }

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "notice.gui.title"),
            tr(mObjectLanguage, "notice.gui.label"),
            mNoticeNames
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mNoticeIds = std::move(mNoticeIds)](Player& pl, int index) -> void {
            this->notice(pl, mNoticeIds.at(index));
        });

        form->sendPage(player, 1);
    }

    void NoticePlugin::registeryCommand() {
        ll::command::CommandRegistrar::getInstance(false).tryRegisterSoftEnum(NoticeObjectName, this->getDatabase()->keys());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("notice", tr({}, "commands.notice.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("gui").optional("Object").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                param.Object.empty() ? this->mGui->open(player) : this->mGui->notice(player, param.Object);

                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
            });
        command.overload().text("edit").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->edit(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("setting").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->setting(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    void NoticePlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();

            if (!this->mImpl->db2->has("Notice", mObject)) {
                std::unordered_map<std::string, std::string> mData = {
                    { "name", event.self().getRealName() },
                    { "close", "false" }
                };

                this->mImpl->db2->set("Notice", mObject, mData);
            }
            
            if (this->isClose(event.self()))
                return;

            this->mGui->notice(event.self());
        });

        this->mImpl->NoticeCreateEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::NoticeCreateEvent>([](LOICollection::ServerEvents::NoticeCreateEvent& event) mutable -> void {
            ll::command::CommandRegistrar::getInstance(false).addSoftEnumValues(NoticeObjectName, { event.getTarget() });
        });

        this->mImpl->NoticeDeleteEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::NoticeDeleteEvent>([](LOICollection::ServerEvents::NoticeDeleteEvent& event) mutable -> void {
            ll::command::CommandRegistrar::getInstance(false).removeSoftEnumValues(NoticeObjectName, { event.getTarget() });
        });
    }

    void NoticePlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->NoticeCreateEventListener);
        eventBus.removeListener(this->mImpl->NoticeDeleteEventListener);
    }

    void NoticePlugin::create(const std::string& id, const std::string& title, int priority, bool poiontout) {
        if (!this->isValid())
            return;

        nlohmann::ordered_json data = {
            { "title", title },
            { "content", nlohmann::ordered_json::array() },
            { "priority", priority },
            { "poiontout", poiontout }
        };

        this->getDatabase()->set(id, data);
        this->getDatabase()->save();
    }

    void NoticePlugin::remove(const std::string& id) {
        if (!this->isValid())
            return;

        this->getDatabase()->remove(id);
        this->getDatabase()->save();
    }

    bool NoticePlugin::has(const std::string& id) {
        if (!this->isValid())
            return false;

        return this->getDatabase()->has(id);
    }

    bool NoticePlugin::isClose(Player& player) {
        if (!this->isValid()) 
            return false;

        std::string mObject = player.getUuid().asString();

        if (this->mImpl->CloseCache.contains(mObject))
            return *this->mImpl->CloseCache.get(mObject).value();

        bool result = this->mImpl->db2->get("Notice", mObject, "close", "false") == "true";

        this->mImpl->CloseCache.put(mObject, result);

        return result;
    }

    bool NoticePlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    bool NoticePlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Notice)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_shared<JsonStorage>(mDataPath / "notice.json");
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool NoticePlugin::unload() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->db2.reset();
        this->mImpl->logger.reset();
        this->mImpl->ModuleEnabled = false;

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool NoticePlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->mImpl->db2->create("Notice", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("close");
        });

        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool NoticePlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->save();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("NoticePlugin", LOICollection::Plugins::NoticePlugin, LOICollection::Plugins::NoticePlugin::getInstance(), LOICollection::modules::ModulePriority::High)
