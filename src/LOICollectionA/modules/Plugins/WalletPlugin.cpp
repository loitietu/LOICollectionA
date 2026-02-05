#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/coro/InterruptableSleep.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <ll/api/form/SimpleForm.h>
#include <ll/api/form/CustomForm.h>
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

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/Form/PaginatedForm.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/WalletPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
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
        std::unordered_map<std::string, std::vector<RedEnvelopeEntry>> mRedEnvelopeMap;
        std::unordered_map<std::string, std::shared_ptr<ll::coro::InterruptableSleep>> mRedEnvelopeTimers;

        std::atomic<bool> mRegistered{ false };

        Config::C_Wallet options;
        
        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr PlayerChatEventListener;
    };

    WalletPlugin::WalletPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    WalletPlugin::~WalletPlugin() = default;

    WalletPlugin& WalletPlugin::getInstance() {
        static WalletPlugin instance;
        return instance;
    }

    std::shared_ptr<ll::io::Logger> WalletPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void WalletPlugin::gui::content(Player& player, const std::string& target, TransferType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mTargetName = type == TransferType::online ? 
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(target))->getRealName() : this->mParent.getPlayerInfo(target);

        std::string mLabel = tr(mObjectLanguage, "wallet.gui.label") + "\n" + tr(mObjectLanguage, "wallet.gui.transfer.label2");

        ll::form::CustomForm form(tr(mObjectLanguage, "wallet.gui.title"));
        form.appendLabel(fmt::format(fmt::runtime(mLabel), 
            ScoreboardUtils::getScore(player, this->mParent.mImpl->options.TargetScoreboard),
            std::to_string(this->mParent.mImpl->options.ExchangeRate * 100) + "%%",
            mTargetName
        ));
        form.appendInput("Input", tr(mObjectLanguage, "wallet.gui.transfer.input"), tr(mObjectLanguage, "wallet.gui.transfer.input.placeholder"));
        form.sendTo(player, [this, mObjectLanguage, target, mTargetName, type](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->transfer(pl, type);

            std::string mScoreboard = this->mParent.mImpl->options.TargetScoreboard;

            int mMoney = SystemUtils::toInt(std::get<std::string>(dt->at("Input")), 0);
            if (ScoreboardUtils::getScore(pl, mScoreboard) < mMoney || mMoney <= 0) {
                pl.sendMessage(tr(mObjectLanguage, "wallet.tips.transfer"));
                return this->transfer(pl, type);
            }

            ScoreboardUtils::reduceScore(pl, mScoreboard, mMoney);

            this->mParent.transfer(target, static_cast<int>(mMoney * (1 - this->mParent.mImpl->options.ExchangeRate)));

            this->mParent.getLogger()->info(fmt::runtime(tr({}, "wallet.log")), pl.getRealName(), mTargetName, mMoney);
        });
    }

    void WalletPlugin::gui::transfer(Player& player, TransferType type) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::vector<std::string> mPlayerNames;
        std::vector<std::string> mPlayerUuids;

        switch (type) {
            case TransferType::online: {
                ll::service::getLevel()->forEachPlayer([&mPlayerNames, &mPlayerUuids](Player& mTarget) -> bool {
                    if (mTarget.isSimulatedPlayer())
                        return true;

                    mPlayerNames.push_back(mTarget.getRealName());
                    mPlayerUuids.push_back(mTarget.getUuid().asString());
                    return true;
                });

                break;
            }
            case TransferType::offline: {
                for (auto& mTarget : this->mParent.getPlayerInfo()) {
                    mPlayerNames.push_back(mTarget.second);
                    mPlayerUuids.push_back(mTarget.first);
                }

                break;
            }
        }

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "wallet.gui.title"),
            tr(mObjectLanguage, "wallet.gui.transfer.label1"),
            mPlayerNames
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, type, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            this->content(pl, mPlayerUuids.at(index), type);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void WalletPlugin::gui::redenvelope(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "wallet.gui.title"));
        form.appendLabel(fmt::format(fmt::runtime(tr(mObjectLanguage, "wallet.gui.label")),
            ScoreboardUtils::getScore(player, this->mParent.mImpl->options.TargetScoreboard), std::to_string(this->mParent.mImpl->options.ExchangeRate * 100) + "%%"
        ));
        form.appendInput("Input1", tr(mObjectLanguage, "wallet.gui.redenvelope.input1"), tr(mObjectLanguage, "wallet.gui.redenvelope.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "wallet.gui.redenvelope.input2"), tr(mObjectLanguage, "wallet.gui.redenvelope.input2.placeholder"));
        form.appendInput("Input3", tr(mObjectLanguage, "wallet.gui.redenvelope.input3"), tr(mObjectLanguage, "wallet.gui.redenvelope.input3.placeholder"));
        form.sendTo(player, [this, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->open(pl);

            std::string mObjectKey = std::get<std::string>(dt->at("Input3"));

            if (mObjectKey.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->open(pl);
            }

            int mScore = SystemUtils::toInt(std::get<std::string>(dt->at("Input1")), 0);
            int mCount = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);

            std::string mScoreboard = this->mParent.mImpl->options.TargetScoreboard;
            if (mScore <= 0 || mCount <= 0 || ScoreboardUtils::getScore(pl, mScoreboard) < mScore * mCount) {
                pl.sendMessage(tr(mObjectLanguage, "wallet.tips.redenvelope"));
                return this->open(pl);
            }

            ScoreboardUtils::reduceScore(pl, mScoreboard, mScore * mCount);

            this->mParent.redenvelope(pl, mObjectKey, mScore, mCount);
        });
    }

    void WalletPlugin::gui::wealth(Player& player) {
        std::string mMessage = LOICollectionAPI::APIUtils::getInstance().translate(tr(LanguagePlugin::getInstance().getLanguage(player), "wallet.showOff"), player);

        TextPacket::createRawMessage(
            fmt::format(fmt::runtime(mMessage), ScoreboardUtils::getScore(player, this->mParent.mImpl->options.TargetScoreboard))
        ).sendToClients();
    }

    void WalletPlugin::gui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mLabel = tr(mObjectLanguage, "wallet.gui.label");
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "wallet.gui.title"), 
            fmt::format(fmt::runtime(mLabel), 
                ScoreboardUtils::getScore(player, this->mParent.mImpl->options.TargetScoreboard),
                std::to_string(this->mParent.mImpl->options.ExchangeRate * 100) + "%%"
            )
        );
        form.appendButton(tr(mObjectLanguage, "wallet.gui.transfer"), "textures/ui/MCoin", "path", [this](Player& pl) -> void {
            this->transfer(pl, TransferType::online);
        });
        form.appendButton(tr(mObjectLanguage, "wallet.gui.offlineTransfer"), "textures/ui/icon_best3", "path", [this](Player& pl) -> void {
            this->transfer(pl, TransferType::offline);
        });
        form.appendButton(tr(mObjectLanguage, "wallet.gui.redenvelope"), "textures/ui/comment", "path", [this](Player& pl) -> void {
            this->redenvelope(pl);
        });
        form.appendButton(tr(mObjectLanguage, "wallet.gui.wealth"), "textures/ui/creative_icon", "path", [this](Player& pl) -> void {
            this->wealth(pl);
        });
        form.sendTo(player);
    }

    void WalletPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("wallet", tr({}, "commands.wallet.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("transfer").required("Target").required("Score").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                std::string mScoreboard = this->mImpl->options.TargetScoreboard;

                int mMoney = param.Score * static_cast<int>(results.size());
                if (ScoreboardUtils::getScore(player, mScoreboard) < mMoney || param.Score < 0)
                    return output.error(tr({}, "commands.wallet.error.score"));

                ScoreboardUtils::reduceScore(player, mScoreboard, mMoney);

                int mTargetMoney = static_cast<int>(param.Score * (1 - this->mImpl->options.ExchangeRate));
                for (Player*& target : results)
                    ScoreboardUtils::addScore(*target, mScoreboard, mTargetMoney);

                output.success(fmt::runtime(tr({}, "commands.wallet.success.transfer")), param.Score, results.size());
            });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("wealth").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->wealth(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
    }

    void WalletPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();
            
            if (!this->mImpl->db->has("Wallet", mObject)) {
                std::unordered_map<std::string, std::string> mData = {
                    { "name", event.self().getRealName() },
                    { "score", "0" }
                };

                this->mImpl->db->set("Wallet", mObject, mData);
            }

            if (int mScore = SystemUtils::toInt(this->mImpl->db->get("Wallet", mObject, "score", "0"), 0); mScore > 0) {
                ScoreboardUtils::addScore(event.self(), this->mImpl->options.TargetScoreboard, mScore);

                this->mImpl->db->set("Wallet", mObject, "score", "0");
            }
        });
        this->mImpl->PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([this](ll::event::PlayerChatEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            auto it = this->mImpl->mRedEnvelopeMap.find(event.message());
            if (it == this->mImpl->mRedEnvelopeMap.end())
                return;

            std::string mObjectUuid = event.self().getUuid().asString();
            for (auto& mObject : it->second) {
                if (mObject.receivers.find(mObjectUuid) != mObject.receivers.end())
                    continue;

                bool mLast = (mObject.people + 1) == mObject.count;

                int mTargetMoney = mLast ?
                    mObject.capacity :
                    ll::random_utils::rand(0, 
                        (mObject.capacity / (mObject.count - mObject.people + 2)) * 2
                    );

                ScoreboardUtils::addScore(event.self(), this->mImpl->options.TargetScoreboard, mTargetMoney);

                std::string mMessage = LOICollectionAPI::APIUtils::getInstance().translate(tr(LanguagePlugin::getInstance().getLanguage(event.self()), "wallet.tips.redenvelope.receive"), event.self());

                TextPacket::createRawMessage(fmt::format(fmt::runtime(mMessage),
                    mObject.id, mTargetMoney, (mObject.people + 1), mObject.count
                )).sendToClients();

                mObject.people++;
                mObject.capacity -= mTargetMoney;
                mObject.receivers.insert({ mObjectUuid, mTargetMoney });
                mObject.names.insert({ mObjectUuid, event.self().getRealName() });

                if (mLast) {
                    auto mKingIt = std::max_element(mObject.receivers.begin(), mObject.receivers.end(), [](const auto& a, const auto& b) -> bool {
                        return a.second < b.second;
                    });

                    std::string mMessageOver = LOICollectionAPI::APIUtils::getInstance().translate(tr(LanguagePlugin::getInstance().getLanguage(event.self()), "wallet.tips.redenvelope.receive.over"), event.self());
                    
                    TextPacket::createRawMessage(fmt::format(fmt::runtime(mMessageOver),
                        mObject.id, mObject.names.at(mKingIt->first), mKingIt->second
                    )).sendToClients();

                    it->second.erase(std::remove_if(it->second.begin(), it->second.end(), [mObjectId = mObject.id](auto& mObject) -> bool {
                        return mObject.id == mObjectId;
                    }), it->second.end());
                    
                    continue;
                }
            }
        });
    }

    void WalletPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->PlayerChatEventListener);

        for (auto& it : this->mImpl->mRedEnvelopeTimers)
            it.second->interrupt();
    }

    std::string WalletPlugin::getPlayerInfo(const std::string& uuid) {
        if (!this->isValid())
            return "";

        return this->mImpl->db->get("Wallet", uuid, "name", "Unknown");
    }

    std::vector<std::pair<std::string, std::string>> WalletPlugin::getPlayerInfo() {
        if (!this->isValid())
            return {};

        std::vector<std::pair<std::string, std::string>> mResult;
        for (auto& [id, data] : this->mImpl->db->get("Wallet", this->mImpl->db->list("Wallet")))
            mResult.emplace_back(id, data.at("name"));

        return mResult;
    }

    void WalletPlugin::transfer(const std::string& target, int score) {
        if (!this->isValid())
            return; 

        if (Player* mObject = ll::service::getLevel()->getPlayer(mce::UUID::fromString(target)); mObject) {
            ScoreboardUtils::addScore(*mObject, this->mImpl->options.TargetScoreboard, score);
            
            return;
        }
        
        int mWalletScore = SystemUtils::toInt(this->mImpl->db->get("Wallet", target, "score", "0"));
        this->mImpl->db->set("Wallet", target, "score", std::to_string(mWalletScore + score));
    }

    void WalletPlugin::redenvelope(Player& player, const std::string& key, int score, int count) {
        if (!this->isValid())
            return; 

        std::string mObjectId = SystemUtils::getCurrentTimestamp();

        this->mImpl->mRedEnvelopeMap[key].push_back({
            mObjectId,
            player.getUuid().asString(),
            {},
            {},
            count,
            score * count,
            0,
        });

        ll::coro::keepThis([this, mObjectId, key]() -> ll::coro::CoroTask<> {
            this->mImpl->mRedEnvelopeTimers[key] = std::make_shared<ll::coro::InterruptableSleep>();

            co_await this->mImpl->mRedEnvelopeTimers[key]->sleepFor(std::chrono::seconds(this->mImpl->options.RedEnvelopeTimeout));
            
            std::vector<RedEnvelopeEntry>& mEntries = this->mImpl->mRedEnvelopeMap[key];
            auto mIt = std::find_if(mEntries.begin(), mEntries.end(), [mObjectId](RedEnvelopeEntry& entry) -> bool {
                return entry.id  == mObjectId;
            });

            if (mIt == mEntries.end())
                co_return;

            this->transfer(mIt->sender, mIt->capacity);

            TextPacket::createRawMessage(
                fmt::format(fmt::runtime(tr({}, "wallet.tips.redenvelope.timeout")), mObjectId)
            ).sendToClients();

            mEntries.erase(std::remove_if(mEntries.begin(), mEntries.end(), [mObjectId](RedEnvelopeEntry& entry) -> bool {
                return entry.id  == mObjectId;
            }), mEntries.end());

            this->mImpl->mRedEnvelopeTimers.erase(key);
        }).launch(ll::thread::ServerThreadExecutor::getDefault());

        std::string mMessage = LOICollectionAPI::APIUtils::getInstance().translate(tr(LanguagePlugin::getInstance().getLanguage(player), "wallet.tips.redenvelope.content"), player);

        TextPacket::createRawMessage(fmt::format(fmt::runtime(mMessage), 
            mObjectId, score, count, this->mImpl->options.RedEnvelopeTimeout, key
        )).sendToClients();
    }

    bool WalletPlugin::isValid() {
        return this->getLogger() != nullptr && this->mImpl->db != nullptr;
    }

    bool WalletPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Wallet.ModuleEnabled)
            return false;

        this->mImpl->db = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Wallet;

        return true;
    }

    bool WalletPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool WalletPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db->create("Wallet", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("score");
        });
        
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool WalletPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        for (auto& it : this->mImpl->mRedEnvelopeMap) {
            for (auto& mObject : it.second)
                this->transfer(mObject.sender, mObject.capacity);
        }

        this->mImpl->mRedEnvelopeMap.clear();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("WalletPlugin", LOICollection::Plugins::WalletPlugin, LOICollection::Plugins::WalletPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
