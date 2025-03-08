#include <chrono>
#include <string>
#include <sstream>
#include <algorithm>

#include <Windows.h>

#include "SystemUtils.h"

namespace SystemUtils {
    std::string getSystemLocaleCode() {
        wchar_t buf[LOCALE_NAME_MAX_LENGTH]{};
        if (!GetUserDefaultLocaleName(buf, LOCALE_NAME_MAX_LENGTH))
            return "";

        int mSize = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
        if (!mSize)
            return "";

        std::string locale(mSize - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, &locale[0], mSize, nullptr, nullptr);
        std::replace(locale.begin(), locale.end(), '-', '_');
        return locale;
    }

    std::string getCurrentTimestamp() {
        auto mTimeNow = std::chrono::system_clock::now();
        return std::to_string(mTimeNow.time_since_epoch().count());
    }

    std::string getNowTime(const std::string& format) {
        auto mTimeNow = std::chrono::system_clock::now();
        auto mTimeNowSecond = std::chrono::floor<std::chrono::seconds>(mTimeNow); 
        auto zt = std::chrono::zoned_time(std::chrono::current_zone(), mTimeNowSecond);
        return std::vformat("{:" + format + "}", std::make_format_args(zt));
    }

    std::string formatDataTime(const std::string& timeString) {
        if (timeString.size() != 14)
            return "None";
        
        std::chrono::local_seconds mTp;
        std::istringstream iss(timeString);
        iss >> std::chrono::parse("%Y%m%d%H%M%S", mTp);
        if (iss.fail())
            return "None";
        
        return std::format("{:%Y-%m-%d %H:%M:%S}", mTp);
    }

    std::string timeCalculate(const std::string& timeString, int hours) {
        std::chrono::local_seconds mTp;
        std::istringstream iss(timeString);
        iss >> std::chrono::parse("%Y-%m-%d %H:%M:%S", mTp);
        if (iss.fail())
            return "0";
        
        mTp += std::chrono::hours(hours);
        return std::format("{:%Y%m%d%H%M%S}", mTp);
    }

    int toInt(const std::string& str, int defaultValue) {
        char* endpt{};
        long result = std::strtol(str.c_str(), &endpt, 10);
        return (endpt == str.c_str()) ? defaultValue : static_cast<int>(result);
    }

    bool isReach(const std::string& timeString) {
        if (timeString.size() != 14)
            return false;
        
        std::chrono::local_seconds mLocalTp;
        std::istringstream iss(timeString);
        iss >> std::chrono::parse("%Y%m%d%H%M%S", mLocalTp);
        if (iss.fail())
            return false;
        
        auto mTargetSysTp = std::chrono::current_zone()->to_sys(mLocalTp);
        auto mNowSysTp = std::chrono::system_clock::now();
        return mTargetSysTp <= mNowSysTp;
    }
}