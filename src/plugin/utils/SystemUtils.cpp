#include <ctime>
#include <string>
#include <sstream>
#include <iomanip>

#include <ll/api/utils/HashUtils.h>
#include <ll/api/utils/StringUtils.h>

#include <fmt/core.h>
#include <fmt/color.h>

#include "SystemUtils.h"

namespace SystemUtils {
    std::string getColorCode(std::string color) {
        switch (ll::hash_utils::doHash(color)) {
            case ll::hash_utils::doHash("0"):
                return fmt::format(fmt::fg(fmt::color::black), "{}", "");
            case ll::hash_utils::doHash("1"):
                return fmt::format(fmt::fg(fmt::color::dark_blue), "{}", "");
            case ll::hash_utils::doHash("2"):
                return fmt::format(fmt::fg(fmt::color::dark_green), "{}", "");
            case ll::hash_utils::doHash("3"):
                return fmt::format(fmt::fg(fmt::color::alice_blue), "{}", "");
            case ll::hash_utils::doHash("4"):
                return fmt::format(fmt::fg(fmt::color::dark_red), "{}", "");
            case ll::hash_utils::doHash("5"):
                return fmt::format(fmt::fg(fmt::color::medium_purple), "{}", "");
            case ll::hash_utils::doHash("6"):
                return fmt::format(fmt::fg(fmt::color::gold), "{}", "");
            case ll::hash_utils::doHash("7"):
                return fmt::format(fmt::fg(fmt::color::gray), "{}", "");
            case ll::hash_utils::doHash("8"):
                return fmt::format(fmt::fg(fmt::color::dark_gray), "{}", "");
            case ll::hash_utils::doHash("9"):
                return fmt::format(fmt::fg(fmt::color::blue), "{}", "");
            case ll::hash_utils::doHash("a"):
                return fmt::format(fmt::fg(fmt::color::green), "{}", "");
            case ll::hash_utils::doHash("b"):
                return fmt::format(fmt::fg(fmt::color::aqua), "{}", "");
            case ll::hash_utils::doHash("c"):
                return fmt::format(fmt::fg(fmt::color::red), "{}", "");
            case ll::hash_utils::doHash("d"):
                return fmt::format(fmt::fg(fmt::color::purple), "{}", "");
            case ll::hash_utils::doHash("e"):
                return fmt::format(fmt::fg(fmt::color::yellow), "{}", "");
            case ll::hash_utils::doHash("f"):
                return fmt::format(fmt::fg(fmt::color::white), "{}", "");
            case ll::hash_utils::doHash("g"):
                return fmt::format(fmt::fg(fmt::color::golden_rod), "{}", "");
        };
        return "";
    }

    std::string getNowTime(const std::string& format) {
        std::tm currentTimeInfo;
        std::time_t currentTime = std::time(nullptr);
        localtime_s(&currentTimeInfo, &currentTime);
        std::ostringstream oss;
        oss << std::put_time(&currentTimeInfo, format.c_str());
        return oss.str();
    }

    std::string timeCalculate(const std::string& timeString, int hours) {
        std::tm tm = {};
        std::istringstream iss(timeString);
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        tm.tm_hour += hours;
        std::mktime(&tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d%H%M%S");
        return oss.str();
    }

    std::string formatDataTime(const std::string& timeString) {
        if (timeString.size() != 14) 
            return "None";
        std::string fomatted;
        fomatted += timeString.substr(0, 4) + "-";
        fomatted += timeString.substr(4, 2) + "-";
        fomatted += timeString.substr(6, 2) + " ";
        fomatted += timeString.substr(8, 2) + ":";
        fomatted += timeString.substr(10, 2) + ":";
        fomatted += timeString.substr(12, 2);
        return fomatted;
    }

    int toInt(const std::string& str, int defaultValue) {
        auto result = ll::string_utils::svtoi(str);
        return result.has_value() ? result.value() : defaultValue;
    }

    bool isReach(const std::string& timeString) {
        if (timeString == "0")
            return false;
        return std::stoll(getNowTime("%Y%m%d%H%M%S")) > std::stoll(timeString);
    }
}