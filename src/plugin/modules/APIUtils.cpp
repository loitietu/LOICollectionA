#include <regex>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <unordered_map>

#include <magic_enum.hpp>

#include <ll/api/Versions.h>
#include <ll/api/memory/Memory.h>
#include <ll/api/service/Bedrock.h>

#include <mc/util/ProfilerLite.h>
#include <mc/deps/core/math/Vec3.h>

#include <mc/world/level/Level.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/actor/player/Player.h>
#include <mc/network/ServerNetworkHandler.h>

#include "include/pvpPlugin.h"
#include "include/mutePlugin.h"
#include "include/chatPlugin.h"
#include "include/languagePlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "include/APIUtils.h"

std::unordered_map<std::string, std::function<std::string(Player&)>> mVariableMap;
std::unordered_map<std::string, std::function<std::string(Player&, std::string)>> mVariableMapParameter;

namespace LOICollection::LOICollectionAPI {
    void initialization() {
        registerVariable("version.mc", [](Player& /*unused*/) -> std::string {
            return ll::getGameVersion().to_string();
        });
        registerVariable("version.ll", [](Player& /*unused*/) -> std::string {
            return ll::getLoaderVersion().to_string();
        });
        registerVariable("version.protocol", [](Player& /*unused*/) -> std::string {
            return std::to_string(ll::getNetworkProtocolVersion()); 
        });
        registerVariable("player", [](Player& player) -> std::string {
            return player.getName();
        });
        registerVariable("player.title", [](Player& player) -> std::string {
            Plugins::chat::update(player);
            return Plugins::chat::getTitle(player);
        });
        registerVariable("player.title.time", [](Player& player) -> std::string {
            Plugins::chat::update(player);
            return SystemUtils::formatDataTime(
                Plugins::chat::getTitleTime(player, 
                    Plugins::chat::getTitle(player)
                )
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
        registerVariable("player.pos", [](Player& player) -> std::string {
            return player.getPosition().toString();
        });
        registerVariable("player.pos.block", [](Player& player) -> std::string {
            return player.getEyePos().toString();
        });
        registerVariable("player.pos.lastDeath", [](Player& player) -> std::string {
            return player.getLastDeathPos()->toString();
        });
        registerVariable("player.realName", [](Player& player) -> std::string {
            return player.getRealName();
        });
        registerVariable("player.xuid", [](Player& player) -> std::string {
            return player.getXuid();
        });
        registerVariable("player.uuid", [](Player& player) -> std::string {
            return player.getUuid().asString();
        });
        registerVariable("player.canFly", [](Player& player) -> std::string {
            return player.canFly() ? "true" : "false";
        });
        registerVariable("player.maxHealth", [](Player& player) -> std::string {
            return std::to_string(player.getMaxHealth());
        });
        registerVariable("player.health", [](Player& player) -> std::string {
            return std::to_string(player.getHealth());
        });
        registerVariable("player.speed", [](Player& player) -> std::string {
            return std::to_string(player.getSpeed());
        });
        registerVariable("player.direction", [](Player& player) -> std::string {
            return std::to_string(player.getDirection());
        });
        registerVariable("player.dimension", [](Player& player) -> std::string {
            return std::to_string(player.getDimensionId());
        });
        registerVariable("player.os", [](Player& player) -> std::string {
            return std::string(magic_enum::enum_name(player.getPlatform()));
        });
        registerVariable("player.ip", [](Player& player) -> std::string {
            return player.getIPAndPort();
        });
        registerVariable("player.xp", [](Player& player) -> std::string {
            return std::to_string(player.getXpEarnedAtCurrentLevel());
        });
        registerVariable("player.HandItem", [](Player& player) -> std::string {
            return player.getCarriedItem().getName();
        });
        registerVariable("player.OffHand", [](Player& player) -> std::string {
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
        registerVariable("server.tps", [](Player& /*unused*/) -> std::string {
            double mMspt = ((double) ProfilerLite::gProfilerLiteInstance().getServerTickTime().count() / 1000000.0);
            return std::to_string(mMspt <= 50.0 ? 20.0 : (double)(1000.0 / mMspt));
        });
        registerVariable("server.mspt", [](Player& /*unused*/) -> std::string { 
            return std::to_string((double) ProfilerLite::gProfilerLiteInstance().getServerTickTime().count() / 1000000.0);
        });
        registerVariable("server.time", [](Player& /*unused*/) -> std::string {
            return SystemUtils::getNowTime();
        });
        registerVariable("server.player.max", [](Player& /*unused*/) -> std::string {
            return std::to_string(ll::memory::dAccess<int>(ll::service::getServerNetworkHandler().as_ptr(), 200 * 4));
        });
        registerVariable("server.player.online", [](Player& /*unused*/) -> std::string {
            return std::to_string(ll::service::getLevel()->getActivePlayerCount());
        });
        registerVariableParameter("score", [](Player& player, std::string name) -> std::string {
            return std::to_string(McUtils::scoreboard::getScore(player, name));
        });
        registerVariableParameter("tr", [](Player& player, std::string name) -> std::string {
            return I18nUtils::getInstance()->get(Plugins::language::getLanguage(player), name);
        });
        registerVariableParameter("entity", [](Player& /*unused*/, std::string name) -> std::string {
            std::vector<Actor*> mRuntimeActorList = ll::service::getLevel()->getRuntimeActorList();
            int count = (int)std::count_if(mRuntimeActorList.begin(), mRuntimeActorList.end(), [&name](Actor* actor) {
                return actor->getTypeName() == name;
            });
            return std::to_string(count);
        });
    }

    void registerVariable(const std::string& name, const std::function<std::string(Player&)> callback) {
        if (mVariableMap.find(name) != mVariableMap.end())
            return;
        mVariableMap[name] = std::move(callback);
    }

    void registerVariableParameter(const std::string& name, const std::function<std::string(Player&, std::string)> callback) {
        if (mVariableMapParameter.find(name) != mVariableMapParameter.end())
            return;
        mVariableMapParameter[name] = std::move(callback);
    }

    std::string getValueForVariable(const std::string& name, Player& player) {
        if (auto it = mVariableMap.find(name); it != mVariableMap.end()) {
            try {
                return it->second(player);
            } catch (...) {};
        }
        return "None";
    }

    std::string getValueForVariable(const std::string& name, Player& player, const std::string& parameter) {
        if (auto it = mVariableMapParameter.find(name); it != mVariableMapParameter.end()) {
            try {
                return it->second(player, parameter);
            } catch (...) {};
        }
        return "None";
    }

    std::string& translateString(std::string& contentString, Player& player) {
        if (contentString.empty())
            return contentString;

        std::smatch mMatchVariable;
        std::smatch mMatchParameter;
        std::regex mMatchVariableRegex("\\{(.*?)\\}");
        std::regex mMatchParameterRegex("(.*?)\\((.*?)\\)");

        while (std::regex_search(contentString, mMatchVariable, mMatchVariableRegex)) {
            std::string mVariableName = mMatchVariable.str(1);
            if (auto it = mVariableMap.find(mVariableName); it != mVariableMap.end()) {
                std::string mValue = getValueForVariable(mVariableName, player);
                contentString.replace(mMatchVariable.position(), mMatchVariable.length(), mValue);
                continue;
            }
            if (std::regex_match(mVariableName, mMatchParameter, mMatchParameterRegex)) {
                std::string mVariableParameterName = mMatchParameter.str(1);
                std::string mVariableParameterValue = mMatchParameter.str(2);
                if (auto it = mVariableMapParameter.find(mVariableParameterName); it != mVariableMapParameter.end()) {
                    std::string mValue = getValueForVariable(mVariableParameterName, player, mVariableParameterValue);
                    contentString.replace(mMatchVariable.position(), mMatchVariable.length(), mValue);
                    continue;
                }
            }
            contentString.replace(mMatchVariable.position(), mMatchVariable.length(), mVariableName);
        }
        return contentString;
    }

    std::string& translateString(const std::string& contentString, Player& player) {
        return translateString(const_cast<std::string&>(contentString), player);
    }
}