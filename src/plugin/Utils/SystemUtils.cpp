#include <ctime>
#include <string>
#include <sstream>
#include <iomanip>

#include <ll/api/utils/StringUtils.h>

#include "SystemUtils.h"

namespace SystemUtils {
    std::string getNowTime() {
        std::tm currentTimeInfo;
        std::time_t currentTime = std::time(nullptr);
        localtime_s(&currentTimeInfo, &currentTime);
        std::ostringstream oss;
        oss << std::put_time(&currentTimeInfo, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    std::string timeCalculate(const std::string& timeString, int hours) {
        if (!hours)
            return "0";
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
        std::string formattedTime = getNowTime();
        ll::string_utils::replaceAll(formattedTime, "-", "");
        ll::string_utils::replaceAll(formattedTime, ":", "");
        ll::string_utils::replaceAll(formattedTime, " ", "");
        return std::stoll(formattedTime) > std::stoll(timeString);
    }
}