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

#include <mc/util/ProfilerLite.h>
#include <mc/deps/core/math/Vec2.h>
#include <mc/deps/core/math/Vec3.h>

#include <mc/world/level/Level.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/actor/BuiltInActorComponents.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/provider/ActorAttribute.h>
#include <mc/world/attribute/AttributeInstance.h>
#include <mc/entity/components/ActorRotationComponent.h>
#include <mc/network/ServerNetworkHandler.h>

#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
#include "LOICollectionA/frontend/Evaluator.h"

#include "LOICollectionA/include/Plugins/PvpPlugin.h"
#include "LOICollectionA/include/Plugins/MutePlugin.h"
#include "LOICollectionA/include/Plugins/ChatPlugin.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/include/APIEngine.h"

#include "LOICollectionA/include/APIUtils.h"

using namespace LOICollection::LOICollectionAPI;

class VariableProcessor : public IContentProcessor {
public:
    std::string process(const std::string& content, const Context& context) override {
        auto [name, parameter] = parser(content);

        if (parameter.empty())
            return APIUtils::getInstance().getValueForVariable(name, std::any_cast<std::reference_wrapper<Player>>(context.params.at(0)));

        return APIUtils::getInstance().getValueForVariable(name, std::any_cast<std::reference_wrapper<Player>>(context.params.at(0)), parameter);
    }

    std::pair<std::string, std::string> parser(const std::string& content) {
        size_t pos = content.find('(');
        if (pos != std::string::npos && content.back() == ')')
            return {
                content.substr(0, pos),
                content.substr(pos + 1, content.size() - pos - 2)
            };
        
        return { content, "" };
    }
};

class GrammarProcessor : public IContentProcessor {
public:
    std::string process(const std::string& content, const Context&) override {
        return APIUtils::getInstance().tryGetGrammarResult(content);
    }
};

namespace LOICollection::LOICollectionAPI {
    struct APIUtils::Impl {
        std::unordered_map<std::string, std::function<std::string(Player&)>> mVariableMap;
        std::unordered_map<std::string, std::function<std::string(Player&, std::string)>> mVariableMapParameter;
    };

    APIUtils::APIUtils() : mImpl(std::make_unique<Impl>()) {
        APIEngine::getInstance().registery(
            std::make_unique<VariableProcessor>(),
            APIEngineConfig{ "variable", "{", "}", "$", 2 }
        );
        APIEngine::getInstance().registery(
            std::make_unique<GrammarProcessor>(),
            APIEngineConfig{ "grammar", "@", "@", "", 1 }
        );

        this->registerVariable("version.mc", [](Player&) -> std::string {
            return ll::getGameVersion().to_string();
        });
        this->registerVariable("version.ll", [](Player&) -> std::string {
            return ll::getLoaderVersion().to_string();
        });
        this->registerVariable("version.protocol", [](Player&) -> std::string {
            return std::to_string(ll::getNetworkProtocolVersion()); 
        });
        this->registerVariable("player", [](Player& player) -> std::string {
            return std::string{player.mName};
        });
        this->registerVariable("player.title", [](Player& player) -> std::string {
            return Plugins::ChatPlugin::getInstance().getTitle(player);
        });
        this->registerVariable("player.title.time", [](Player& player) -> std::string {
            return SystemUtils::toFormatTime(
                Plugins::ChatPlugin::getInstance().getTitleTime(player, Plugins::ChatPlugin::getInstance().getTitle(player)), "None"
            );
        });
        this->registerVariable("player.pvp", [](Player& player) -> std::string {
            return Plugins::PvpPlugin::getInstance().isEnable(player) ? "true" : "false";
        });
        this->registerVariable("player.mute", [](Player& player) -> std::string {
            return Plugins::MutePlugin::getInstance().isMute(player) ? "true" : "false";
        });
        this->registerVariable("player.language", [](Player& player) -> std::string { 
            return Plugins::LanguagePlugin::getInstance().getLanguage(player);
        });
        this->registerVariable("player.language.name", [](Player& player) -> std::string {
            return I18nUtils::getInstance()->get(Plugins::LanguagePlugin::getInstance().getLanguage(player), "name");
        });
        this->registerVariable("player.gamemode", [](Player& player) -> std::string {
            return std::string(magic_enum::enum_name(player.getPlayerGameType()));
        });
        this->registerVariable("player.pos", [](Player& player) -> std::string {
            return player.getPosition().toString();
        });
        this->registerVariable("player.pos.x", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getPosition().x));
        });
        this->registerVariable("player.pos.y", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getPosition().y));
        });
        this->registerVariable("player.pos.z", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getPosition().z));
        });
        this->registerVariable("player.pos.respawn", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? player.getExpectedSpawnPosition().toString() : "None";
        });
        this->registerVariable("player.pos.respawn.x", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? std::to_string(static_cast<int>(player.getExpectedSpawnPosition().x)) : "None";
        });
        this->registerVariable("player.pos.respawn.y", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? std::to_string(static_cast<int>(player.getExpectedSpawnPosition().y)) : "None";
        });
        this->registerVariable("player.pos.respawn.z", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? std::to_string(static_cast<int>(player.getExpectedSpawnPosition().z)) : "None";
        });
        this->registerVariable("player.pos.block", [](Player& player) -> std::string {
            return player.getEyePos().toString();
        });
        this->registerVariable("player.pos.lastdeath", [](Player& player) -> std::string {
            return player.getLastDeathPos() ? player.getLastDeathPos()->toString() : "None";
        });
        this->registerVariable("player.realname", [](Player& player) -> std::string {
            return player.getRealName();
        });
        this->registerVariable("player.xuid", [](Player& player) -> std::string {
            return player.getXuid();
        });
        this->registerVariable("player.uuid", [](Player& player) -> std::string {
            return player.getUuid().asString();
        });
        this->registerVariable("player.is.op", [](Player& player) -> std::string {
            return player.isOperator() ? "true" : "false";
        });
        this->registerVariable("player.can.fly", [](Player& player) -> std::string {
            return player.canFly() ? "true" : "false";
        });
        this->registerVariable("player.health", [](Player& player) -> std::string {
            return std::to_string(ActorAttribute::getHealth(player.getEntityContext()));
        });
        this->registerVariable("player.max.health", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getMaxHealth()));
        });
        this->registerVariable("player.hunger", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getAttribute(Player::HUNGER()).mCurrentValue));
        });
        this->registerVariable("player.max.hunger", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getAttribute(Player::HUNGER()).mCurrentMaxValue));
        });
        this->registerVariable("player.saturation", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getAttribute(Player::SATURATION()).mCurrentValue));
        });
        this->registerVariable("player.max.saturation", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getAttribute(Player::SATURATION()).mCurrentMaxValue));
        });
        this->registerVariable("player.speed", [](Player& player) -> std::string {
            return std::to_string(player.getSpeed());
        });
        this->registerVariable("player.direction", [](Player& player) -> std::string {
            return player.mBuiltInComponents->mActorRotationComponent->mRotationDegree->toString();
        });
        this->registerVariable("player.dimension", [](Player& player) -> std::string {
            return std::to_string(player.getDimensionId());
        });
        this->registerVariable("player.os", [](Player& player) -> std::string {
            return magic_enum::enum_name(player.mBuildPlatform).data();
        });
        this->registerVariable("player.ip", [](Player& player) -> std::string {
            return player.getIPAndPort();
        });
        this->registerVariable("player.exp.xp", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getAttribute(Player::EXPERIENCE()).mCurrentValue));
        });
        this->registerVariable("player.exp.level", [](Player& player) -> std::string {
            return std::to_string(static_cast<int>(player.getAttribute(Player::LEVEL()).mCurrentValue));
        });
        this->registerVariable("player.exp.level.next", [](Player& player) -> std::string {
            return std::to_string(player.getXpNeededForNextLevel());
        });
        this->registerVariable("player.handitem", [](Player& player) -> std::string {
            return player.getCarriedItem().getName();
        });
        this->registerVariable("player.offhand", [](Player& player) -> std::string {
            return player.getOffhandSlot().getName();
        });
        this->registerVariable("player.ms", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mAveragePing);
        });
        this->registerVariable("player.ms.avg", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mCurrentPing);
        });
        this->registerVariable("player.packet", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mAveragePacketLoss);
        });
        this->registerVariable("player.packet.avg", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mCurrentPacketLoss);
        });
        this->registerVariable("server.tps", [](Player&) -> std::string {
            auto mMspt = static_cast<double>(ProfilerLite::gProfilerLiteInstance().mDebugServerTickTime->count()) / 1e6;
            return std::to_string(mMspt <= 50.0 ? 20.0 : static_cast<double>(1000.0 / mMspt));
        });
        this->registerVariable("server.mspt", [](Player&) -> std::string { 
            return std::to_string(static_cast<double>(ProfilerLite::gProfilerLiteInstance().mDebugServerTickTime->count()) / 1e6);
        });
        this->registerVariable("server.time", [](Player&) -> std::string {
            return SystemUtils::getNowTime();
        });
        this->registerVariable("server.player.max", [](Player&) -> std::string {
            return std::to_string(ll::service::getServerNetworkHandler()->mMaxNumPlayers);
        });
        this->registerVariable("server.player.online", [](Player&) -> std::string {
            return std::to_string(ll::service::getLevel()->getActivePlayerCount());
        });
        this->registerVariable("server.entity", [](Player&) -> std::string {
            return std::to_string(ll::service::getLevel()->getRuntimeActorList().size());
        });
        this->registerVariable("score", [](Player& player, const std::string& name) -> std::string {
            return std::to_string(ScoreboardUtils::getScore(player, name));
        });
        this->registerVariable("tr", [](Player& player, const std::string& name) -> std::string {
            return I18nUtils::getInstance()->get(Plugins::LanguagePlugin::getInstance().getLanguage(player), name);
        });
        this->registerVariable("entity", [](Player&, std::string name) -> std::string {
            std::vector<Actor*> mRuntimeActorList = ll::service::getLevel()->getRuntimeActorList();
            int count = static_cast<int>(std::count_if(mRuntimeActorList.begin(), mRuntimeActorList.end(), [&name](Actor* actor) -> bool {
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

    void APIUtils::registerVariable(const std::string& name, std::function<std::string(Player&)> callback) {
        this->mImpl->mVariableMap.emplace(name, std::move(callback));
    }

    void APIUtils::registerVariable(const std::string& name, std::function<std::string(Player&, std::string)> callback) {
        this->mImpl->mVariableMapParameter.emplace(name, std::move(callback));
    }

    std::string APIUtils::getValueForVariable(const std::string& name, Player& player) try {
        auto it = this->mImpl->mVariableMap.find(name);
        return it != this->mImpl->mVariableMap.end() ? it->second(player) : "None";
    } catch (...) {
        return "None";
    }

    std::string APIUtils::getValueForVariable(const std::string& name, Player& player, const std::string& parameter) try {
        auto it = this->mImpl->mVariableMapParameter.find(name);
        return it != this->mImpl->mVariableMapParameter.end() ? it->second(player, parameter) : "None";
    } catch (...) {
        return "None";
    }

    std::string APIUtils::tryGetGrammarResult(const std::string& str) try {
        frontend::Lexer mLexer(str);
        frontend::Parser mParser(mLexer);
        frontend::Evaluator mEvaluator;
        return mEvaluator.evaluate(*mParser.parse());
    } catch (...) {
        return "None";
    }

    std::string APIUtils::getVariableString(const std::string& str, Player& player) {
        return APIEngine::getInstance().get("variable", str, { std::ref(player) });
    }

    std::string APIUtils::getGrammarString(const std::string& str) {
        return APIEngine::getInstance().get("grammar", str);
    }

    std::string APIUtils::translateString(const std::string& str, Player& player) {
        return APIEngine::getInstance().process(str, { std::ref(player) });
    }
}
