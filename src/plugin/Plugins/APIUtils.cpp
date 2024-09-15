#include <regex>
#include <string>

#include <ll/api/schedule/Task.h>
#include <ll/api/schedule/Scheduler.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/world/actor/player/Player.h>

#include "Include/pvpPlugin.h"
#include "Include/mutePlugin.h"
#include "Include/chatPlugin.h"
#include "Include/languagePlugin.h"

#include "Utils/toolUtils.h"

#include "Include/APIUtils.h"

using namespace ll::chrono_literals;

int mTicksPerSecond;
float mTicksPerMinute;
unsigned short mTicks;
std::vector<unsigned short> mTicksList;

namespace LOICollectionAPI {
    void initialization() {
        static ll::schedule::GameTickAsyncScheduler scheduler1;
        static ll::schedule::ServerTimeAsyncScheduler scheduler2;
        scheduler1.add<ll::schedule::RepeatTask>(1_tick, [] {
            mTicks++;
        });
        scheduler2.add<ll::schedule::RepeatTask>(1s, [] {
            if (mTicks > 20)
                mTicks = 20;
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
    }

    void translateString2(std::string& contentString, void* player_ptr, bool enable) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mChatTitle = chatPlugin::getTitle(player);

        chatPlugin::update(player);
        ll::string_utils::replaceAll(contentString, "{title}", mChatTitle);
        ll::string_utils::replaceAll(contentString, "{title.time}", chatPlugin::getTitleTime(player, mChatTitle));
        ll::string_utils::replaceAll(contentString, "{pvp}", pvpPlugin::isEnable(player) ? "true" : "false");
        ll::string_utils::replaceAll(contentString, "{mute}", mutePlugin::isMute(player) ? "true" : "false");
        ll::string_utils::replaceAll(contentString, "{language}", languagePlugin::getLanguage(player));
        ll::string_utils::replaceAll(contentString, "{tps}", std::to_string(mTicksPerSecond));
        ll::string_utils::replaceAll(contentString, "{tpm}", std::to_string(mTicksPerMinute));
        ll::string_utils::replaceAll(contentString, "{player}", player->getName());
        ll::string_utils::replaceAll(contentString, "{pos}", player->getPosition().toString());
        ll::string_utils::replaceAll(contentString, "{blockPos}", player->getEyePos().toString());
        try {
            ll::string_utils::replaceAll(contentString, "{lastDeathPos}", player->getLastDeathPos()->toString());
        } catch (...) {
            ll::string_utils::replaceAll(contentString, "{lastDeathPos}", "NULL");
        };
        ll::string_utils::replaceAll(contentString, "{realName}", player->getRealName());
        ll::string_utils::replaceAll(contentString, "{xuid}", player->getXuid());
        ll::string_utils::replaceAll(contentString, "{uuid}", player->getUuid().asString());
        ll::string_utils::replaceAll(contentString, "{canFly}", player->canFly() ? "true" : "false");
        ll::string_utils::replaceAll(contentString, "{maxHealth}", std::to_string(player->getMaxHealth()));
        ll::string_utils::replaceAll(contentString, "{health}", std::to_string(player->getHealth()));
        ll::string_utils::replaceAll(contentString, "{speed}", std::to_string(player->getSpeed()));
        ll::string_utils::replaceAll(contentString, "{direction}", std::to_string(player->getDirection()));
        ll::string_utils::replaceAll(contentString, "{dimension}", std::to_string(player->getDimensionId()));
        ll::string_utils::replaceAll(contentString, "{os}", toolUtils::getDevice(player));
        ll::string_utils::replaceAll(contentString, "{ip}", player->getIPAndPort());
        ll::string_utils::replaceAll(contentString, "{xp}", std::to_string(player->getXpEarnedAtCurrentLevel()));
        ll::string_utils::replaceAll(contentString, "{HandItem}", player->getSelectedItem().getName());
        ll::string_utils::replaceAll(contentString, "{OffHand}", player->getOffhandSlot().getName());
        if (enable) {
            ll::string_utils::replaceAll(contentString, "{ms}", std::to_string(player->getNetworkStatus()->mAveragePing));
            ll::string_utils::replaceAll(contentString, "{avgms}", std::to_string(player->getNetworkStatus()->mCurrentPing));
            ll::string_utils::replaceAll(contentString, "{Packet}", std::to_string(player->getNetworkStatus()->mAveragePacketLoss));
            ll::string_utils::replaceAll(contentString, "{avgPacket}", std::to_string(player->getNetworkStatus()->mCurrentPacketLoss));
        }
        std::smatch match;
        std::regex pattern("\\{score\\((.*?)\\)\\}");
        while (std::regex_search(contentString, match, pattern)) {
            std::string extractedContent = match.str(1);
            int score = toolUtils::scoreboard::getScore(player, extractedContent);
            contentString = std::regex_replace(contentString, pattern, std::to_string(score));
        }
    }

    std::string translateString(std::string contentString, void* player_ptr, bool enable) {
        translateString2(contentString, player_ptr, enable);
        return contentString;
    }
}