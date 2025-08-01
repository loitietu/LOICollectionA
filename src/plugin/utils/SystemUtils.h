#pragma once

#include <string>
#include <vector>

namespace SystemUtils {
    std::string getSystemLocaleCode(const std::string& defaultValue = "");
    std::string getCurrentTimestamp();
    std::string getNowTime(const std::string& format = "%Y-%m-%d %H:%M:%S");
    std::string formatDataTime(const std::string& timeString, const std::string& defaultValue = "");
    std::string timeCalculate(const std::string& timeString, int hours, const std::string& defaultValue = "");

    std::vector<std::string> getIntersection(const std::vector<std::vector<std::string>>& elements);

    int toInt(const std::string& str, int defaultValue = 0);
        
    bool isReach(const std::string& timeString);
}