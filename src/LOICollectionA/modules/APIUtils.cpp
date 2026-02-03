#include <any>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <functional>
#include <unordered_map>

#include <magic_enum/magic_enum.hpp>

#include <ll/api/Versions.h>
#include <ll/api/memory/Memory.h>
#include <ll/api/service/Bedrock.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <mc/util/ProfilerLite.h>
#include <mc/deps/core/math/Vec2.h>
#include <mc/deps/core/math/Vec3.h>

#include <mc/world/level/Level.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/actor/BuiltInActorComponents.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/attribute/AttributeInstanceConstRef.h>
#include <mc/world/attribute/AttributeInstance.h>
#include <mc/entity/components/ActorRotationComponent.h>
#include <mc/network/ServerNetworkHandler.h>

#include "LOICollectionA/base/Cache.h"

#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
#include "LOICollectionA/frontend/Callback.h"
#include "LOICollectionA/frontend/Evaluator.h"

#include "LOICollectionA/include/Plugins/PvpPlugin.h"
#include "LOICollectionA/include/Plugins/ChatPlugin.h"
#include "LOICollectionA/include/Plugins/MutePlugin.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/Plugins/StatisticsPlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/include/APIUtils.h"

namespace LOICollection::LOICollectionAPI {
    struct APIUtils::Impl {
        LRUKCache<std::string, frontend::ASTNode> mAstCache;

        std::shared_ptr<ll::io::Logger> logger;

        std::unordered_map<std::string, std::function<std::string()>> mVariableCommonMap;
        std::unordered_map<std::string, std::function<std::string(Player&)>> mVariableMap;
        std::unordered_map<std::string, std::function<std::string(const std::vector<std::string>&)>> mVariableCommonMapParameter;
        std::unordered_map<std::string, std::function<std::string(Player&, const std::vector<std::string>&)>> mVariableMapParameter;

        Impl() : mAstCache(100, 200, 5) {}
    };

    APIUtils::APIUtils() : mImpl(std::make_unique<Impl>()) {
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");

        this->registerVariable("version_mc", []() -> std::string {
            return ll::getGameVersion().to_string();
        });
        this->registerVariable("version_ll", []() -> std::string {
            return ll::getLoaderVersion().to_string();
        });
        this->registerVariable("version_protocol", []() -> std::string {
            return std::to_string(ll::getNetworkProtocolVersion()); 
        });
        this->registerVariable("player", [](Player& player) -> std::string {
            return std::string{player.mName};
        });
        this->registerVariable("player_title", [](Player& player) -> std::string {
            return Plugins::ChatPlugin::getInstance().getTitle(player);
        });
        this->registerVariable("player_title_time", [](Player& player) -> std::string {
            return SystemUtils::toFormatTime(
                Plugins::ChatPlugin::getInstance().getTitleTime(player, Plugins::ChatPlugin::getInstance().getTitle(player)), "None"
            );
        });
        this->registerVariable("player_pvp", [](Player& player) -> std::string {
            return Plugins::PvpPlugin::getInstance().isEnable(player) ? "true" : "false";
        });
        this->registerVariable("player_mute", [](Player& player) -> std::string {
            return Plugins::MutePlugin::getInstance().isMute(player) ? "true" : "false";
        });
        this->registerVariable("player_language", [](Player& player) -> std::string { 
            return Plugins::LanguagePlugin::getInstance().getLanguage(player);
        });
        this->registerVariable("player_language_name", [](Player& player) -> std::string {
            return I18nUtils::getInstance()->get(Plugins::LanguagePlugin::getInstance().getLanguage(player), "name");
        });
        this->registerVariable("player_statistcs_onlinetime", [](Player& player) -> std::string {
            return SystemUtils::toFormatSecond(
                std::to_string(Plugins::StatisticsPlugin::getInstance().getStatistic(player, Plugins::StatisticType::onlinetime)), "None"
            );
        });
        this->registerVariable("player_statistcs_kills", [](Player& player) -> std::string {
            return std::to_string(Plugins::StatisticsPlugin::getInstance().getStatistic(player, Plugins::StatisticType::kills));
        });
        this->registerVariable("player_statistcs_deaths", [](Player& player) -> std::string {
            return std::to_string(Plugins::StatisticsPlugin::getInstance().getStatistic(player, Plugins::StatisticType::deaths));
        });
        this->registerVariable("player_statistcs_place", [](Player& player) -> std::string {
            return std::to_string(Plugins::StatisticsPlugin::getInstance().getStatistic(player, Plugins::StatisticType::place));
        });
        this->registerVariable("player_statistcs_destroy", [](Player& player) -> std::string {
            return std::to_string(Plugins::StatisticsPlugin::getInstance().getStatistic(player, Plugins::StatisticType::destroy));
        });
        this->registerVariable("player_statistcs_respawn", [](Player& player) -> std::string {
            return std::to_string(Plugins::StatisticsPlugin::getInstance().getStatistic(player, Plugins::StatisticType::respawn));
        });
        this->registerVariable("player_statistcs_join", [](Player& player) -> std::string {
            return std::to_string(Plugins::StatisticsPlugin::getInstance().getStatistic(player, Plugins::StatisticType::join));
        });
        this->registerVariable("player_gamemode", [](Player& player) -> std::string {
            return std::string(magic_enum::enum_name(player.getPlayerGameType()));
        });
        this->registerVariable("player_pos", [](Player& player) -> std::string {
            return player.getPosition().toString();
        });
        this->registerVariable("player_pos_x", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getPosition().x));
        });
        this->registerVariable("player_pos_y", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getPosition().y));
        });
        this->registerVariable("player_pos_z", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getPosition().z));
        });
        this->registerVariable("player_pos_respawn", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? player.getExpectedSpawnPosition().toString() : "None";
        });
        this->registerVariable("player_pos_respawn_x", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? std::to_string(static_cast<int>(player.getExpectedSpawnPosition().x)) : "None";
        });
        this->registerVariable("player_pos_respawn_y", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? std::to_string(static_cast<int>(player.getExpectedSpawnPosition().y)) : "None";
        });
        this->registerVariable("player_pos_respawn_z", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? std::to_string(static_cast<int>(player.getExpectedSpawnPosition().z)) : "None";
        });
        this->registerVariable("player_pos_block", [](Player& player) -> std::string {
            return player.getEyePos().toString();
        });
        this->registerVariable("player_pos_lastdeath", [](Player& player) -> std::string {
            return player.getLastDeathPos() ? player.getLastDeathPos()->toString() : "None";
        });
        this->registerVariable("player_realname", [](Player& player) -> std::string {
            return player.getRealName();
        });
        this->registerVariable("player_xuid", [](Player& player) -> std::string {
            return player.getXuid();
        });
        this->registerVariable("player_uuid", [](Player& player) -> std::string {
            return player.getUuid().asString();
        });
        this->registerVariable("player_is_op", [](Player& player) -> std::string {
            return player.isOperator() ? "true" : "false";
        });
        this->registerVariable("player_can_fly", [](Player& player) -> std::string {
            return player.canFly() ? "true" : "false";
        });
        this->registerVariable("player_health", [](Player& player) -> std::string {
            return std::to_string(player.getHealth());
        });
        this->registerVariable("player_max_health", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getMaxHealth()));
        });
        this->registerVariable("player_hunger", [](Player& player) -> std::string {
            if (auto attribute = player.getAttribute(Player::HUNGER()).mPtr; attribute)
                return std::to_string(static_cast<int>(attribute->mCurrentValue));

            return "None";
        });
        this->registerVariable("player_max_hunger", [](Player& player) -> std::string {
            if (auto attribute = player.getAttribute(Player::HUNGER()).mPtr; attribute)
                return std::to_string(static_cast<int>(attribute->mCurrentMaxValue));

            return "None";
        });
        this->registerVariable("player_saturation", [](Player& player) -> std::string {
            if (auto attribute = player.getAttribute(Player::SATURATION()).mPtr; attribute)
                return std::to_string(static_cast<int>(attribute->mCurrentValue));

            return "None";
        });
        this->registerVariable("player_max_saturation", [](Player& player) -> std::string {
            if (auto attribute = player.getAttribute(Player::SATURATION()).mPtr; attribute)
                return std::to_string(static_cast<int>(attribute->mCurrentMaxValue));

            return "None";
        });
        this->registerVariable("player_speed", [](Player& player) -> std::string {
            return std::to_string(player.getSpeed());
        });
        this->registerVariable("player_direction", [](Player& player) -> std::string {
            return player.mBuiltInComponents->mActorRotationComponent->mRotationDegree->toString();
        });
        this->registerVariable("player_dimension", [](Player& player) -> std::string {
            return std::to_string(player.getDimensionId());
        });
        this->registerVariable("player_os", [](Player& player) -> std::string {
            return magic_enum::enum_name(player.mBuildPlatform).data();
        });
        this->registerVariable("player_ip", [](Player& player) -> std::string {
            return player.getIPAndPort();
        });
        this->registerVariable("player_exp_xp", [](Player& player) -> std::string {
            if (auto attribute = player.getAttribute(Player::EXPERIENCE()).mPtr; attribute)
                return std::to_string(static_cast<int>(attribute->mCurrentValue));

            return "None";
        });
        this->registerVariable("player_exp_level", [](Player& player) -> std::string {
            if (auto attribute = player.getAttribute(Player::LEVEL()).mPtr; attribute)
                return std::to_string(static_cast<int>(attribute->mCurrentValue));

            return "None";
        });
        this->registerVariable("player_exp_level_next", [](Player& player) -> std::string {
            return std::to_string(player.getXpNeededForNextLevel());
        });
        this->registerVariable("player_handitem", [](Player& player) -> std::string {
            return player.getCarriedItem().getName();
        });
        this->registerVariable("player_offhand", [](Player& player) -> std::string {
            return player.getOffhandSlot().getName();
        });
        this->registerVariable("player_ms", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mAveragePing);
        });
        this->registerVariable("player_ms_avg", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mCurrentPing);
        });
        this->registerVariable("player_packet", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mAveragePacketLoss);
        });
        this->registerVariable("player_packet_avg", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mCurrentPacketLoss);
        });
        this->registerVariable("server_tps", []() -> std::string {
            auto mMspt = static_cast<double>(ProfilerLite::gProfilerLiteInstance().mDebugServerTickTime->count()) / 1e6;
            return std::to_string(mMspt <= 50.0 ? 20.0 : static_cast<double>(1000.0 / mMspt));
        });
        this->registerVariable("server_mspt", []() -> std::string { 
            return std::to_string(static_cast<double>(ProfilerLite::gProfilerLiteInstance().mDebugServerTickTime->count()) / 1e6);
        });
        this->registerVariable("server_time", []() -> std::string {
            return SystemUtils::getNowTime();
        });
        this->registerVariable("server_player_max", []() -> std::string {
            return std::to_string(ll::service::getServerNetworkHandler()->mMaxNumPlayers);
        });
        this->registerVariable("server_player_online", []() -> std::string {
            return std::to_string(ll::service::getLevel()->getActivePlayerCount());
        });
        this->registerVariable("server_entity", []() -> std::string {
            return std::to_string(ll::service::getLevel()->getRuntimeActorList().size());
        });
        this->registerVariable("score", [](Player& player, const std::vector<std::string>& args) -> std::string {
            std::string name = args.empty() ? "None" : args[0];

            return std::to_string(ScoreboardUtils::getScore(player, name));
        });
        this->registerVariable("tr", [](Player& player, const std::vector<std::string>& args) -> std::string {
            std::string name = args.empty() ? "None" : args[0];

            return I18nUtils::getInstance()->get(Plugins::LanguagePlugin::getInstance().getLanguage(player), name);
        });
        this->registerVariable("entity", [](const std::vector<std::string>& args) -> std::string {
            std::string name = args.empty() ? "None" : args[0];

            std::vector<Actor*> mRuntimeActorList = ll::service::getLevel()->getRuntimeActorList();
            int count = static_cast<int>(std::count_if(mRuntimeActorList.begin(), mRuntimeActorList.end(), [name](Actor* actor) -> bool {
                return actor->getTypeName() == name;
            }));
            
            return std::to_string(count);
        });
    }
    APIUtils::~APIUtils() = default;

    APIUtils& APIUtils::getInstance() {
        static APIUtils instance;
        return instance;
    }

    void APIUtils::registerVariable(const std::string& name, std::function<std::string()> callback) {
        this->mImpl->mVariableCommonMap.emplace(name, std::move(callback));

        frontend::MacroCall::getInstance().registerMacro(name, [this, name](const frontend::CallbackTypeArgs&) -> std::string {
            return this->getValueForVariable(name);
        });
    }

    void APIUtils::registerVariable(const std::string& name, std::function<std::string(Player&)> callback) {
        this->mImpl->mVariableMap.emplace(name, std::move(callback));

        frontend::MacroCall::getInstance().registerMacro(name, [this, name](const frontend::CallbackTypeArgs&, const frontend::CallbackTypePlaces& placeholders) -> std::string {
            return this->getValueForVariable(name, std::any_cast<std::reference_wrapper<Player>>(placeholders.at(0)));
        });
    }

    void APIUtils::registerVariable(const std::string& name, std::function<std::string(const std::vector<std::string>&)> callback) {
        this->mImpl->mVariableCommonMapParameter.emplace(name, std::move(callback));

        frontend::MacroCall::getInstance().registerMacro(name, [this, name](const frontend::CallbackTypeArgs& args) -> std::string {
            return this->getValueForVariable(name, args);
        });
    }

    void APIUtils::registerVariable(const std::string& name, std::function<std::string(Player&, const std::vector<std::string>&)> callback) {
        this->mImpl->mVariableMapParameter.emplace(name, std::move(callback));

        frontend::MacroCall::getInstance().registerMacro(name, [this, name](const frontend::CallbackTypeArgs& args, const frontend::CallbackTypePlaces& placeholders) -> std::string {
            return this->getValueForVariable(name, std::any_cast<std::reference_wrapper<Player>>(placeholders.at(0)), args);
        });
    }

    std::string APIUtils::getValueForVariable(const std::string& name) try {
        auto it = this->mImpl->mVariableCommonMap.find(name);
        return it != this->mImpl->mVariableCommonMap.end() ? it->second() : "None";
    } catch (...) {
        return "None";
    }

    std::string APIUtils::getValueForVariable(const std::string& name, Player& player) try {
        auto it = this->mImpl->mVariableMap.find(name);
        return it != this->mImpl->mVariableMap.end() ? it->second(player) : this->getValueForVariable(name);
    } catch (...) {
        return "None";
    }

    std::string APIUtils::getValueForVariable(const std::string& name, const std::vector<std::string>& parameter) try {
        auto it = this->mImpl->mVariableCommonMapParameter.find(name);
        return it != this->mImpl->mVariableCommonMapParameter.end() ? it->second(parameter) : "None";
    } catch (...) {
        return "None";
    }

    std::string APIUtils::getValueForVariable(const std::string& name, Player& player, const std::vector<std::string>& parameter) try {
        auto it = this->mImpl->mVariableMapParameter.find(name);
        return it != this->mImpl->mVariableMapParameter.end() ? it->second(player, parameter) : this->getValueForVariable(name, parameter);
    } catch (...) {
        return "None";
    }

    std::string APIUtils::translate(const std::string& str, Player& player) try {
        frontend::Evaluator mEvaluator;

        if (this->mImpl->mAstCache.contains(str)) {
            auto mCached = this->mImpl->mAstCache.get(str);

            if (mCached.has_value())
                return mEvaluator.evaluate(*mCached.value(), { std::ref(player) });
        }

        frontend::Lexer mLexer(str);
        frontend::Parser mParser(mLexer);

        std::unique_ptr<frontend::ASTNode> mAst = mParser.parse();

        std::string result = mEvaluator.evaluate(*mAst, { std::ref(player) });

        std::shared_ptr<frontend::TemplateNode> mTemplate = std::make_shared<frontend::TemplateNode>();
        mTemplate->parts = std::move(static_cast<frontend::TemplateNode*>(mAst.release())->parts);

        this->mImpl->mAstCache.put(str, mTemplate);
        return result;
    } catch (const std::exception& e) {
        this->mImpl->logger->error("APIUtils: {}", e.what());
        
        return "None";
    }

    std::string APIUtils::translate(const std::string& str) try {
        frontend::Evaluator mEvaluator;

        if (this->mImpl->mAstCache.contains(str)) {
            auto mCached = this->mImpl->mAstCache.get(str);

            if (mCached.has_value())
                return mEvaluator.evaluate(*mCached.value());
        }

        frontend::Lexer mLexer(str);
        frontend::Parser mParser(mLexer);

        std::unique_ptr<frontend::ASTNode> mAst = mParser.parse();

        std::string result = mEvaluator.evaluate(*mAst);

        std::shared_ptr<frontend::TemplateNode> mTemplate = std::make_shared<frontend::TemplateNode>();
        mTemplate->parts = std::move(static_cast<frontend::TemplateNode*>(mAst.release())->parts);

        this->mImpl->mAstCache.put(str, mTemplate);
        return result;
    } catch (const std::exception& e) {
        this->mImpl->logger->error("APIUtils: {}", e.what());

        return "None";
    }
}
