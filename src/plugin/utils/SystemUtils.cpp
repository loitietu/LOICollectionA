#include <ctime>
#include <cstdlib>

#include <string>
#include <sstream>
#include <iomanip>

#include "SystemUtils.h"

namespace SystemUtils {
    std::string getNowTime(const std::string& format) {
        std::time_t currentTime = std::time(nullptr);
        std::tm currentTimeInfo;
        localtime_s(&currentTimeInfo, &currentTime);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), format.c_str(), &currentTimeInfo);
        return std::string(buffer);
    }


    std::string timeCalculate(const std::string& timeString, int hours) {
        std::tm tm = {};
        std::istringstream iss(timeString);
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        tm.tm_hour += hours;
        std::mktime(&tm);
        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", &tm);
        return std::string(buffer);
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
        const char* ptr = str.data();
        char* endpt{};
        auto result = std::strtol(ptr, &endpt, 10);
        if (endpt == ptr)
            return defaultValue;
        return result;
    }

    bool isReach(const std::string& timeString) {
        if (timeString == "0")
            return false;
        return std::stoll(getNowTime("%Y%m%d%H%M%S")) > std::stoll(timeString);
    }
}