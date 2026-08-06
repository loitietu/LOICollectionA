#include <any>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <functional>
#include <unordered_map>

#include <magic_enum/magic_enum.hpp>

#include <ll/api/Expected.h>
#include <ll/api/Versions.h>
#include <ll/api/memory/Memory.h>
#include <ll/api/service/Bedrock.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <mc/deps/core/math/Vec2.h>
#include <mc/deps/core/math/Vec3.h>

#include <mc/profile/ProfilerLite.h>

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
#include "LOICollectionA/frontend/SemanticAnalyzer.h"
#include "LOICollectionA/frontend/ir/Compiler.h"
#include "LOICollectionA/frontend/ir/Optimizer.h"
#include "LOICollectionA/frontend/ir/VM.h"

#include "LOICollectionA/include/server/Plugins/PvpPlugin.h"
#include "LOICollectionA/include/server/Plugins/ChatPlugin.h"
#include "LOICollectionA/include/server/Plugins/MutePlugin.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Plugins/StatisticsPlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/include/server/APIUtils.h"

namespace LOICollection::server::LOICollectionAPI {
    struct APIUtils::Impl {
        LRUKCache<std::string, frontend::ir::BytecodeChunk> mAstCache;

        std::shared_ptr<ll::io::Logger> logger;

        std::unordered_map<std::string, std::function<ll::Expected<frontend::TypedValue>()>> mVariableCommonMap;
        std::unordered_map<std::string, std::function<ll::Expected<frontend::TypedValue>(Player&)>> mVariableMap;
        std::unordered_map<std::string, std::function<ll::Expected<frontend::TypedValue>(const frontend::CallbackTypeValues&)>> mVariableCommonMapParameter;
        std::unordered_map<std::string, std::function<ll::Expected<frontend::TypedValue>(Player&, const frontend::CallbackTypeValues&)>> mVariableMapParameter;

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
        this->registerVariable("version_protocol", []() -> int {
            return ll::getNetworkProtocolVersion(); 
        });
        this->registerVariable("player", [](Player& player) -> std::string {
            return std::string{player.mName};
        });
        this->registerVariable("player_title", [](Player& player) -> ll::Expected<std::string> {
            return Plugins::ChatPlugin::getShared()->getTitle(player);
        });
        this->registerVariable("player_title_time", [](Player& player) -> ll::Expected<std::string> {
            return Plugins::ChatPlugin::getShared()->getTitle(player)
                .and_then([&player](const std::string& title) -> ll::Expected<std::string> {
                    return Plugins::ChatPlugin::getShared()->getTitleTime(player, title);
                })
                .transform([](const std::string& time) -> std::string {
                    return SystemUtils::toFormatTime(time);
                });
        });
        this->registerVariable("player_pvp", [](Player& player) -> ll::Expected<bool> {
            return Plugins::PvpPlugin::getShared()->isEnable(player);
        });
        this->registerVariable("player_mute", [](Player& player) -> ll::Expected<bool> {
            return Plugins::MutePlugin::getShared()->isMute(player);
        });
        this->registerVariable("player_language", [](Player& player) -> ll::Expected<std::string> { 
            return Plugins::LanguagePlugin::getShared()->getLanguage(player);
        });
        this->registerVariable("player_language_name", [](Player& player) -> ll::Expected<std::string> {
            return Plugins::LanguagePlugin::getShared()->getLanguage(player)
                .transform([](const std::string& language) -> std::string {
                    return I18nUtils::getInstance()->get(language, "name");
                });
        });
        this->registerVariable("player_statistcs_onlinetime", [](Player& player) -> ll::Expected<std::string> {
            return Plugins::StatisticsPlugin::getShared()->getStatistic(player, Plugins::StatisticType::onlinetime)
                .transform([](int time) -> std::string {
                    return SystemUtils::toFormatSecond(std::to_string(time), "None");
                });
        });
        this->registerVariable("player_statistcs_kills", [](Player& player) -> ll::Expected<int> {
            return Plugins::StatisticsPlugin::getShared()->getStatistic(player, Plugins::StatisticType::kills);
        });
        this->registerVariable("player_statistcs_deaths", [](Player& player) -> ll::Expected<int> {
            return Plugins::StatisticsPlugin::getShared()->getStatistic(player, Plugins::StatisticType::deaths);
        });
        this->registerVariable("player_statistcs_place", [](Player& player) -> ll::Expected<int> {
            return Plugins::StatisticsPlugin::getShared()->getStatistic(player, Plugins::StatisticType::place);
        });
        this->registerVariable("player_statistcs_destroy", [](Player& player) -> ll::Expected<int> {
            return Plugins::StatisticsPlugin::getShared()->getStatistic(player, Plugins::StatisticType::destroy);
        });
        this->registerVariable("player_statistcs_respawn", [](Player& player) -> ll::Expected<int> {
            return Plugins::StatisticsPlugin::getShared()->getStatistic(player, Plugins::StatisticType::respawn);
        });
        this->registerVariable("player_statistcs_join", [](Player& player) -> ll::Expected<int> {
            return Plugins::StatisticsPlugin::getShared()->getStatistic(player, Plugins::StatisticType::join);
        });
        this->registerVariable("player_gamemode", [](Player& player) -> std::string {
            return std::string(magic_enum::enum_name(player.getPlayerGameType()));
        });
        this->registerVariable("player_pos", [](Player& player) -> std::string {
            return player.getPosition().toString();
        });
        this->registerVariable("player_pos_x", [](Player& player) -> int {
            return static_cast<int>(player.getPosition().x);
        });
        this->registerVariable("player_pos_y", [](Player& player) -> int {
            return static_cast<int>(player.getPosition().y);
        });
        this->registerVariable("player_pos_z", [](Player& player) -> int {
            return static_cast<int>(player.getPosition().z);
        });
        this->registerVariable("player_pos_respawn", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? player.getExpectedSpawnPosition().toString() : "None";
        });
        this->registerVariable("player_pos_respawn_x", [](Player& player) -> frontend::TypedValue {
            if (!player.hasRespawnPosition()) return "None";

            return static_cast<int>(player.getExpectedSpawnPosition().x);
        });
        this->registerVariable("player_pos_respawn_y", [](Player& player) -> frontend::TypedValue {
            if (!player.hasRespawnPosition()) return "None";

            return static_cast<int>(player.getExpectedSpawnPosition().y);
        });
        this->registerVariable("player_pos_respawn_z", [](Player& player) -> frontend::TypedValue {
            if (!player.hasRespawnPosition()) return "None";

            return static_cast<int>(player.getExpectedSpawnPosition().z);
        });
        this->registerVariable("player_pos_block", [](Player& player) -> std::string {
            return player.getFeetBlockPos().toString();
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
        this->registerVariable("player_is_op", [](Player& player) -> bool {
            return player.isOperator();
        });
        this->registerVariable("player_can_fly", [](Player& player) -> bool {
            return player.canFly();
        });
        this->registerVariable("player_health", [](Player& player) -> int {
            return player.getHealth();
        });
        this->registerVariable("player_max_health", [](Player& player) -> int {
            return static_cast<int>(player.getMaxHealth());
        });
        this->registerVariable("player_hunger", [](Player& player) -> frontend::TypedValue {
            if (auto attribute = player.getAttribute(Player::HUNGER()).mPtr; attribute)
                return static_cast<int>(attribute->mCurrentValue);

            return "None";
        });
        this->registerVariable("player_max_hunger", [](Player& player) -> frontend::TypedValue {
            if (auto attribute = player.getAttribute(Player::HUNGER()).mPtr; attribute)
                return static_cast<int>(attribute->mCurrentMaxValue);

            return "None";
        });
        this->registerVariable("player_saturation", [](Player& player) -> frontend::TypedValue {
            if (auto attribute = player.getAttribute(Player::SATURATION()).mPtr; attribute)
                return static_cast<int>(attribute->mCurrentValue);

            return "None";
        });
        this->registerVariable("player_max_saturation", [](Player& player) -> frontend::TypedValue{
            if (auto attribute = player.getAttribute(Player::SATURATION()).mPtr; attribute)
                return static_cast<int>(attribute->mCurrentMaxValue);

            return "None";
        });
        this->registerVariable("player_speed", [](Player& player) -> float {
            return player.getSpeed();
        });
        this->registerVariable("player_direction", [](Player& player) -> std::string {
            return player.mBuiltInComponents->mActorRotationComponent->mRot->toString();
        });
        this->registerVariable("player_dimension", [](Player& player) -> int {
            return player.getDimensionId();
        });
        this->registerVariable("player_os", [](Player& player) -> std::string {
            return magic_enum::enum_name(player.mBuildPlatform).data();
        });
        this->registerVariable("player_ip", [](Player& player) -> std::string {
            return player.getIPAndPort();
        });
        this->registerVariable("player_exp_xp", [](Player& player) -> frontend::TypedValue {
            if (auto attribute = player.getAttribute(Player::EXPERIENCE()).mPtr; attribute)
                return static_cast<int>(attribute->mCurrentValue);

            return "None";
        });
        this->registerVariable("player_exp_level", [](Player& player) -> frontend::TypedValue {
            if (auto attribute = player.getAttribute(Player::LEVEL()).mPtr; attribute)
                return static_cast<int>(attribute->mCurrentValue);

            return "None";
        });
        this->registerVariable("player_exp_level_next", [](Player& player) -> int {
            return player.getXpNeededForNextLevel();
        });
        this->registerVariable("player_handitem", [](Player& player) -> std::string {
            return player.getCarriedItem().getName();
        });
        this->registerVariable("player_offhand", [](Player& player) -> std::string {
            return player.getOffhandSlot().getName();
        });
        this->registerVariable("player_ms", [](Player& player) -> int {
            return static_cast<int>(std::min(player.getNetworkStatus()->mAveragePing->count(), static_cast<long long>(300000)));
        });
        this->registerVariable("player_ms_avg", [](Player& player) -> int {
            return static_cast<int>(std::min(player.getNetworkStatus()->mCurrentPing->count(), static_cast<long long>(300000)));
        });
        this->registerVariable("player_packet", [](Player& player) -> float {
            return player.getNetworkStatus()->mAveragePacketLoss;
        });
        this->registerVariable("player_packet_avg", [](Player& player) -> float {
            return player.getNetworkStatus()->mCurrentPacketLoss;
        });
        this->registerVariable("server_tps", []() -> float {
            auto mMspt = static_cast<double>(ProfilerLite::gProfilerLiteInstance().mDebugServerTickTime->count()) / 1e6;
            return static_cast<float>(mMspt <= 50.0 ? 20.0 : static_cast<double>(1000.0 / mMspt));
        });
        this->registerVariable("server_mspt", []() -> float { 
            return static_cast<float>(static_cast<double>(ProfilerLite::gProfilerLiteInstance().mDebugServerTickTime->count()) / 1e6);
        });
        this->registerVariable("server_time", []() -> std::string {
            return SystemUtils::getNowTime();
        });
        this->registerVariable("server_player_max", []() -> int {
            return ll::service::getServerNetworkHandler()->mMaxNumPlayers;
        });
        this->registerVariable("server_player_online", []() -> int {
            return ll::service::getLevel()->getActivePlayerCount();
        });
        this->registerVariable("server_entity", []() -> int {
            return static_cast<int>(ll::service::getLevel()->getRuntimeActorList().size());
        });
        this->registerVariable("score", [](Player& player, const frontend::CallbackTypeValues& args) -> int {
            std::string name = std::get<std::string>(args[0]);

            return ScoreboardUtils::getScore(player, name);
        }, { frontend::ParamType::STRING });
        this->registerVariable("tr", [](Player& player, const frontend::CallbackTypeValues& args) -> ll::Expected<std::string> {
            std::string name = std::get<std::string>(args[0]);

            return Plugins::LanguagePlugin::getShared()->getLanguage(player)
                .transform([name](const std::string& language) -> std::string {
                    return I18nUtils::getInstance()->get(language, name);
                });
        }, { frontend::ParamType::STRING });
        this->registerVariable("tr", [](const frontend::CallbackTypeValues& args) -> std::string {
            std::string langcode = std::get<std::string>(args[0]);
            std::string name = std::get<std::string>(args[1]);

            return I18nUtils::getInstance()->get(langcode, name);
        }, { frontend::ParamType::STRING, frontend::ParamType::STRING });
        this->registerVariable("entity", [](const frontend::CallbackTypeValues& args) -> int {
            std::string name = std::get<std::string>(args[0]);

            std::vector<Actor*> mRuntimeActorList = ll::service::getLevel()->getRuntimeActorList();
            int count = static_cast<int>(std::count_if(mRuntimeActorList.begin(), mRuntimeActorList.end(), [name](Actor* actor) -> bool {
                return actor->getTypeName() == name;
            }));
            
            return count;
        }, { frontend::ParamType::STRING });
    }
    APIUtils::~APIUtils() = default;

    APIUtils& APIUtils::getInstance() {
        static APIUtils instance;
        return instance;
    }

    void APIUtils::registerVariable(const std::string& name, std::function<ll::Expected<frontend::TypedValue>()> callback) {
        this->mImpl->mVariableCommonMap.emplace(name, std::move(callback));

        frontend::MacroCall::getInstance().registerMacro(name, [this, name](const frontend::CallbackTypeValues&) -> ll::Expected<frontend::TypedValue> {
            return this->getValueForVariable(name);
        }, {});
    }

    void APIUtils::registerVariable(const std::string& name, std::function<ll::Expected<frontend::TypedValue>(Player&)> callback) {
        this->mImpl->mVariableMap.emplace(name, std::move(callback));

        frontend::MacroCall::getInstance().registerMacro(name, [this, name](const frontend::CallbackTypeValues&, const frontend::CallbackTypePlaces& placeholders) -> ll::Expected<frontend::TypedValue> {
            return this->getValueForVariable(name, std::any_cast<std::reference_wrapper<Player>>(placeholders.at(0)));
        }, {});
    }

    void APIUtils::registerVariable(const std::string& name, std::function<ll::Expected<frontend::TypedValue>(const frontend::CallbackTypeValues&)> callback, frontend::CallbackTypeArgs args) {
        this->mImpl->mVariableCommonMapParameter.emplace(name, std::move(callback));

        frontend::MacroCall::getInstance().registerMacro(name, [this, name](const frontend::CallbackTypeValues& args) -> ll::Expected<frontend::TypedValue> {
            return this->getValueForVariable(name, args);
        }, args);
    }

    void APIUtils::registerVariable(const std::string& name, std::function<ll::Expected<frontend::TypedValue>(Player&, const frontend::CallbackTypeValues&)> callback, frontend::CallbackTypeArgs args) {
        this->mImpl->mVariableMapParameter.emplace(name, std::move(callback));

        frontend::MacroCall::getInstance().registerMacro(name, [this, name](const frontend::CallbackTypeValues& args, const frontend::CallbackTypePlaces& placeholders) -> ll::Expected<frontend::TypedValue> {
            return this->getValueForVariable(name, std::any_cast<std::reference_wrapper<Player>>(placeholders.at(0)), args);
        }, args);
    }

    ll::Expected<frontend::TypedValue> APIUtils::getValueForVariable(const std::string& name) {
        auto it = this->mImpl->mVariableCommonMap.find(name);
        return it != this->mImpl->mVariableCommonMap.end() ? it->second() : "None";
    }

    ll::Expected<frontend::TypedValue> APIUtils::getValueForVariable(const std::string& name, Player& player) {
        auto it = this->mImpl->mVariableMap.find(name);
        return it != this->mImpl->mVariableMap.end() ? it->second(player) : this->getValueForVariable(name);
    }

    ll::Expected<frontend::TypedValue> APIUtils::getValueForVariable(const std::string& name, const frontend::CallbackTypeValues& parameter) {
        auto it = this->mImpl->mVariableCommonMapParameter.find(name);
        return it != this->mImpl->mVariableCommonMapParameter.end() ? it->second(parameter) : "None";
    }

    ll::Expected<frontend::TypedValue> APIUtils::getValueForVariable(const std::string& name, Player& player, const frontend::CallbackTypeValues& parameter) {
        auto it = this->mImpl->mVariableMapParameter.find(name);
        return it != this->mImpl->mVariableMapParameter.end() ? it->second(player, parameter) : this->getValueForVariable(name, parameter);
    }

    std::string APIUtils::translate(const std::string& str, Player& player) {
        frontend::DiagnosticEngine diagnostics;

        frontend::ir::VM mVM;

        if (this->mImpl->mAstCache.contains(str)) {
            auto mCached = this->mImpl->mAstCache.get(str);

            if (mCached.has_value()) {
                auto result = mVM.run(*mCached.value(), { std::ref(player) }, diagnostics);
                if (diagnostics.hasErrors()) {
                    this->mImpl->logger->error("APIUtils: {}", diagnostics.getErrorMessage());
                    diagnostics.clear();
                }
                
                return frontend::ir::VM::valueToString(result);
            }
        }

        frontend::ir::Compiler mCompiler(diagnostics);

        frontend::Lexer mLexer(str, diagnostics);
        frontend::Parser mParser(mLexer, diagnostics);

        auto mAst = mParser.parse();
        if (diagnostics.hasErrors()) {
            this->mImpl->logger->error("APIUtils: {}", diagnostics.getErrorMessage());
            return str;
        }

        frontend::SemanticAnalyzer analyzer(diagnostics);
        if (auto tpl = dynamic_cast<frontend::TemplateNode*>(mAst.get()))
            analyzer.analyze(*tpl);

        if (diagnostics.hasErrors()) {
            this->mImpl->logger->error("APIUtils: {}", diagnostics.getErrorMessage());
            return str;
        }

        if (diagnostics.hasWarnings()) {
            this->mImpl->logger->warn("APIUtils: {}", diagnostics.getWarningMessage());
            return str;
        }

        auto bytecode = mCompiler.compile(*mAst);
        if (diagnostics.hasErrors()) {
            this->mImpl->logger->error("APIUtils: {}", diagnostics.getErrorMessage());
            return str;
        }

        frontend::ir::Optimizer optimizer;
        optimizer.optimize(bytecode);

        auto result = mVM.run(bytecode, { std::ref(player) }, diagnostics);
        if (diagnostics.hasErrors()) {
            this->mImpl->logger->error("APIUtils: {}", diagnostics.getErrorMessage());
            return str;
        }

        this->mImpl->mAstCache.put(str, bytecode);
        return frontend::ir::VM::valueToString(result);
    }

    std::string APIUtils::translate(const std::string& str) {
        frontend::DiagnosticEngine diagnostics;

        frontend::ir::VM mVM;

        if (this->mImpl->mAstCache.contains(str)) {
            auto mCached = this->mImpl->mAstCache.get(str);

            if (mCached.has_value()) {
                auto result = mVM.run(*mCached.value(), {}, diagnostics);
                if (diagnostics.hasErrors()) {
                    this->mImpl->logger->error("APIUtils: {}", diagnostics.getErrorMessage());
                    diagnostics.clear();
                }

                return frontend::ir::VM::valueToString(result);
            }
        }

        frontend::ir::Compiler mCompiler(diagnostics);

        frontend::Lexer mLexer(str, diagnostics);
        frontend::Parser mParser(mLexer, diagnostics);

        auto mAst = mParser.parse();
        if (diagnostics.hasErrors()) {
            this->mImpl->logger->error("APIUtils: {}", diagnostics.getErrorMessage());
            return str;
        }

        frontend::SemanticAnalyzer analyzer(diagnostics);
        if (auto tpl = dynamic_cast<frontend::TemplateNode*>(mAst.get()))
            analyzer.analyze(*tpl);

        if (diagnostics.hasErrors()) {
            this->mImpl->logger->error("APIUtils: {}", diagnostics.getErrorMessage());
            return str;
        }

        if (diagnostics.hasWarnings()) {
            this->mImpl->logger->warn("APIUtils: {}", diagnostics.getWarningMessage());
            return str;
        }

        auto bytecode = mCompiler.compile(*mAst);
        if (diagnostics.hasErrors()) {
            this->mImpl->logger->error("APIUtils: {}", diagnostics.getErrorMessage());
            return str;
        }

        frontend::ir::Optimizer optimizer;
        optimizer.optimize(bytecode);

        auto result = mVM.run(bytecode, {}, diagnostics);
        if (diagnostics.hasErrors()) {
            this->mImpl->logger->error("APIUtils: {}", diagnostics.getErrorMessage());
            return str;
        }

        this->mImpl->mAstCache.put(str, bytecode);
        return frontend::ir::VM::valueToString(result);
    }
}
