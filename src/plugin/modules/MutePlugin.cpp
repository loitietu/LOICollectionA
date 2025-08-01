#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerChatEvent.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"

#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/MutePlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::mute {
    struct MuteOP {
        CommandSelector<Player> Target;
        std::string Cause;
        int Time = 0;
    };

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;
    
    ll::event::ListenerPtr PlayerChatEventListener;

    namespace MainGui {
        void info(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);
            
            std::unordered_map<std::string, std::string> mData = db->getByPrefix("Mute", id + ".");

            std::string mObjectLabel = tr(mObjectLanguage, "mute.gui.info.label");
            ll::string_utils::replaceAll(mObjectLabel, "${target}", id);
            ll::string_utils::replaceAll(mObjectLabel, "${player}", mData.at(id + ".NAME"));
            ll::string_utils::replaceAll(mObjectLabel, "${cause}", mData.at(id + ".CAUSE"));
            ll::string_utils::replaceAll(mObjectLabel, "${subtime}", SystemUtils::formatDataTime(mData.at(id + ".SUBTIME"), "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(mData.at(id + ".TIME"), "None"));

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.remove.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "mute.gui.info.remove"), [id](Player&) -> void {
                delMute(id);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::remove(pl);
            });
        }

        void content(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "mute.gui.add.title"));
            form.appendLabel(tr(mObjectLanguage, "mute.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "mute.gui.add.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "mute.gui.add.input2"), "", "0");
            form.sendTo(player, [&](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::add(pl);

                addMute(target, std::get<std::string>(dt->at("Input1")), SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0));
            });
        }

        void add(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.add.title"), tr(mObjectLanguage, "mute.gui.add.label"));
            ll::service::getLevel()->forEachPlayer([&form](Player& mTarget) -> bool {
                if (mTarget.isSimulatedPlayer())
                    return true;

                form.appendButton(mTarget.getRealName(), [&mTarget](Player& pl) -> void  {
                    MainGui::content(pl, mTarget);
                });
                return true;
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void remove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.remove.title"), tr(mObjectLanguage, "mute.gui.remove.label"));
            for (std::string& mItem : getMutes()) {
                form.appendButton(mItem, [mItem](Player& pl) -> void {
                    MainGui::info(pl, mItem);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.title"), tr(mObjectLanguage, "mute.gui.label"));
            form.appendButton(tr(mObjectLanguage, "mute.gui.addMute"), "textures/ui/backup_replace", "path", [](Player& pl) -> void {
                MainGui::add(pl);
            });
            form.appendButton(tr(mObjectLanguage, "mute.gui.removeMute"), "textures/ui/free_download_symbol", "path", [](Player& pl) -> void {
                MainGui::remove(pl);
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("mute", tr({}, "commands.mute.description"), CommandPermissionLevel::GameDirectors);
            command.overload<MuteOP>().text("add").required("Target").optional("Cause").optional("Time").execute(
                [](CommandOrigin const& origin, CommandOutput& output, MuteOP const& param) -> void {
                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    if (isMute(*pl) || pl->getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors || pl->isSimulatedPlayer()) {
                        output.error(fmt::runtime(tr({}, "commands.mute.error.add")), pl->getRealName());
                        continue;
                    }
                    addMute(*pl, param.Cause, param.Time);

                    output.success(fmt::runtime(tr({}, "commands.mute.success.add")), pl->getRealName());
                }
            });
            command.overload<MuteOP>().text("remove").required("Target").execute(
                [](CommandOrigin const& origin, CommandOutput& output, MuteOP const& param) -> void {
                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));
                
                for (Player*& pl : results) {
                    if (!isMute(*pl)) {
                        output.error(fmt::runtime(tr({}, "commands.mute.error.remove")), pl->getRealName());
                        continue;
                    }
                    delMute(*pl);

                    output.success(fmt::runtime(tr({}, "commands.mute.success.remove")), pl->getRealName());
                }
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                
                MainGui::open(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }

        void listenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([](ll::event::PlayerChatEvent& event) -> void {
                std::string mUuid = event.self().getUuid().asString();

                std::vector<std::string> mKeys = getMutes();
                std::for_each(mKeys.begin(), mKeys.end(), [mUuid, &event](const std::string& mId) -> void {
                    if (mId != mUuid)
                        return;

                    if (SystemUtils::isReach(db->get("Mute", mId + ".TIME"))) {
                        delMute(event.self());
                        return;
                    }

                    std::string mObjectTips = tr(getLanguage(event.self()), "mute.tips");
                    ll::string_utils::replaceAll(mObjectTips, "${cause}", db->get("Mute", mId + ".CAUSE"));
                    ll::string_utils::replaceAll(mObjectTips, "${time}", SystemUtils::formatDataTime(db->get("Mute", mId + ".TIME"), "None"));
                    
                    event.self().sendMessage(mObjectTips);
                    event.cancel();
                    
                    return;
                });
            }, ll::event::EventPriority::Highest);
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerChatEventListener);
        }
    }

    void addMute(Player& player, const std::string& cause, int time) {
        if (player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors || !isValid())
            return;

        std::string mCause = cause.empty() ? "None" : cause;

        std::string mTimestamp = SystemUtils::getCurrentTimestamp();

        db->set("Mute", mTimestamp + ".NAME", player.getRealName());
        db->set("Mute", mTimestamp + ".CAUSE", mCause);
        db->set("Mute", mTimestamp + ".TIME", time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time, "0") : "0");
        db->set("Mute", mTimestamp + ".SUBTIME", SystemUtils::getNowTime("%Y%m%d%H%M%S"));
        db->set("Mute", mTimestamp + ".DATA", player.getUuid().asString());

        logger->info(LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "mute.log1"), "${cause}", mCause), player)
        );
    }

    void delMute(Player& player) {
        if (!isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        
        delMute(mObject);
    }

    void delMute(const std::string& target) {
        if (!isValid())
            return;

        db->delByPrefix("Mute", target + ".");

        logger->info(ll::string_utils::replaceAll(tr({}, "mute.log2"), "${target}", target));
    }

    std::vector<std::string> getMutes() {
        if (!isValid())
            return {};

        std::vector<std::string> mResult;

        std::vector<std::string> mKeys = db->listByPrefix("Mute", "%.");
        std::for_each(mKeys.begin(), mKeys.end(), [&mResult](const std::string& mId) -> void {
            std::string mData = mId.substr(0, mId.find_last_of('.'));

            mResult.push_back(mData);
        });

        std::sort(mResult.begin(), mResult.end());
        mResult.erase(std::unique(mResult.begin(), mResult.end()), mResult.end());

        return mResult;
    }

    bool isMute(Player& player) {
        if (!isValid())
            return false;

        std::string mUuid = player.getUuid().asString();

        std::vector<std::string> mKeys = getMutes();
        auto it = std::find_if(mKeys.begin(), mKeys.end(), [&mUuid](const std::string& mId) -> bool {
            return mId == mUuid;
        });

        return it != mKeys.end();
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");

        db->create("Mute");
        
        registerCommand();
        listenEvent();
    }
    
    void unregistery() {
        unlistenEvent();

        db->exec("VACUUM;");
    }
}

/*
    Database
    -> Mute
        -> TIMESTAMP.NAME: NAME
        -> TIMESTAMP.CAUSE: CAUSE
        -> TIMESTAMP.TIME: TIME
        -> TIMESTAMP.SUBTIME: SUBTIME
        -> TIMESTAMP.DATA: UUID
*/