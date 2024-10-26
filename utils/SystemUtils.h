#ifndef SYSTEMUTILS_H
#define SYSTEMUTILS_H

#include <string>

namespace SystemUtils {
    std::string getNowTime();
    std::string timeCalculate(const std::string& timeString, int hours); 
    std::string formatDataTime(const std::string& timeString);

    int toInt(const std::string& str, int defaultValue = 0);
        
    bool isReach(const std::string& timeString);
}

#endif