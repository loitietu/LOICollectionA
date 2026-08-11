#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

#include <fmt/core.h>

#include <ll/api/Expected.h>
#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/base/Containers.h>

#include <ll/api/service/Bedrock.h>
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

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"

#include "LOICollectionA/coro/TimerManager.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/frontend/AST.h"

#include "LOICollectionA/include/form/GUIManager.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/WalletPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    struct WalletPlugin::RedEnvelopeEntry {
        std::string id;
        std::string sender;

        std::unordered_map<std::string, int> receivers;
        std::unordered_map<std::string, std::string> names;

        int count;
        int capacity;
        int people;
    };

    struct WalletPlugin::operation {
        CommandSelector<Player> Target;
        int Score = 0;
    };

    struct WalletPlugin::Impl {
        std::shared_ptr<TimerManager> mTimerManager;

        ll::ConcurrentDenseMap<std::string, std::vector<RedEnvelopeEntry>> mRedEnvelopes;

        std::atomic<bool> mRegistered{ false };

        Config::C_Wallet options;
        
        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        std::filesystem::path mGuiPath;

        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr PlayerChatEventListener;

        Impl() : mTimerManager(std::make_shared<TimerManager>(ll::thread::ServerThreadExecutor::getDefault())) {}
    };

    WalletPlugin::WalletPlugin() : mImpl(std::make_unique<Impl>()) {};
    WalletPlugin::~WalletPlugin() = default;

    std::shared_ptr<WalletPlugin> WalletPlugin::getShared() {
        static auto instance = std::shared_ptr<WalletPlugin>(new WalletPlugin());
        return instance;
    }

    std::error_code WalletPlugin::makeErrorCode(WalletPluginErrorCode e) {
        static WalletPluginErrorCategory cat;
        return std::error_code{ static_cast<int>(e), cat };
    }

    std::shared_ptr<ll::io::Logger> WalletPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void WalletPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("wallet", tr({}, "commands.wallet.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("transfer").required("Target").required("Score").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player))
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                std::string mScoreboard = this->mImpl->options.TargetScoreboard;

                int mMoney = param.Score * static_cast<int>(results.size());
                if (ScoreboardUtils::getScore(player, mScoreboard) < mMoney || param.Score < 0)
                    return output.error(tr(origin.getLocaleCode(), "commands.wallet.error.score"));

                ScoreboardUtils::reduceScore(player, mScoreboard, mMoney);

                int mTargetMoney = static_cast<int>(param.Score * (1 - this->mImpl->options.ExchangeRate));
                for (Player*& target : results)
                    ScoreboardUtils::addScore(*target, mScoreboard, mTargetMoney);

                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.wallet.success.transfer")), param.Score, results.size());
            });
        command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            form::GUIManager::getInstance().open("wallet", "wallet.open", form::GUIManagerType::CustomForm, player)
                .or_else(modules::defaultErrorHandler<WalletPlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("wealth").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->wealth(player).or_else(modules::defaultErrorHandler<WalletPlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    ll::Expected<void> WalletPlugin::registeryUI() {
        return form::GUIManager::getInstance().load("wallet", (this->mImpl->mGuiPath / "wallet.lcui").string())
            .transform([this]() -> void {
                form::GUIManager::getInstance().registerValue("wallet.players.online", [](Player&) -> frontend::ArrayRef {
                    auto values = std::make_shared<frontend::ArrayValue>();

                    ll::service::getLevel()->forEachPlayer([&values](Player& target) -> bool {
                        if (!target.isSimulatedPlayer())
                            values->elements.emplace_back(target.getRealName());

                        return true;
                    });

                    return values;
                });

                form::GUIManager::getInstance().registerValue("wallet.players.offline", [this](Player&) -> ll::Expected<frontend::ArrayRef> {
                    return this->getPlayerInfo()
                        .transform([](const std::vector<std::pair<std::string, std::string>>& players) -> frontend::ArrayRef {
                            auto values = std::make_shared<frontend::ArrayValue>();

                            for (const auto& [uuid, name] : players)
                                values->elements.emplace_back(name);

                            return values;
                        });
                });

                form::GUIManager::getInstance().registerRequest("wallet.info", [this](frontend::ArrayRef, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    auto values = std::make_shared<frontend::ArrayValue>();
                    values->elements.emplace_back(ScoreboardUtils::getScore(player, this->getTargetScoreboard()));
                    values->elements.emplace_back(std::to_string(this->getExchangeRate() * 100) + "%%");

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("wallet.transfer.info", [this](frontend::ArrayRef args, Player&) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 2 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<int>(args->elements[1]))
                        return ll::makeStringError("wallet.transfer.info: must take a string and an int parameter");

                    auto type = std::get<std::string>(args->elements[0]);
                    int index = std::get<int>(args->elements[1]);

                    std::vector<std::pair<std::string, std::string>> players;
                    if (type == "online") {
                        ll::service::getLevel()->forEachPlayer([&players](Player& target) -> bool {
                            if (!target.isSimulatedPlayer())
                                players.emplace_back(target.getUuid().asString(), target.getRealName());

                            return true;
                        });
                    } else if (type == "offline") {
                        auto result = this->getPlayerInfo();
                        if (!result.has_value())
                            return ll::Unexpected(result.error());

                        players = result.value();
                    } else {
                        return ll::makeStringError("wallet.transfer.info: unknown transfer type");
                    }

                    if (index < 0 || index >= static_cast<int>(players.size()))
                        return ll::makeStringError("wallet.transfer.info: index out of range");

                    auto values = std::make_shared<frontend::ArrayValue>();
                    values->elements.emplace_back(players.at(static_cast<size_t>(index)).first);
                    values->elements.emplace_back(players.at(static_cast<size_t>(index)).second);

                    return values;
                });

                form::GUIManager::getInstance().registerRequest("wallet.transfer.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 4 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]) ||
                        !std::holds_alternative<std::string>(args->elements[3]))
                        return ll::makeStringError("wallet.transfer.submit: must take four string parameters");

                    auto uuid = std::get<std::string>(args->elements[0]);
                    auto name = std::get<std::string>(args->elements[1]);
                    int money = SystemUtils::toInt(std::get<std::string>(args->elements[3]), 0);

                    auto result = this->forTransfer(player, uuid, name, money);
                    auto values = std::make_shared<frontend::ArrayValue>();

                    if (!result.has_value()) {
                        modules::defaultErrorHandler<WalletPlugin>(result.error());

                        values->elements.emplace_back(false);
                        return values;
                    }

                    if (!result.value()) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "wallet.tips.transfer"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    values->elements.emplace_back(true);
                    return values;
                });

                form::GUIManager::getInstance().registerRequest("wallet.redenvelope.submit", [this](frontend::ArrayRef args, Player& player) -> ll::Expected<frontend::ArrayRef> {
                    if (args->elements.size() != 3 ||
                        !std::holds_alternative<std::string>(args->elements[0]) ||
                        !std::holds_alternative<std::string>(args->elements[1]) ||
                        !std::holds_alternative<std::string>(args->elements[2]))
                        return ll::makeStringError("wallet.redenvelope.submit: must take three string parameters");

                    auto values = std::make_shared<frontend::ArrayValue>();
                    auto key = std::get<std::string>(args->elements[2]);

                    if (key.empty()) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "generic.tips.noinput"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    int score = SystemUtils::toInt(std::get<std::string>(args->elements[0]), 0);
                    int count = SystemUtils::toInt(std::get<std::string>(args->elements[1]), 0);

                    if (score <= 0 || count <= 0 || ScoreboardUtils::getScore(player, this->getTargetScoreboard()) < score * count) {
                        return LanguagePlugin::getShared()->getLanguage(player)
                            .and_then([&player, values](const std::string& language) -> ll::Expected<frontend::ArrayRef> {
                                player.sendMessage(tr(language, "wallet.tips.redenvelope"));

                                values->elements.emplace_back(false);
                                return values;
                            });
                    }

                    auto result = this->redenvelope(player, key, score, count);
                    if (!result.has_value()) {
                        modules::defaultErrorHandler<WalletPlugin>(result.error());

                        values->elements.emplace_back(false);
                        return values;
                    }

                    values->elements.emplace_back(true);
                    return values;
                });

                form::GUIManager::getInstance().registerCallback("wallet.wealth", [this](frontend::ArrayRef, Player& player) -> ll::Expected<void> {
                    return this->wealth(player);
                });
            });
    }

    void WalletPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string uuid = event.self().getUuid().asString();

            this->mImpl->db->has("Wallet", uuid)
                .and_then([this, uuid, name = event.self().getRealName()](bool exists) -> ll::Expected<void> {
                    if (!exists) {
                        std::unordered_map<std::string, std::string> data = {
                            { "name", name },
                            { "score", "0" }
                        };

                        return this->mImpl->db->set("Wallet", uuid, data);
                    }

                    return {};
                })
                .or_else(modules::defaultErrorHandler<WalletPlugin>);

            this->mImpl->db->get("Wallet", uuid, "score", "0")
                .and_then([this, uuid, &event](const std::string& value) -> ll::Expected<void> { 
                    int score = SystemUtils::toInt(value, 0);
                    if (score > 0) {
                        ScoreboardUtils::addScore(event.self(), this->mImpl->options.TargetScoreboard, score);

                        return this->mImpl->db->set("Wallet", uuid, "score", "0");
                    }

                    return {};
                })
                .or_else(modules::defaultErrorHandler<WalletPlugin>);
        });
        this->mImpl->PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([this](ll::event::PlayerChatEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            this->tryGrabRedEnvelope(event.self(), event.message())
                .or_else([](ll::Error e) -> ll::Expected<void> {
                    if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(WalletPluginErrorCode::NotFound))
                        return {};

                    return ll::Unexpected(e);
                })
                .or_else(modules::defaultErrorHandler<WalletPlugin>);
        }, ll::event::EventPriority::High);
    }

    void WalletPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->PlayerChatEventListener);

        this->mImpl->mTimerManager->cancelAll();
    }

    ll::Expected<std::string> WalletPlugin::getPlayerInfo(const std::string& uuid) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(WalletPluginErrorCode::Invalid));

        return this->mImpl->db->get("Wallet", uuid, "name", "Unknown");
    }

    ll::Expected<std::vector<std::pair<std::string, std::string>>> WalletPlugin::getPlayerInfo() {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(WalletPluginErrorCode::Invalid));

        return this->mImpl->db->list("Wallet")
            .and_then([this](const std::vector<std::string>& ids) -> ll::Expected<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> {
                return this->mImpl->db->get("Wallet", ids);
            })
            .transform([](std::unordered_map<std::string, std::unordered_map<std::string, std::string>> mData) -> std::vector<std::pair<std::string, std::string>> {
                std::vector<std::pair<std::string, std::string>> result;

                result.reserve(mData.size());
                for (auto& [id, data] : mData)
                    result.emplace_back(id, data.at("name"));

                return result;
            });
    }

    ll::Expected<bool> WalletPlugin::forTransfer(Player& player, const std::string& target, const std::string& name, int score) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(WalletPluginErrorCode::Invalid));

        std::string mScoreboard = this->mImpl->options.TargetScoreboard;
        if (ScoreboardUtils::getScore(player, mScoreboard) < score || score <= 0)
            return false;

        ScoreboardUtils::reduceScore(player, mScoreboard, score);

        return this->transfer(target, static_cast<int>(score * (1 - this->mImpl->options.ExchangeRate)))
            .transform([this, name, score, playerName = player.getRealName()]() -> bool {
                this->getLogger()->info(fmt::runtime(tr({}, "wallet.log")), playerName, name, score);

                return true;
            });
    }

    ll::Expected<void> WalletPlugin::setExecutor(const ll::coro::Executor& executor) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(WalletPluginErrorCode::Invalid));

        this->mImpl->mTimerManager->setExecutor(executor);

        return {};
    }

    ll::Expected<void> WalletPlugin::tryGrabRedEnvelope(Player& player, const std::string& message) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(WalletPluginErrorCode::Invalid));

        auto it = this->mImpl->mRedEnvelopes.find(message);
        if (it == this->mImpl->mRedEnvelopes.end())
            return ll::makeErrorCodeError(makeErrorCode(WalletPluginErrorCode::NotFound));

        std::string mObjectUuid = player.getUuid().asString();
        std::string completedId;

        for (auto& mObject : it->second) {
            if (mObject.receivers.find(mObjectUuid) != mObject.receivers.end())
                continue;

            bool mLast = (mObject.people + 1) == mObject.count;

            int remainingPeople = mObject.count - mObject.people;
            int mTargetMoney = mLast ?
                mObject.capacity :
                ll::random_utils::rand(1, (mObject.capacity / remainingPeople) * 2);

            ScoreboardUtils::addScore(player, this->mImpl->options.TargetScoreboard, mTargetMoney);

            ll::service::getLevel()->forEachPlayer([mObject, mTargetMoney, &player](Player& target) -> bool {
                LanguagePlugin::getShared()->getLanguage(target)
                    .transform([mObject, mTargetMoney, &player, &target](const std::string& language) -> void {
                        std::string mMessage = LOICollectionAPI::APIUtils::getInstance().translate(
                            tr(language, "wallet.tips.redenvelope.receive"), player
                        );

                        TextPacket::createRawMessage(fmt::format(fmt::runtime(mMessage),
                            mObject.id, mTargetMoney, (mObject.people + 1), mObject.count
                        )).sendTo(target);
                    })
                    .or_else(modules::defaultErrorHandler<WalletPlugin>);

                return true;
            });

            mObject.people++;
            mObject.capacity -= mTargetMoney;
            mObject.receivers.insert({ mObjectUuid, mTargetMoney });
            mObject.names.insert({ mObjectUuid, player.getRealName() });

            if (mLast) {
                auto mKingIt = std::max_element(mObject.receivers.begin(), mObject.receivers.end(), [](const auto& a, const auto& b) {
                    return a.second < b.second;
                });

                ll::service::getLevel()->forEachPlayer([mObject, &mKingIt](Player& target) -> bool {
                    LanguagePlugin::getShared()->getLanguage(target)
                        .transform([mObject, &mKingIt, &target](const std::string& language) -> void {
                            TextPacket::createRawMessage(fmt::format(fmt::runtime(
                                tr(language, "wallet.tips.redenvelope.receive.over")),
                                mObject.id, mObject.names.at(mKingIt->first), mKingIt->second
                            )).sendTo(target);
                        })
                        .or_else(modules::defaultErrorHandler<WalletPlugin>);

                    return true;
                });

                completedId = mObject.id;
            }

            break;
        }

        if (!completedId.empty()) {
            auto& entries = it->second;
            entries.erase(std::remove_if(entries.begin(), entries.end(), [&completedId](const RedEnvelopeEntry& entry) -> bool {
                return entry.id == completedId;
            }), entries.end());
        }

        return {};
    }

    ll::Expected<void> WalletPlugin::transfer(const std::string& target, int score) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(WalletPluginErrorCode::Invalid));

        if (Player* mObject = ll::service::getLevel()->getPlayer(mce::UUID::fromString(target)); mObject) {
            ScoreboardUtils::addScore(*mObject, this->mImpl->options.TargetScoreboard, score);
            
            return {};
        }

        return this->mImpl->db->get("Wallet", target, "score", "0")
            .and_then([this, score, target](const std::string& value) -> ll::Expected<void> {
                int walletScore = SystemUtils::toInt(value);

                return this->mImpl->db->set("Wallet", target, "score", std::to_string(walletScore + score));
            });
    }

    ll::Expected<void> WalletPlugin::wealth(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(WalletPluginErrorCode::Invalid));

        int score = ScoreboardUtils::getScore(player, this->mImpl->options.TargetScoreboard);

        ll::service::getLevel()->forEachPlayer([score, &player](Player& target) -> bool {
            LanguagePlugin::getShared()->getLanguage(target)
                .transform([score, &player, &target](const std::string& language) -> void {
                    std::string mMessage = LOICollectionAPI::APIUtils::getInstance().translate(
                        tr(language, "wallet.showOff"), player
                    );

                    TextPacket::createRawMessage(
                        fmt::format(fmt::runtime(mMessage), score)
                    ).sendTo(target);
                })
                .or_else(modules::defaultErrorHandler<WalletPlugin>);

            return true;
        });

        return {};
    }

    ll::Expected<void> WalletPlugin::redenvelope(Player& player, const std::string& key, int score, int count) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(WalletPluginErrorCode::Invalid));

        ScoreboardUtils::reduceScore(player, this->getTargetScoreboard(), score * count);
        
        std::string mObjectId = SystemUtils::getCurrentTimestamp();

        this->mImpl->mRedEnvelopes[key].push_back({
            mObjectId,
            player.getUuid().asString(),
            {},
            {},
            count,
            score * count,
            0,
        });

        this->mImpl->mTimerManager->schedule(mObjectId, std::chrono::seconds(this->mImpl->options.RedEnvelopeTimeout), [this, key, mObjectId]() -> void {
            auto& mEntries = this->mImpl->mRedEnvelopes[key];
            auto mIt = std::find_if(mEntries.begin(), mEntries.end(), [mObjectId](RedEnvelopeEntry& entry) -> bool {
                return entry.id  == mObjectId;
            });

            if (mIt == mEntries.end())
                return;

            this->transfer(mIt->sender, mIt->capacity).or_else(modules::defaultErrorHandler<WalletPlugin>);

            ll::service::getLevel()->forEachPlayer([mObjectId](Player& target) -> bool {
                LanguagePlugin::getShared()->getLanguage(target)
                    .transform([mObjectId, &target](const std::string& language) -> void {
                        TextPacket::createRawMessage(
                            fmt::format(fmt::runtime(tr(language, "wallet.tips.redenvelope.timeout")), mObjectId)
                        ).sendTo(target);
                    })
                    .or_else(modules::defaultErrorHandler<WalletPlugin>);

                return true;
            });

            mEntries.erase(std::remove_if(mEntries.begin(), mEntries.end(), [mObjectId](RedEnvelopeEntry& entry) -> bool {
                return entry.id  == mObjectId;
            }), mEntries.end());
        });

        ll::service::getLevel()->forEachPlayer([&](Player& target) -> bool {
            LanguagePlugin::getShared()->getLanguage(target)
                .transform([&](const std::string& language) -> void {
                    std::string mMessage = LOICollectionAPI::APIUtils::getInstance().translate(
                        tr(language, "wallet.tips.redenvelope.content"), player
                    );

                    TextPacket::createRawMessage(fmt::format(fmt::runtime(mMessage), 
                        mObjectId, score, count, this->mImpl->options.RedEnvelopeTimeout, key
                    )).sendTo(target);
                })
                .or_else(modules::defaultErrorHandler<WalletPlugin>);

            return true;
        });

        return {};
    }

    bool WalletPlugin::isValid() {
        return this->getLogger() != nullptr && this->mImpl->db != nullptr;
    }

    std::string WalletPlugin::getTargetScoreboard() {
        return this->mImpl->options.TargetScoreboard;
    }

    double WalletPlugin::getExchangeRate() {
        return this->mImpl->options.ExchangeRate;
    }

    std::string WalletPlugin::getName() {
        return "WalletPlugin";
    }

    modules::ModulePriority WalletPlugin::getPriority() {
        return modules::ModulePriority::High;
    }

    ll::Expected<bool> WalletPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Wallet.ModuleEnabled)
            return false;

        this->mImpl->db = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Wallet;
        this->mImpl->mGuiPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("GuiPath")->data());

        return true;
    }

    ll::Expected<bool> WalletPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    ll::Expected<bool> WalletPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        return this->mImpl->db->create("Wallet", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("score");
        }).and_then([this]() -> ll::Expected<void> {
            return this->registeryUI();
        }).transform([this]() -> bool {
            this->registeryCommand();
            this->listenEvent();

            this->mImpl->mRegistered.store(true, std::memory_order_release);

            return true;
        });
    }

    ll::Expected<bool> WalletPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        for (auto& it : this->mImpl->mRedEnvelopes) {
            for (auto& mObject : it.second) {
                auto result = this->transfer(mObject.sender, mObject.capacity);
                if (!result.has_value())
                    return ll::Unexpected(result.error());
            }
        }

        this->mImpl->mRedEnvelopes.clear();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}
