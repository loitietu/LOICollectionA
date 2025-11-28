#include <chrono>
#include <string>
#include <sstream>
#include <algorithm>
#include <unordered_set>

#include <Windows.h>

#include "LOICollectionA/utils/core/SystemUtils.h"

namespace SystemUtils {
    std::string getSystemLocaleCode(const std::string& defaultValue) {
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
        if (!GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH))
            return defaultValue;
        
        if (const int size = WideCharToMultiByte(CP_UTF8, 0, localeName, -1, nullptr, 0, nullptr, nullptr); size > 0) {
            std::string result(size, '\0');
            if (!WideCharToMultiByte(CP_UTF8, 0, localeName, -1, result.data(), size, nullptr, nullptr))
                return defaultValue;

            result.resize(size - 1);
            std::replace(result.begin(), result.end(), '-', '_');
            
            return result;
        }
        return defaultValue;
    }

    std::string getCurrentTimestamp() {
        auto mTimeNow = std::chrono::high_resolution_clock::now();
        return std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(mTimeNow.time_since_epoch()).count());
    }

    std::string getNowTime(const std::string& format) {
        auto mTimeNow = std::chrono::system_clock::now();
        auto mTimeNowSecond = std::chrono::floor<std::chrono::seconds>(mTimeNow); 
        auto zt = std::chrono::zoned_time(std::chrono::current_zone(), mTimeNowSecond);
        return std::vformat("{:" + format + "}", std::make_format_args(zt));
    }

    std::string toFormatTime(const std::string& str, const std::string& defaultValue) {
        std::chrono::local_seconds mTp;
        if ((std::istringstream(str) >> std::chrono::parse("%Y%m%d%H%M%S", mTp)).fail())
            return defaultValue;
        
        return std::format("{:%Y-%m-%d %H:%M:%S}", mTp);
    }

    std::string toTimeCalculate(const std::string& str, int hours, const std::string& defaultValue) {
        std::chrono::local_seconds mTp;
        if ((std::istringstream(str) >> std::chrono::parse("%Y-%m-%d %H:%M:%S", mTp)).fail())
            return defaultValue;
        
        mTp += std::chrono::hours(hours);
        return std::format("{:%Y%m%d%H%M%S}", mTp);
    }

    std::vector<std::string> getIntersection(const std::vector<std::vector<std::string>>& elements) {
        auto it = std::min_element(elements.begin(), elements.end(), [](const auto& left, const auto& right) -> bool {
            return left.size() < right.size();
        });

        std::unordered_set<std::string> mCommonSet(it->begin(), it->end());
        for (auto iter = std::next(elements.begin()); iter != elements.end() && !mCommonSet.empty(); ++iter) {
            const std::unordered_set<std::string> mCurrentSet(iter->begin(), iter->end());
            std::erase_if(mCommonSet, [&](const auto& element) -> bool {
                return !mCurrentSet.contains(element);
            });
        }

        return { mCommonSet.begin(), mCommonSet.end() };
    }

    int toInt(const std::string& str, int defaultValue) {
        char* endpt{};
        long result = std::strtol(str.c_str(), &endpt, 10);
        return (endpt == str.c_str()) ? defaultValue : static_cast<int>(result);
    }

    bool isPastOrPresent(const std::string& str) {
        std::chrono::local_seconds mLocalTp;
        if ((std::istringstream(str) >> std::chrono::parse("%Y%m%d%H%M%S", mLocalTp)).fail())
            return false;
        
        auto mTargetSysTp = std::chrono::current_zone()->to_sys(mLocalTp);
        auto mNowSysTp = std::chrono::system_clock::now();
        return mTargetSysTp <= mNowSysTp;
    }
}