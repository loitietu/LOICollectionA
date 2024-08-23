#include <vector>
#include <string>
#include <sstream>
#include <cstddef>

#include <ll/api/Mod/Manifest.h>
#include <ll/api/Mod/NativeMod.h>
#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/scores/Objective.h>
#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>
#include <mc/world/actor/player/Player.h>
#include <mc/network/ConnectionRequest.h>
#include <mc/enums/BuildPlatform.h>

#include "../ConfigPlugin.h"

#include "toolUtils.h"

namespace toolUtils {
    namespace {
        ll::mod::Manifest manifestPlugin;
    }

    void init(void* mSelfPtr) { manifestPlugin = static_cast<ll::mod::NativeMod*>(mSelfPtr)->getManifest(); }

    void SynchronousPluginConfigVersion(void* config_ptr) {
        C_Config* config = static_cast<C_Config*>(config_ptr);

        std::stringstream ss;
        std::vector<std::string> version = split(getVersion(), '.');
        for (size_t i = 0; i < version.size(); i++) {
            ss << version[i];
        }
        config->version = std::stoi(ss.str());
        ss.clear();
    }

    std::string getVersion() {
        return manifestPlugin.version->to_string();
    }

    std::string getDevice(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        if (!player->isSimulated()) {
            auto request = player->getConnectionRequest();
            if (request) {
                switch (request->getDeviceOS()) {
                    case BuildPlatform::Android:
                        return "android";
                    case BuildPlatform::iOS:
                        return "ios";
                    case BuildPlatform::Amazon:
                        return "amazon";
                    case BuildPlatform::Linux:
                        return "linux";
                    case BuildPlatform::Xbox:
                        return "xbox";
                    case BuildPlatform::Dedicated:
                        return "dedicated";
                    case BuildPlatform::GearVR:
                        return "gearvr";
                    case BuildPlatform::Nx:
                        return "nx";
                    case BuildPlatform::OSX:
                        return "osx";
                    case BuildPlatform::PS4:
                        return "ps4";
                    case BuildPlatform::WindowsPhone:
                        return "windowsphone";
                    case BuildPlatform::Win32:
                        return "win32";
                    case BuildPlatform::UWP:
                        return "uwp";
                    default:
                        return "unknown";
                }
            }
        }
        return "unknown";
    }

    std::string replaceString(std::string str, const std::string& from, const std::string& to) {
        for (std::string::size_type pos(0); pos != std::string::npos; pos += to.length()) {
            if ((pos = str.find(from, pos)) != std::string::npos) {
                str.replace(pos, from.length(), to);
            } else break;
        }
        return str;
    }

    std::string timeCalculate(int hours) {
        if (hours > 0) {
            std::time_t currentTime = std::time(nullptr);
            std::tm* timeInfo = std::localtime(&currentTime);
            timeInfo->tm_hour += hours;
            std::time_t laterTime = std::mktime(timeInfo);
            char formattedTime[15];
            std::strftime(formattedTime, sizeof(formattedTime), "%Y%m%d%H%M%S", std::localtime(&laterTime));
            std::string formattedTimeString(formattedTime);
            return formattedTimeString;
        }
        return "0";
    }

    std::vector<std::string> split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(s);
        std::string token;
        while (std::getline(ss, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    bool isReach(const std::string& timeString) {
        if (timeString != "0") {
            std::time_t currentTime = std::time(nullptr);
            char formattedTime[15];
            std::strftime(formattedTime, sizeof(formattedTime), "%Y%m%d%H%M%S", std::localtime(&currentTime));
            std::string formattedTimeString(formattedTime);
            int64_t formattedTimeInt = std::stoll(formattedTimeString);
            int64_t timeInt = std::stoll(timeString);
            if (formattedTimeInt > timeInt) return true;
            else return false;
        }
        return false;
    }

    namespace scoreboard {
        int getScore(void* player_ptr, const std::string& name) {
            Player* player = static_cast<Player*>(player_ptr);
            auto mScoreboardId(ll::service::getLevel()->getScoreboard().getScoreboardId(*player));
            if (mScoreboardId.isValid()) {
                auto obj(ll::service::getLevel()->getScoreboard().getObjective(name));
                auto scores(ll::service::getLevel()->getScoreboard().getIdScores(mScoreboardId));
                for (auto& score : scores) {
                    if (score.mObjective == obj) {
                        return score.mScore;
                    }
                }
            }
            return 0;
        }
    }
}
