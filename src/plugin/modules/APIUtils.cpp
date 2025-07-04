#include <string>
#include <vector>
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

#include "frontend/Lexer.h"
#include "frontend/Parser.h"
#include "frontend/Evaluator.h"

#include "include/PvpPlugin.h"
#include "include/MutePlugin.h"
#include "include/ChatPlugin.h"
#include "include/LanguagePlugin.h"

#include "utils/ScoreboardUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "include/APIUtils.h"

std::unordered_map<std::string, std::function<std::string(Player&)>> mVariableMap;
std::unordered_map<std::string, std::function<std::string(Player&, std::string)>> mVariableMapParameter;

namespace LOICollection::LOICollectionAPI {
    void initialization() {
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
            Plugins::chat::update(player);
            return Plugins::chat::getTitle(player);
        });
        registerVariable("player.title.time", [](Player& player) -> std::string {
            Plugins::chat::update(player);
            return SystemUtils::formatDataTime(
                Plugins::chat::getTitleTime(player, Plugins::chat::getTitle(player)), "None"
            );
        });
        registerVariable("player.pvp", [](Player& player) -> std::string {
            return Plugins::pvp::isEnable(player) ? "true" : "false";
        });
        registerVariable("player.mute", [](Player& player) -> std::string {
            return Plugins::mute::isMute(player) ? "true" : "false";
        });
        registerVariable("player.language", [](Player& player) -> std::string { 
            return Plugins::language::getLanguage(player);
        });
        registerVariable("player.language.name", [](Player& player) -> std::string {
            return I18nUtils::getInstance()->get(Plugins::language::getLanguage(player), "name");
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
            return I18nUtils::getInstance()->get(Plugins::language::getLanguage(player), name);
        });
        registerVariable("entity", [](Player&, std::string name) -> std::string {
            std::vector<Actor*> mRuntimeActorList = ll::service::getLevel()->getRuntimeActorList();
            int count = (int)std::count_if(mRuntimeActorList.begin(), mRuntimeActorList.end(), [&name](Actor* actor) {
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

    std::string tryGetGrammarResult(const std::string& contentString) try {
        frontend::Lexer mLexer(contentString);
        frontend::Parser mParser(mLexer);
        frontend::Evaluator mEvaluator;
        return mEvaluator.evaluate(*mParser.parse());
    } catch (...) {
        return "None";
    }

    std::string& translateString(std::string& contentString, Player& player) {
        if (contentString.empty() || contentString.find_first_of("{}@") == std::string::npos)
            return contentString;

        std::string FinalResult;
        std::string InitialResult;
        std::vector<VarOccurrence> occurrences;
        
        FinalResult.reserve(contentString.size() * 2);
        InitialResult.reserve(contentString.size() * 2);
        occurrences.reserve(30);

        size_t mStartPos = 0;
        while (mStartPos < contentString.size()) {
            if (contentString[mStartPos] != '{') {
                mStartPos++;
                continue;
            }

            if (size_t mEndPos = contentString.find('}', mStartPos + 1); mEndPos != std::string::npos) {
                std::string mVariableName = contentString.substr(mStartPos + 1, mEndPos - mStartPos - 1);
                
                if (mVariableName.find('(') != std::string::npos && mVariableName.back() == ')') {
                    std::string mVariableParameterName = mVariableName.substr(0, mVariableName.find('('));
                    std::string mVariableParameterValue = mVariableName.substr(mVariableName.find('(') + 1, mVariableName.length() - mVariableName.find('(') - 2);
                    if (auto itp = mVariableMapParameter.find(mVariableParameterName); itp != mVariableMapParameter.end()) {
                        std::string mValue = getValueForVariable(mVariableParameterName, player, mVariableParameterValue);
                        occurrences.push_back({mStartPos, mEndPos - mStartPos + 1, mValue});
                    }
                } else if (auto it = mVariableMap.find(mVariableName); it != mVariableMap.end()) {
                    std::string mValue = getValueForVariable(mVariableName, player);
                    occurrences.push_back({mStartPos, mEndPos - mStartPos + 1, mValue});
                }

                mStartPos = mEndPos + 1;
            }
        }

        size_t mInitialPos = 0;
        for (const auto& occ : occurrences) {
            InitialResult.append(contentString, mInitialPos, occ.start - mInitialPos);
            InitialResult.append(occ.replacement);
            mInitialPos = occ.start + occ.length;
        }

        if (mInitialPos < contentString.size())
            InitialResult.append(contentString, mInitialPos, contentString.size() - mInitialPos);

        occurrences.clear();

        size_t mGrammarStartPos = 0;
        while (mGrammarStartPos < InitialResult.size()) {
            if (InitialResult[mGrammarStartPos] != '@') {
                mGrammarStartPos++;
                continue;
            }

            size_t mGrammarEndPos = InitialResult.find('@', mGrammarStartPos + 1);
            if (mGrammarEndPos == std::string::npos) 
                break;

            std::string mGrammar = InitialResult.substr(mGrammarStartPos + 1, mGrammarEndPos - mGrammarStartPos - 1);
            std::string mValue = tryGetGrammarResult(mGrammar);
            occurrences.push_back({mGrammarStartPos, mGrammarEndPos - mGrammarStartPos + 1, mValue});

            mGrammarStartPos = mGrammarEndPos + 1;
        }

        size_t mFinalPos = 0;
        for (const auto& occ : occurrences) {
            FinalResult.append(InitialResult, mFinalPos, occ.start - mFinalPos);
            FinalResult.append(occ.replacement);
            mFinalPos = occ.start + occ.length;
        }

        if (mFinalPos < InitialResult.size())
            FinalResult.append(InitialResult, mFinalPos, InitialResult.size() - mFinalPos);

        contentString.swap(FinalResult);
        return contentString;
    }

    std::string& translateString(const std::string& contentString, Player& player) {
        return translateString(const_cast<std::string&>(contentString), player);
    }
}