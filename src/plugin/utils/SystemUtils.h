#ifndef SYSTEMUTILS_H
#define SYSTEMUTILS_H

#include <string>

namespace SystemUtils {
    std::string getSystemLocaleCode();
    std::string getNowTime(const std::string& format = "%Y-%m-%d %H:%M:%S");
    std::string formatDataTime(const std::string& timeString);
    std::string timeCalculate(const std::string& timeString, int hours);

    template <typename T, class Func>
    T nest(Func func, T value, int loop = 0) {
        T ref = value;
        if(func(ref, loop))
            return nest(func, value, ++loop);
        return ref;
    };

    int toInt(const std::string& str, int defaultValue = 0);
        
    bool isReach(const std::string& timeString);
}

#endif