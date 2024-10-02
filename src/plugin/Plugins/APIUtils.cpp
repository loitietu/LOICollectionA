#include <regex>
#include <string>
#include <functional>
#include <unordered_map>

#include <ll/api/Versions.h>
#include <ll/api/memory/Memory.h>
#include <ll/api/schedule/Task.h>
#include <ll/api/schedule/Scheduler.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/utils/StringUtils.h>

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

using namespace ll::chrono_literals;

int mTicksPerSecond;
float mTicksPerMinute;
unsigned short mTicks;
std::vector<unsigned short> mTicksList;

std::unordered_map<std::string, std::function<std::string(void*)>> mVariableMap;
std::unordered_map<std::string, std::function<std::string(void*, std::string)>> mVariableMapParameter;

namespace LOICollection::LOICollectionAPI {
    void initialization() {
        static ll::schedule::GameTickAsyncScheduler scheduler1;
        static ll::schedule::ServerTimeScheduler scheduler2;
        scheduler1.add<ll::schedule::RepeatTask>(1_tick, [] {
            mTicks++;
            if (mTicks > 20)
                mTicks = 20;
        });
        scheduler2.add<ll::schedule::RepeatTask>(1s, [] {
            mTicksList.push_back(mTicks);
            mTicksPerSecond = (int) mTicks;
            mTicks = 0;
            if (mTicksList.size() >= 60) {
                mTicksList.clear();
                return;
            }
            unsigned int sum = 0;
            for (auto& i : mTicksList)
                sum = sum + i;
            mTicksPerMinute = (float) sum / (int) mTicksList.size();
        });

        registerVariable("mcVersion", [](void* /*unused*/) { return ll::getGameVersion().to_string(); });
        registerVariable("llVersion", [](void* /*unused*/) { return ll::getLoaderVersion().to_string(); });
        registerVariable("protocolVersion", [](void* /*unused*/) { return std::to_string(ll::getNetworkProtocolVersion()); });
        registerVariable("title", [](void* player_ptr) {
            Plugins::chat::update(player_ptr);
            return Plugins::chat::getTitle(player_ptr);
        });
        registerVariable("title.time", [](void* player_ptr) {
            Plugins::chat::update(player_ptr);
            return toolUtils::formatDataTime(Plugins::chat::getTitleTime(player_ptr, Plugins::chat::getTitle(player_ptr)));
        });
        registerVariable("pvp", [](void* player_ptr) { return Plugins::pvp::isEnable(player_ptr) ? "true" : "false"; });
        registerVariable("mute", [](void* player_ptr) { return Plugins::mute::isMute(player_ptr) ? "true" : "false"; });
        registerVariable("language", [](void* player_ptr) { return Plugins::language::getLanguage(player_ptr); });
        registerVariable("tps", [](void* /*unused*/) { return std::to_string(mTicksPerSecond); });
        registerVariable("tpm", [](void* /*unused*/) { return std::to_string(mTicksPerMinute); });
        registerVariable("time", [](void* /*unused*/) { return toolUtils::getNowTime(); });
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
            return toolUtils::getDevice(player);
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