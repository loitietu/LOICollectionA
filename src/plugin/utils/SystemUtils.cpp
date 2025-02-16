#include <ctime>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <Windows.h>

#include "SystemUtils.h"

namespace SystemUtils {
    std::string getSystemLocaleCode() {
        wchar_t buf[LOCALE_NAME_MAX_LENGTH]{};
        if (!GetUserDefaultLocaleName(buf, LOCALE_NAME_MAX_LENGTH))
            return "";

        int mSize = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
        if (!mSize)
            return "";

        std::string locale(mSize - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, &locale[0], mSize, NULL, NULL);
        std::replace(locale.begin(), locale.end(), '-', '_');
        return locale;
    }

    std::string getCurrentTimestamp() {
        std::time_t now = std::time(nullptr);
        return std::to_string(now);
    }

    std::string getNowTime(const std::string& format) {
        std::time_t currentTime = std::time(nullptr);
        std::tm currentTimeInfo{};
        localtime_s(&currentTimeInfo, &currentTime);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), format.c_str(), &currentTimeInfo);
        return buffer;
    }

    std::string formatDataTime(const std::string& timeString) {
        if (timeString.size() != 14) 
            return "None";
        return timeString.substr(0, 4) + "-" + timeString.substr(4, 2) + "-" + timeString.substr(6, 2) + " " +
            timeString.substr(8, 2) + ":" + timeString.substr(10, 2) + ":" + timeString.substr(12, 2);
    }

    std::string timeCalculate(const std::string& timeString, int hours) {
        std::tm tm = {};
        std::istringstream iss(timeString);
        iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        tm.tm_hour += hours;
        std::mktime(&tm);
        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", &tm);
        return buffer;
    }

    int toInt(const std::string& str, int defaultValue) {
        char* endpt{};
        long result = std::strtol(str.c_str(), &endpt, 10);
        return (endpt == str.c_str()) ? defaultValue : static_cast<int>(result);
    }

    bool isReach(const std::string& timeString) {
        if (timeString.size() != 14)
            return false;
        std::tm tm = {};
        std::istringstream iss(timeString);
        iss >> std::get_time(&tm, "%Y%m%d%H%M%S");
        if (iss.fail())
            return false;
        std::time_t targetTime = std::mktime(&tm);
        return std::difftime(targetTime, std::time(nullptr)) <= 0;
    }
}