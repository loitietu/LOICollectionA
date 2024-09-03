#include <regex>
#include <string>

#include <mc/world/actor/player/Player.h>

#include "../Include/pvpPlugin.h"
#include "../Include/mutePlugin.h"
#include "../Include/chatPlugin.h"
#include "../Include/languagePlugin.h"

#include "../Utils/toolUtils.h"

#include "../Include/APIUtils.h"

namespace LOICollectionAPI {
    void translateString2(std::string& contentString, void* player_ptr, bool enable) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mChatTitle = chatPlugin::getTitle(player);

        chatPlugin::update(player);
        toolUtils::replaceString2(contentString, "{title}", mChatTitle);
        toolUtils::replaceString2(contentString, "{title.time}", chatPlugin::getTitleTime(player, mChatTitle));
        toolUtils::replaceString2(contentString, "{pvp}", pvpPlugin::isEnable(player) ? "true" : "false");
        toolUtils::replaceString2(contentString, "{mute}", mutePlugin::isMute(player) ? "true" : "false");
        toolUtils::replaceString2(contentString, "{language}", languagePlugin::getLanguage(player));
        toolUtils::replaceString2(contentString, "{player}", player->getName());
        toolUtils::replaceString2(contentString, "{pos}", player->getPosition().toString());
        toolUtils::replaceString2(contentString, "{blockPos}", player->getEyePos().toString());
        try {
            toolUtils::replaceString2(contentString, "{lastDeathPos}", player->getLastDeathPos()->toString());
        } catch (...) {
            toolUtils::replaceString2(contentString, "{lastDeathPos}", "NULL");
        };
        toolUtils::replaceString2(contentString, "{realName}", player->getRealName());
        toolUtils::replaceString2(contentString, "{xuid}", player->getXuid());
        toolUtils::replaceString2(contentString, "{uuid}", player->getUuid().asString());
        toolUtils::replaceString2(contentString, "{canFly}", player->canFly() ? "true" : "false");
        toolUtils::replaceString2(contentString, "{maxHealth}", std::to_string(player->getMaxHealth()));
        toolUtils::replaceString2(contentString, "{health}", std::to_string(player->getHealth()));
        toolUtils::replaceString2(contentString, "{speed}", std::to_string(player->getSpeed()));
        toolUtils::replaceString2(contentString, "{direction}", std::to_string(player->getDirection()));
        toolUtils::replaceString2(contentString, "{dimension}", std::to_string(player->getDimensionId()));
        toolUtils::replaceString2(contentString, "{os}", toolUtils::getDevice(player));
        toolUtils::replaceString2(contentString, "{ip}", player->getIPAndPort());
        toolUtils::replaceString2(contentString, "{xp}", std::to_string(player->getXpEarnedAtCurrentLevel()));
        toolUtils::replaceString2(contentString, "{HandItem}", player->getSelectedItem().getName());
        toolUtils::replaceString2(contentString, "{OffHand}", player->getOffhandSlot().getName());
        if (enable) {
            toolUtils::replaceString2(contentString, "{ms}", std::to_string(player->getNetworkStatus()->mAveragePing));
            toolUtils::replaceString2(contentString, "{avgms}", std::to_string(player->getNetworkStatus()->mCurrentPing));
            toolUtils::replaceString2(contentString, "{Packet}", std::to_string(player->getNetworkStatus()->mAveragePacketLoss));
            toolUtils::replaceString2(contentString, "{avgPacket}", std::to_string(player->getNetworkStatus()->mCurrentPacketLoss));
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