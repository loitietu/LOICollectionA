#pragma once

#include <string>
#include <vector>

namespace SystemUtils {
    std::string getSystemLocaleCode(const std::string& defaultValue = "");
    std::string getCurrentTimestamp();
    std::string getNowTime(const std::string& format = "%Y-%m-%d %H:%M:%S");
    std::string toFormatTime(const std::string& str, const std::string& defaultValue = "");
    std::string toTimeCalculate(const std::string& str, int hours, const std::string& defaultValue = "");

    std::vector<std::string> getIntersection(const std::vector<std::vector<std::string>>& elements);

    int toInt(const std::string& str, int defaultValue = 0);
        
    bool isPastOrPresent(const std::string& str);
}