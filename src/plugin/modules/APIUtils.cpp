#include <regex>
#include <chrono>
#include <string>
#include <functional>
#include <unordered_map>

#include <magic_enum.hpp>

#include <ll/api/Versions.h>
#include <ll/api/memory/Memory.h>
#include <ll/api/service/Bedrock.h>

#include <mc/util/ProfilerLite.h>
#include <mc/world/level/Level.h>
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
        registerVariable("mcVersion", [](Player& /*unused*/) {
            return ll::getGameVersion().to_string();
        });
        registerVariable("llVersion", [](Player& /*unused*/) {
            return ll::getLoaderVersion().to_string();
        });
        registerVariable("protocolVersion", [](Player& /*unused*/) {
            return std::to_string(ll::getNetworkProtocolVersion());
        });
        registerVariable("title", [](Player& player) {
            Plugins::chat::update(player);
            return Plugins::chat::getTitle(player);
        });
        registerVariable("title.time", [](Player& player) {
            Plugins::chat::update(player);
            return SystemUtils::formatDataTime(Plugins::chat::getTitleTime(player, Plugins::chat::getTitle(player)));
        });
        registerVariable("pvp", [](Player& player) {
            return Plugins::pvp::isEnable(player) ? "true" : "false";
        });
        registerVariable("mute", [](Player& player) {
            return Plugins::mute::isMute(player) ? "true" : "false";
        });
        registerVariable("language", [](Player& player) { 
            return Plugins::language::getLanguage(player);
        });
        registerVariable("languageName", [](Player& player) {
            return I18nUtils::tr(Plugins::language::getLanguage(player), "name");
        });
        registerVariable("tps", [](Player& /*unused*/) {
            double mMspt = ((double) ProfilerLite::gProfilerLiteInstance.getServerTickTime().count() / 1000000.0);
            return std::to_string(mMspt <= 50.0 ? 20.0 : (double)(1000.0 / mMspt));
        });
        registerVariable("mspt", [](Player& /*unused*/) { 
            return std::to_string((double) ProfilerLite::gProfilerLiteInstance.getServerTickTime().count() / 1000000.0);
        });
        registerVariable("time", [](Player& /*unused*/) {
            return SystemUtils::getNowTime();
        });
        registerVariable("player", [](Player& player) {
            return player.getName();
        });
        registerVariable("maxPlayer", [](Player& /*unused*/) {
            return std::to_string(ll::memory::dAccess<int>(ll::service::getServerNetworkHandler().as_ptr(), 200 * 4));
        });
        registerVariable("onlinePlayer", [](Player& /*unused*/) {
            return std::to_string(ll::service::getLevel()->getActivePlayerCount());
        });
        registerVariable("pos", [](Player& player) {
            return player.getPosition().toString();
        });
        registerVariable("blockPos", [](Player& player) {
            return player.getEyePos().toString();
        });
        registerVariable("lastDeathPos", [](Player& player) {
            return player.getLastDeathPos()->toString();
        });
        registerVariable("realName", [](Player& player) {
            return player.getRealName();
        });
        registerVariable("xuid", [](Player& player) {
            return player.getXuid();
        });
        registerVariable("uuid", [](Player& player) {
            return player.getUuid().asString();
        });
        registerVariable("canFly", [](Player& player) {
            return player.canFly() ? "true" : "false";
        });
        registerVariable("maxHealth", [](Player& player) {
            return std::to_string(player.getMaxHealth());
        });
        registerVariable("health", [](Player& player) {
            return std::to_string(player.getHealth());
        });
        registerVariable("speed", [](Player& player) {
            return std::to_string(player.getSpeed());
        });
        registerVariable("direction", [](Player& player) {
            return std::to_string(player.getDirection());
        });
        registerVariable("dimension", [](Player& player) {
            return std::to_string(player.getDimensionId());
        });
        registerVariable("os", [](Player& player) {
            return std::string(magic_enum::enum_name(player.getPlatform()));
        });
        registerVariable("ip", [](Player& player) {
            return player.getIPAndPort();
        });
        registerVariable("xp", [](Player& player) {
            return std::to_string(player.getXpEarnedAtCurrentLevel());
        });
        registerVariable("HandItem", [](Player& player) {
            return player.getCarriedItem().getName();
        });
        registerVariable("OffHand", [](Player& player) {
            return player.getOffhandSlot().getName();
        });
        registerVariable("ms", [](Player& player) {
            return std::to_string(player.getNetworkStatus()->mAveragePing);
        });
        registerVariable("avgms", [](Player& player) {
            return std::to_string(player.getNetworkStatus()->mCurrentPing);
        });
        registerVariable("Packet", [](Player& player) {
            return std::to_string(player.getNetworkStatus()->mAveragePacketLoss);
        });
        registerVariable("avgPacket", [](Player& player) {
            return std::to_string(player.getNetworkStatus()->mCurrentPacketLoss);
        });
        registerVariableParameter("score", [](Player& player, std::string name) {
            return std::to_string(McUtils::scoreboard::getScore(player, name));
        });
        registerVariableParameter("tr", [](Player& player, std::string name) {
            return I18nUtils::tr(Plugins::language::getLanguage(player), name);
        });
    }

    void registerVariable(const std::string& name, const std::function<std::string(Player&)> callback) {
        if (mVariableMap.find(name) != mVariableMap.end())
            return;
        mVariableMap[name] = callback;
    }

    void registerVariableParameter(const std::string& name, const std::function<std::string(Player&, std::string)> callback) {
        if (mVariableMapParameter.find(name) != mVariableMapParameter.end())
            return;
        mVariableMapParameter[name] = callback;
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