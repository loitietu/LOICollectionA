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

std::unordered_map<std::string, std::function<std::string(void*)>> mVariableMap;
std::unordered_map<std::string, std::function<std::string(void*, std::string)>> mVariableMapParameter;

namespace LOICollection::LOICollectionAPI {
    void initialization() {
        registerVariable("mcVersion", [](void* /*unused*/) { return ll::getGameVersion().to_string(); });
        registerVariable("llVersion", [](void* /*unused*/) { return ll::getLoaderVersion().to_string(); });
        registerVariable("protocolVersion", [](void* /*unused*/) { return std::to_string(ll::getNetworkProtocolVersion()); });
        registerVariable("title", [](void* player_ptr) {
            Plugins::chat::update(player_ptr);
            return Plugins::chat::getTitle(player_ptr);
        });
        registerVariable("title.time", [](void* player_ptr) {
            Plugins::chat::update(player_ptr);
            return SystemUtils::formatDataTime(Plugins::chat::getTitleTime(player_ptr, Plugins::chat::getTitle(player_ptr)));
        });
        registerVariable("pvp", [](void* player_ptr) { return Plugins::pvp::isEnable(player_ptr) ? "true" : "false"; });
        registerVariable("mute", [](void* player_ptr) { return Plugins::mute::isMute(player_ptr) ? "true" : "false"; });
        registerVariable("language", [](void* player_ptr) { return Plugins::language::getLanguage(player_ptr); });
        registerVariable("languageName", [](void* player_ptr) { return I18nUtils::tr(Plugins::language::getLanguage(player_ptr), "name"); });
        registerVariable("tps", [](void* /*unused*/) {
            double mMspt = ((double) ProfilerLite::gProfilerLiteInstance.getServerTickTime().count() / 1000000.0);
            return std::to_string(mMspt <= 50.0 ? 20.0 : (double)(1000.0 / mMspt));
        });
        registerVariable("mspt", [](void* /*unused*/) { 
            return std::to_string((double) ProfilerLite::gProfilerLiteInstance.getServerTickTime().count() / 1000000.0);
        });
        registerVariable("time", [](void* /*unused*/) { return SystemUtils::getNowTime(); });
        registerVariable("player", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->getName();
        });
        registerVariable("maxPlayer", [](void* /*unused*/) {
            return std::to_string(ll::memory::dAccess<int>(ll::service::getServerNetworkHandler().as_ptr(), 200 * 4));
        });
        registerVariable("onlinePlayer", [](void* /*unused*/) {
            return std::to_string(ll::service::getLevel()->getActivePlayerCount());
        });
        registerVariable("pos", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->getPosition().toString();
        });
        registerVariable("blockPos", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->getEyePos().toString();
        });
        registerVariable("lastDeathPos", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->getLastDeathPos()->toString();
        });
        registerVariable("realName", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->getRealName();
        });
        registerVariable("xuid", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->getXuid();
        });
        registerVariable("uuid", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->getUuid().asString();
        });
        registerVariable("canFly", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->canFly() ? "true" : "false";
        });
        registerVariable("maxHealth", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(player->getMaxHealth());
        });
        registerVariable("health", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(player->getHealth());
        });
        registerVariable("speed", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(player->getSpeed());
        });
        registerVariable("direction", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(player->getDirection());
        });
        registerVariable("dimension", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(player->getDimensionId());
        });
        registerVariable("os", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::string(magic_enum::enum_name(player->getPlatform()));
        });
        registerVariable("ip", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->getIPAndPort();
        });
        registerVariable("xp", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(player->getXpEarnedAtCurrentLevel());
        });
        registerVariable("HandItem", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->getCarriedItem().getName();
        });
        registerVariable("OffHand", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return player->getOffhandSlot().getName();
        });
        registerVariable("ms", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(player->getNetworkStatus()->mAveragePing);
        });
        registerVariable("avgms", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(player->getNetworkStatus()->mCurrentPing);
        });
        registerVariable("Packet", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(player->getNetworkStatus()->mAveragePacketLoss);
        });
        registerVariable("avgPacket", [](void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(player->getNetworkStatus()->mCurrentPacketLoss);
        });
        registerVariableParameter("score", [](void* player_ptr, std::string name) {
            Player* player = static_cast<Player*>(player_ptr);
            return std::to_string(McUtils::scoreboard::getScore(player, name));
        });
        registerVariableParameter("tr", [](void* player_ptr, std::string name) {
            return I18nUtils::tr(Plugins::language::getLanguage(player_ptr), name);
        });
    }

    void registerVariable(const std::string& name, const std::function<std::string(void*)> callback) {
        if (mVariableMap.find(name) != mVariableMap.end())
            return;
        mVariableMap[name] = callback;
    }

    void registerVariableParameter(const std::string& name, const std::function<std::string(void*, std::string)> callback) {
        if (mVariableMapParameter.find(name) != mVariableMapParameter.end())
            return;
        mVariableMapParameter[name] = callback;
    }

    std::string getValueForVariable(const std::string& name, void* player_ptr) {
        if (auto it = mVariableMap.find(name); it != mVariableMap.end()) {
            try {
                return it->second(player_ptr);
            } catch (...) {};
        }
        return "None";
    }

    std::string getValueForVariable(const std::string& name, void* player_ptr, const std::string& parameter) {
        if (auto it = mVariableMapParameter.find(name); it != mVariableMapParameter.end()) {
            try {
                return it->second(player_ptr, parameter);
            } catch (...) {};
        }
        return "None";
    }

    std::string& translateString(std::string& contentString, void* player_ptr) {
        if (contentString.empty())
            return contentString;

        std::smatch mMatchVariable;
        std::smatch mMatchParameter;
        std::regex mMatchVariableRegex("\\{(.*?)\\}");
        std::regex mMatchParameterRegex("(.*?)\\((.*?)\\)");

        while (std::regex_search(contentString, mMatchVariable, mMatchVariableRegex)) {
            std::string mVariableName = mMatchVariable.str(1);
            if (auto it = mVariableMap.find(mVariableName); it != mVariableMap.end()) {
                std::string mValue = getValueForVariable(mVariableName, player_ptr);
                contentString.replace(mMatchVariable.position(), mMatchVariable.length(), mValue);
                continue;
            }
            if (std::regex_match(mVariableName, mMatchParameter, mMatchParameterRegex)) {
                std::string mVariableParameterName = mMatchParameter.str(1);
                std::string mVariableParameterValue = mMatchParameter.str(2);
                if (auto it = mVariableMapParameter.find(mVariableParameterName); it != mVariableMapParameter.end()) {
                    std::string mValue = getValueForVariable(mVariableParameterName, player_ptr, mVariableParameterValue);
                    contentString.replace(mMatchVariable.position(), mMatchVariable.length(), mValue);
                    continue;
                }
            }
            contentString.replace(mMatchVariable.position(), mMatchVariable.length(), mVariableName);
        }
        return contentString;
    }

    std::string& translateString(const std::string& contentString, void* player_ptr) {
        return translateString(const_cast<std::string&>(contentString), player_ptr);
    }
}