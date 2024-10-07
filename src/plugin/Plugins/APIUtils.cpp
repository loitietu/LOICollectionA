#include <regex>
#include <chrono>
#include <string>
#include <functional>
#include <unordered_map>

#include <ll/api/Versions.h>
#include <ll/api/memory/Memory.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/util/ProfilerLite.h>
#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>
#include <mc/network/ServerNetworkHandler.h>

#include "Include/pvpPlugin.h"
#include "Include/mutePlugin.h"
#include "Include/chatPlugin.h"
#include "Include/languagePlugin.h"

#include "Utils/I18nUtils.h"
#include "Utils/toolUtils.h"

#include "Include/APIUtils.h"

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
            return toolUtils::System::formatDataTime(Plugins::chat::getTitleTime(player_ptr, Plugins::chat::getTitle(player_ptr)));
        });
        registerVariable("pvp", [](void* player_ptr) { return Plugins::pvp::isEnable(player_ptr) ? "true" : "false"; });
        registerVariable("mute", [](void* player_ptr) { return Plugins::mute::isMute(player_ptr) ? "true" : "false"; });
        registerVariable("language", [](void* player_ptr) { return Plugins::language::getLanguage(player_ptr); });
        registerVariable("tps", [](void* /*unused*/) {
            double mMspt = ((double) ProfilerLite::gProfilerLiteInstance.getServerTickTime().count() / 1000000.0);
            return std::to_string(mMspt <= 50.0 ? 20.0 : (double)(1000.0 / mMspt));
        });
        registerVariable("mspt", [](void* /*unused*/) { 
            return std::to_string((double) ProfilerLite::gProfilerLiteInstance.getServerTickTime().count() / 1000000.0);
        });
        registerVariable("time", [](void* /*unused*/) { return toolUtils::System::getNowTime(); });
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
            return toolUtils::Mc::getDevice(player);
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
            return std::to_string(toolUtils::scoreboard::getScore(player, name));
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

    std::string& translateString(std::string& contentString, void* player_ptr) {
        if (contentString.empty())
            return contentString;

        std::smatch mMatchFind;
        std::regex mPatternFind("\\{(.*?)\\}");
        if (!std::regex_search(contentString, mMatchFind, mPatternFind))
            return contentString;

        for (auto& [name, callback] : mVariableMap) {
            if (contentString.find("{" + name + "}") == std::string::npos)
                continue;
            try {
                ll::string_utils::replaceAll(contentString, "{" + name + "}", callback(player_ptr));
            } catch (...) { ll::string_utils::replaceAll(contentString, "{" + name + "}", "None"); };
        }

        std::smatch mMatchParameter;
        for (auto& [name, callback] : mVariableMapParameter) {
            std::regex mPatternParameter("\\{" + name + "\\((.*?)\\)\\}");
            while (std::regex_search(contentString, mMatchParameter, mPatternParameter)) {
                std::string extractedContent = mMatchParameter.str(1);
                try {
                    ll::string_utils::replaceAll(contentString, "{" + name + "(" + extractedContent + ")}", callback(player_ptr, extractedContent));
                } catch (...) { ll::string_utils::replaceAll(contentString, "{" + name + "(" + extractedContent + ")}", "None"); };
            }
        }
        return contentString;
    }

    std::string& translateString(const std::string& contentString, void* player_ptr) {
        return translateString(const_cast<std::string&>(contentString), player_ptr);
    }
}