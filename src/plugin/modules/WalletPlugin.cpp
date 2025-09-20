#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <ll/api/form/SimpleForm.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/service/PlayerInfo.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerChatEvent.h>
#include <ll/api/utils/RandomUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

#include <mc/network/packet/TextPacket.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"

#include "utils/ScoreboardUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "ConfigPlugin.h"

#include "include/WalletPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

std::unordered_map<std::string, std::vector<std::unordered_map<std::string, std::string>>> mRedEnvelopeMap;

namespace LOICollection::Plugins::wallet {
    struct WalletOP {
        CommandSelector<Player> Target;
        int Score = 0;
    };

    C_Config::C_Plugins::C_Wallet options;
    
    std::shared_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;

    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerChatEventListener;

    namespace MainGui {
        void content(Player& player, const std::string& target, TransferType type) {
            std::string mObjectLanguage = getLanguage(player);

            mce::UUID mTargetUuid = mce::UUID::fromString(target);
            std::string mTargetName = type == TransferType::online ? 
                ll::service::getLevel()->getPlayer(mTargetUuid)->getRealName() : ll::service::PlayerInfo::getInstance().fromUuid(mTargetUuid)->name;

            std::string mLabel = tr(mObjectLanguage, "wallet.gui.label") + "\n" + tr(mObjectLanguage, "wallet.gui.transfer.label2");

            ll::form::CustomForm form(tr(mObjectLanguage, "wallet.gui.title"));
            form.appendLabel(fmt::format(fmt::runtime(mLabel), 
                ScoreboardUtils::getScore(player, options.TargetScoreboard),
                std::to_string(options.ExchangeRate * 100) + "%%",
                mTargetName
            ));
            form.appendInput("Input", tr(mObjectLanguage, "wallet.gui.transfer.input"), tr(mObjectLanguage, "wallet.gui.transfer.input.placeholder"));
            form.sendTo(player, [target, mTargetName, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::transfer(pl, type);

                int mMoney = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
                if (ScoreboardUtils::getScore(pl, options.TargetScoreboard) < mMoney || mMoney <= 0) {
                    pl.sendMessage(tr(getLanguage(pl), "wallet.tips"));
                    return MainGui::transfer(pl, type);
                }

                ScoreboardUtils::reduceScore(pl, options.TargetScoreboard, mMoney);

                wallet::transfer(target, (int)(mMoney * (1 - options.ExchangeRate)));

                logger->info(fmt::runtime(tr({}, "wallet.log")), pl.getRealName(), mTargetName, mMoney);
            });
        }

        void transfer(Player& player, TransferType type) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "wallet.gui.title"), tr(mObjectLanguage, "wallet.gui.transfer.label1"));
            switch (type) {
                case TransferType::online:
                    ll::service::getLevel()->forEachPlayer([&form](Player& mTarget) -> bool {
                        if (mTarget.isSimulatedPlayer())
                            return true;

                        form.appendButton(mTarget.getRealName(), [&mTarget](Player& pl) -> void  {
                            MainGui::content(pl, mTarget.getUuid().asString(), TransferType::online);
                        });
                        return true;
                    });
                    break;
                case TransferType::offline:
                    for (const ll::service::PlayerInfo::PlayerInfoEntry& mTarget : ll::service::PlayerInfo::getInstance().entries()) {
                        form.appendButton(mTarget.name, [mTarget](Player& pl) -> void {
                            MainGui::content(pl, mTarget.uuid.asString(), TransferType::offline);
                        });
                    }
                    break;
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void redenvelope(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "wallet.gui.title"));
            form.appendLabel(fmt::format(fmt::runtime(tr(mObjectLanguage, "wallet.gui.label")),
                ScoreboardUtils::getScore(player, options.TargetScoreboard), std::to_string(options.ExchangeRate * 100) + "%%"
            ));
            form.appendInput("Input1", tr(mObjectLanguage, "wallet.gui.redenvelope.input1"), tr(mObjectLanguage, "wallet.gui.redenvelope.input1.placeholder"));
            form.appendInput("Input2", tr(mObjectLanguage, "wallet.gui.redenvelope.input2"), tr(mObjectLanguage, "wallet.gui.redenvelope.input2.placeholder"));
            form.appendInput("Input3", tr(mObjectLanguage, "wallet.gui.redenvelope.input3"), tr(mObjectLanguage, "wallet.gui.redenvelope.input3.placeholder"));
            form.sendTo(player, [mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::open(pl);

                std::string mObjectKey = std::get<std::string>(dt->at("Input3"));

                if (mObjectKey.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::open(pl);
                }

                int mScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input1")), 0);
                int mCount = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

                if (mScore <= 0 || mCount <= 0 || ScoreboardUtils::getScore(pl, options.TargetScoreboard) < mScore * mCount) {
                    pl.sendMessage(tr(mObjectLanguage, "wallet.tips.redenvelope"));
                    return MainGui::open(pl);
                }

                ScoreboardUtils::reduceScore(pl, options.TargetScoreboard, mScore * mCount);

                wallet::redenvelope(pl, mObjectKey, mScore, mCount);
            });
        }

        void wealth(Player& player) {
            std::string mTipsString = LOICollectionAPI::translateString(tr(getLanguage(player), "wallet.showOff"), player);
            TextPacket::createSystemMessage(
                fmt::format(fmt::runtime(mTipsString), ScoreboardUtils::getScore(player, options.TargetScoreboard))
            ).sendToClients();
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mLabel = tr(mObjectLanguage, "wallet.gui.label");
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "wallet.gui.title"), 
                fmt::format(fmt::runtime(mLabel), 
                    ScoreboardUtils::getScore(player, options.TargetScoreboard),
                    std::to_string(options.ExchangeRate * 100) + "%%"
                )
            );
            form.appendButton(tr(mObjectLanguage, "wallet.gui.transfer"), "textures/ui/MCoin", "path", [](Player& pl) -> void {
                MainGui::transfer(pl, TransferType::online);
            });
            form.appendButton(tr(mObjectLanguage, "wallet.gui.offlineTransfer"), "textures/ui/icon_best3", "path", [](Player& pl) -> void {
                MainGui::transfer(pl, TransferType::offline);
            });
            form.appendButton(tr(mObjectLanguage, "wallet.gui.redenvelope"), "textures/ui/comment", "path", [](Player& pl) -> void {
                MainGui::redenvelope(pl);
            });
            form.appendButton(tr(mObjectLanguage, "wallet.gui.wealth"), "textures/ui/creative_icon", "path", [](Player& pl) -> void {
                MainGui::wealth(pl);
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("wallet", tr({}, "commands.wallet.description"), CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                MainGui::open(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
            command.overload<WalletOP>().text("transfer").required("Target").required("Score").execute(
                [](CommandOrigin const& origin, CommandOutput& output, WalletOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                int mMoney = param.Score * (int) results.size();
                if (ScoreboardUtils::getScore(player, options.TargetScoreboard) < mMoney || param.Score < 0)
                    return output.error(tr({}, "commands.wallet.error.score"));

                ScoreboardUtils::reduceScore(player, options.TargetScoreboard, mMoney);

                int mTargetMoney = (int)(param.Score * (1 - options.ExchangeRate));
                for (Player*& target : results)
                    ScoreboardUtils::addScore(*target, options.TargetScoreboard, mTargetMoney);

                output.success(fmt::runtime(tr({}, "commands.wallet.success.transfer")), param.Score, results.size());
            });
            command.overload().text("wealth").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                MainGui::wealth(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
        }

        void listenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([](ll::event::PlayerJoinEvent& event) -> void {
                if (event.self().isSimulatedPlayer())
                    return;

                std::string mObject = event.self().getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                
                if (!db->has("OBJECT$" + mObject, "Wallet_Score"))
                    db->set("OBJECT$" + mObject, "Wallet_Score", "0");

                if (int mScore = SystemUtils::toInt(db->get("OBJECT$" + mObject, "Wallet_Score"), 0); mScore > 0) {
                    ScoreboardUtils::addScore(event.self(), options.TargetScoreboard, mScore);

                    db->set("OBJECT$" + mObject, "Wallet_Score", "0");
                }
            });
            PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([](ll::event::PlayerChatEvent& event) -> void {
                if (event.self().isSimulatedPlayer())
                    return;

                auto it = mRedEnvelopeMap.find(event.message());
                if (it == mRedEnvelopeMap.end())
                    return;

                std::string mObjectUuid = event.self().getUuid().asString();
                for (auto& mObject : it->second) {
                    if (mObject.at("receiver").find(mObjectUuid) != std::string::npos)
                        continue;

                    int mObjectCapacity = SystemUtils::toInt(mObject.at("capacity"), 0);
                    int mObjectPeople = SystemUtils::toInt(mObject.at("people"), 0) + 1;
                    int mObjectCount = SystemUtils::toInt(mObject.at("count"), 0);

                    if ((mObjectPeople - 1) == mObjectCount) {
                        ScoreboardUtils::addScore(event.self(), options.TargetScoreboard, mObjectCapacity);

                        std::string mTipsString = LOICollectionAPI::translateString(tr(getLanguage(event.self()), "wallet.tips.redenvelope.receive.over"), event.self());
                        TextPacket::createSystemMessage(fmt::format(fmt::runtime(mTipsString), mObject.at("id"), mObjectCapacity)).sendToClients();

                        std::string mObjectId = mObject.at("id");
                        it->second.erase(std::remove_if(it->second.begin(), it->second.end(), [mObjectId](auto& mObject) -> bool {
                            return mObject.at("id") == mObjectId;
                        }), it->second.end());
                        
                        continue;
                    }

                    int mMaxScore = (mObjectCapacity / (mObjectCount - mObjectPeople + 1)) * 2;
                    int mTargetMoney = ll::random_utils::rand(0, mMaxScore);

                    ScoreboardUtils::addScore(event.self(), options.TargetScoreboard, mTargetMoney);

                    std::string mTipsString = LOICollectionAPI::translateString(tr(getLanguage(event.self()), "wallet.tips.redenvelope.receive"), event.self());
                    TextPacket::createSystemMessage(fmt::format(fmt::runtime(mTipsString), mObject.at("id"), mTargetMoney, mObjectPeople, mObjectCount)).sendToClients();

                    mObject["people"] = std::to_string(mObjectPeople);
                    mObject["capacity"] = std::to_string(mObjectCapacity - mTargetMoney);
                    mObject["receiver"].append(mObjectUuid + " ");
                }
            });
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerJoinEventListener);
            eventBus.removeListener(PlayerChatEventListener);
        }
    }

    void transfer(const std::string& target, int score) {
        if (!isValid())
            return; 

        if (Player* mObject = ll::service::getLevel()->getPlayer(mce::UUID::fromString(target)); mObject) {
            ScoreboardUtils::addScore(*mObject, options.TargetScoreboard, score);
            return;
        }

        std::string mObject = target;
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        int mWalletScore = SystemUtils::toInt(db->get("OBJECT$" + mObject, "Wallet_Score"), 0);

        db->set("OBJECT$" + mObject, "Wallet_Score", std::to_string(mWalletScore + score));
    }

    void redenvelope(Player& player, const std::string& key, int score, int count) {
        if (!isValid())
            return; 

        std::string mObjectId = SystemUtils::getCurrentTimestamp();

        mRedEnvelopeMap[key].push_back({
            { "id", mObjectId },
            { "player", player.getUuid().asString() },
            { "count", std::to_string(count) },
            { "capacity", std::to_string(score * count) },
            { "people", "0" },
            { "receiver", "" }
        });

        ll::coro::keepThis([mObjectId, key]() -> ll::coro::CoroTask<> {
            co_await std::chrono::seconds(options.RedEnvelopeTimeout);
            
            std::vector<std::unordered_map<std::string, std::string>>& mObjects = mRedEnvelopeMap[key];
            for (std::unordered_map<std::string, std::string>& mObject : mObjects) {
                if (mObject.at("id") != mObjectId)
                    continue;

                transfer(mObject.at("player"), SystemUtils::toInt(mObject.at("capacity"), 0));

                TextPacket::createSystemMessage(
                    fmt::format(fmt::runtime(tr({}, "wallet.tips.redenvelope.timeout")), mObjectId)
                ).sendToClients();

                break;
            };

            mObjects.erase(std::remove_if(mObjects.begin(), mObjects.end(), [mObjectId](auto& mObject) -> bool {
                return mObject.at("id") == mObjectId;
            }), mObjects.end());
        }).launch(ll::thread::ServerThreadExecutor::getDefault());

        std::string mTipsString = LOICollectionAPI::translateString(tr(getLanguage(player), "wallet.tips.redenvelope.content"), player);
        TextPacket::createSystemMessage(fmt::format(fmt::runtime(mTipsString), 
            mObjectId, score, count, options.RedEnvelopeTimeout, key
        )).sendToClients();
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* setting) {
        db = *static_cast<std::shared_ptr<SQLiteStorage>*>(setting);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        options = Config::GetBaseConfigContext().Plugins.Wallet;
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        unlistenEvent();

        for (auto& it : mRedEnvelopeMap) {
            for (auto& mObject : it.second) {
                transfer(mObject.at("player"), SystemUtils::toInt(mObject.at("capacity"), 0));
            }
        }

        mRedEnvelopeMap.clear();
    }
}