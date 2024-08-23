#pragma once

#include <regex>
#include <string>

#include <mc/world/actor/player/Player.h>

#include "../API/language.h"
#include "../Utils/toolUtils.h"

namespace LOICollectionAPI {
    inline std::string translateString(void* contentPtr, void* playerPtr, bool enable) {
        std::string contentString = *static_cast<std::string*>(contentPtr);
        Player* player = static_cast<Player*>(playerPtr);
        contentString = toolUtils::replaceString(contentString, "{language}", languagePlugin::getLanguage(player));
        contentString = toolUtils::replaceString(contentString, "{player}", player->getName());
        contentString = toolUtils::replaceString(contentString, "{pos}", player->getPosition().toString());
        contentString = toolUtils::replaceString(contentString, "{blockPos}", player->getEyePos().toString());
        try {
            contentString = toolUtils::replaceString(contentString, "{lastDeathPos}", player->getLastDeathPos()->toString());
        } catch (...) {
            contentString = toolUtils::replaceString(contentString, "{lastDeathPos}", "NULL");
        };
        contentString = toolUtils::replaceString(contentString, "{realName}", player->getRealName());
        contentString = toolUtils::replaceString(contentString, "{xuid}", player->getXuid());
        contentString = toolUtils::replaceString(contentString, "{uuid}", player->getUuid().asString());
        contentString = toolUtils::replaceString(contentString, "{canFly}", player->canFly() ? "true" : "false");
        contentString = toolUtils::replaceString(contentString, "{maxHealth}", std::to_string(player->getMaxHealth()));
        contentString = toolUtils::replaceString(contentString, "{health}", std::to_string(player->getHealth()));
        contentString = toolUtils::replaceString(contentString, "{speed}", std::to_string(player->getSpeed()));
        contentString = toolUtils::replaceString(contentString, "{direction}", std::to_string(player->getDirection()));
        contentString = toolUtils::replaceString(contentString, "{os}", toolUtils::getDevice(player));
        contentString = toolUtils::replaceString(contentString, "{ip}", player->getIPAndPort());
        contentString = toolUtils::replaceString(contentString, "{xp}", std::to_string(player->getXpEarnedAtCurrentLevel()));
        contentString = toolUtils::replaceString(contentString, "{HandItem}", player->getSelectedItem().getName());
        contentString = toolUtils::replaceString(contentString, "{OffHand}", player->getOffhandSlot().getName());
        if (enable) {
            contentString = toolUtils::replaceString(contentString, "{ms}", std::to_string(player->getNetworkStatus()->mAveragePing));
            contentString = toolUtils::replaceString(contentString, "{avgms}", std::to_string(player->getNetworkStatus()->mCurrentPing));
            contentString = toolUtils::replaceString(contentString, "{Packet}", std::to_string(player->getNetworkStatus()->mAveragePacketLoss));
            contentString = toolUtils::replaceString(contentString, "{avgPacket}", std::to_string(player->getNetworkStatus()->mCurrentPacketLoss));
        }
        std::smatch match;
        std::regex pattern("\\{score\\((.*?)\\)\\}");
        while (std::regex_search(contentString, match, pattern)) {
            std::string extractedContent = match.str(1);
            int score = toolUtils::scoreboard::getScore(player, extractedContent);
            contentString = std::regex_replace(contentString, pattern, std::to_string(score));
        }
        return contentString;
    }
}