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

#include "LOICollectionA/utils/ScoreboardUtils.h"
#include "LOICollectionA/utils/SystemUtils.h"
#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/include/APIEngine.h"

#include "LOICollectionA/include/APIUtils.h"

std::unordered_map<std::string, std::function<std::string(Player&)>> mVariableMap;
std::unordered_map<std::string, std::function<std::string(Player&, std::string)>> mVariableMapParameter;

namespace LOICollection::LOICollectionAPI {
    class VariableProcessor : public IContentProcessor {
    public:
        std::string process(const std::string& content, const Context& context) override {
            auto [name, parameter] = parser(content);

            if (parameter.empty())
                return getValueForVariable(name, std::any_cast<std::reference_wrapper<Player>>(context.params.at(0)));

            return getValueForVariable(name, std::any_cast<std::reference_wrapper<Player>>(context.params.at(0)), parameter);
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
            return tryGetGrammarResult(content);
        }
    };

    void initialization() {
        APIEngine::getInstance().registery(
            std::make_unique<VariableProcessor>(),
            APIEngineConfig{ "variable", "{", "}", "$", 2 }
        );
        APIEngine::getInstance().registery(
            std::make_unique<GrammarProcessor>(),
            APIEngineConfig{ "grammar", "@", "@", "", 1 }
        );

        registerVariable("version.mc", [](Player&) -> std::string {
            return ll::getGameVersion().to_string();
        });
        registerVariable("version.ll", [](Player&) -> std::string {
            return ll::getLoaderVersion().to_string();
        });
        registerVariable("version.protocol", [](Player&) -> std::string {
            return std::to_string(ll::getNetworkProtocolVersion()); 
        });
        registerVariable("player", [](Player& player) -> std::string {
            return std::string{player.mName};
        });
        registerVariable("player.title", [](Player& player) -> std::string {
            return Plugins::ChatPlugin::getInstance().getTitle(player);
        });
        registerVariable("player.title.time", [](Player& player) -> std::string {
            return SystemUtils::toFormatTime(
                Plugins::ChatPlugin::getInstance().getTitleTime(player, Plugins::ChatPlugin::getInstance().getTitle(player)), "None"
            );
        });
        registerVariable("player.pvp", [](Player& player) -> std::string {
            return Plugins::PvpPlugin::getInstance().isEnable(player) ? "true" : "false";
        });
        registerVariable("player.mute", [](Player& player) -> std::string {
            return Plugins::MutePlugin::getInstance().isMute(player) ? "true" : "false";
        });
        registerVariable("player.language", [](Player& player) -> std::string { 
            return Plugins::LanguagePlugin::getInstance().getLanguage(player);
        });
        registerVariable("player.language.name", [](Player& player) -> std::string {
            return I18nUtils::getInstance()->get(Plugins::LanguagePlugin::getInstance().getLanguage(player), "name");
        });
        registerVariable("player.gamemode", [](Player& player) -> std::string {
            return std::string(magic_enum::enum_name(player.getPlayerGameType()));
        });
        registerVariable("player.pos", [](Player& player) -> std::string {
            return player.getPosition().toString();
        });
        registerVariable("player.pos.x", [](Player& player) -> std::string {
            return std::to_string((int)player.getPosition().x);
        });
        registerVariable("player.pos.y", [](Player& player) -> std::string {
            return std::to_string((int)player.getPosition().y);
        });
        registerVariable("player.pos.z", [](Player& player) -> std::string {
            return std::to_string((int)player.getPosition().z);
        });
        registerVariable("player.pos.respawn", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? player.getExpectedSpawnPosition().toString() : "None";
        });
        registerVariable("player.pos.respawn.x", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? std::to_string((int)player.getExpectedSpawnPosition().x) : "None";
        });
        registerVariable("player.pos.respawn.y", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? std::to_string((int)player.getExpectedSpawnPosition().y) : "None";
        });
        registerVariable("player.pos.respawn.z", [](Player& player) -> std::string {
            return player.hasRespawnPosition() ? std::to_string((int)player.getExpectedSpawnPosition().z) : "None";
        });
        registerVariable("player.pos.block", [](Player& player) -> std::string {
            return player.getEyePos().toString();
        });
        registerVariable("player.pos.lastdeath", [](Player& player) -> std::string {
            return player.getLastDeathPos() ? player.getLastDeathPos()->toString() : "None";
        });
        registerVariable("player.realname", [](Player& player) -> std::string {
            return player.getRealName();
        });
        registerVariable("player.xuid", [](Player& player) -> std::string {
            return player.getXuid();
        });
        registerVariable("player.uuid", [](Player& player) -> std::string {
            return player.getUuid().asString();
        });
        registerVariable("player.is.op", [](Player& player) -> std::string {
            return player.isOperator() ? "true" : "false";
        });
        registerVariable("player.can.fly", [](Player& player) -> std::string {
            return player.canFly() ? "true" : "false";
        });
        registerVariable("player.health", [](Player& player) -> std::string {
            return std::to_string(ActorAttribute::getHealth(player.getEntityContext()));
        });
        registerVariable("player.max.health", [](Player& player) -> std::string {
            return std::to_string((int)player.getMaxHealth());
        });
        registerVariable("player.hunger", [](Player& player) -> std::string {
            return std::to_string((int)player.getAttribute(Player::HUNGER()).mCurrentValue);
        });
        registerVariable("player.max.hunger", [](Player& player) -> std::string {
            return std::to_string((int)player.getAttribute(Player::HUNGER()).mCurrentMaxValue);
        });
        registerVariable("player.saturation", [](Player& player) -> std::string {
            return std::to_string((int)player.getAttribute(Player::SATURATION()).mCurrentValue);
        });
        registerVariable("player.max.saturation", [](Player& player) -> std::string {
            return std::to_string((int)player.getAttribute(Player::SATURATION()).mCurrentMaxValue);
        });
        registerVariable("player.speed", [](Player& player) -> std::string {
            return std::to_string(player.getSpeed());
        });
        registerVariable("player.direction", [](Player& player) -> std::string {
            return player.mBuiltInComponents->mActorRotationComponent->mRotationDegree->toString();
        });
        registerVariable("player.dimension", [](Player& player) -> std::string {
            return std::to_string(player.getDimensionId());
        });
        registerVariable("player.os", [](Player& player) -> std::string {
            return magic_enum::enum_name(player.mBuildPlatform).data();
        });
        registerVariable("player.ip", [](Player& player) -> std::string {
            return player.getIPAndPort();
        });
        registerVariable("player.exp.xp", [](Player& player) -> std::string {
            return std::to_string((int)player.getAttribute(Player::EXPERIENCE()).mCurrentValue);
        });
        registerVariable("player.exp.level", [](Player& player) -> std::string {
            return std::to_string((int)player.getAttribute(Player::LEVEL()).mCurrentValue);
        });
        registerVariable("player.exp.level.next", [](Player& player) -> std::string {
            return std::to_string(player.getXpNeededForNextLevel());
        });
        registerVariable("player.handitem", [](Player& player) -> std::string {
            return player.getCarriedItem().getName();
        });
        registerVariable("player.offhand", [](Player& player) -> std::string {
            return player.getOffhandSlot().getName();
        });
        registerVariable("player.ms", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mAveragePing);
        });
        registerVariable("player.ms.avg", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mCurrentPing);
        });
        registerVariable("player.packet", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mAveragePacketLoss);
        });
        registerVariable("player.packet.avg", [](Player& player) -> std::string {
            return std::to_string(player.getNetworkStatus()->mCurrentPacketLoss);
        });
        registerVariable("server.tps", [](Player&) -> std::string {
            double mMspt = ((double) ProfilerLite::gProfilerLiteInstance().mDebugServerTickTime->count() / 1e6);
            return std::to_string(mMspt <= 50.0 ? 20.0 : (double)(1000.0 / mMspt));
        });
        registerVariable("server.mspt", [](Player&) -> std::string { 
            return std::to_string((double) ProfilerLite::gProfilerLiteInstance().mDebugServerTickTime->count() / 1e6);
        });
        registerVariable("server.time", [](Player&) -> std::string {
            return SystemUtils::getNowTime();
        });
        registerVariable("server.player.max", [](Player&) -> std::string {
            return std::to_string(ll::memory::dAccess<int>(ll::service::getServerNetworkHandler().as_ptr(), 200 * 4));
        });
        registerVariable("server.player.online", [](Player&) -> std::string {
            return std::to_string(ll::service::getLevel()->getActivePlayerCount());
        });
        registerVariable("server.entity", [](Player&) -> std::string {
            return std::to_string(ll::service::getLevel()->getRuntimeActorList().size());
        });
        registerVariable("score", [](Player& player, const std::string& name) -> std::string {
            return std::to_string(ScoreboardUtils::getScore(player, name));
        });
        registerVariable("tr", [](Player& player, const std::string& name) -> std::string {
            return I18nUtils::getInstance()->get(Plugins::LanguagePlugin::getInstance().getLanguage(player), name);
        });
        registerVariable("entity", [](Player&, std::string name) -> std::string {
            std::vector<Actor*> mRuntimeActorList = ll::service::getLevel()->getRuntimeActorList();
            int count = (int)std::count_if(mRuntimeActorList.begin(), mRuntimeActorList.end(), [&name](Actor* actor) -> bool {
                return actor->getTypeName() == name;
            });
            return std::to_string(count);
        });
    }

    void registerVariable(const std::string& name, std::function<std::string(Player&)> callback) {
        mVariableMap.emplace(name, std::move(callback));
    }

    void registerVariable(const std::string& name, std::function<std::string(Player&, std::string)> callback) {
        mVariableMapParameter.emplace(name, std::move(callback));
    }

    std::string getValueForVariable(const std::string& name, Player& player) try {
        auto it = mVariableMap.find(name);
        return it != mVariableMap.end() ? it->second(player) : "None";
    } catch (...) {
        return "None";
    }

    std::string getValueForVariable(const std::string& name, Player& player, const std::string& parameter) try {
        auto it = mVariableMapParameter.find(name);
        return it != mVariableMapParameter.end() ? it->second(player, parameter) : "None";
    } catch (...) {
        return "None";
    }

    std::string tryGetGrammarResult(const std::string& str) try {
        frontend::Lexer mLexer(str);
        frontend::Parser mParser(mLexer);
        frontend::Evaluator mEvaluator;
        return mEvaluator.evaluate(*mParser.parse());
    } catch (...) {
        return "None";
    }

    std::string getVariableString(const std::string& str, Player& player) {
        return APIEngine::getInstance().get("variable", str, { std::ref(player) });
    }

    std::string getGrammarString(const std::string& str) {
        return APIEngine::getInstance().get("grammar", str);
    }

    std::string translateString(const std::string& str, Player& player) {
        return APIEngine::getInstance().process(str, { std::ref(player) });
    }
}
