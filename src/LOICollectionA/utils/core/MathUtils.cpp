#include "MathUtils.h"

namespace MathUtils {
    double pow(double base, double exponent) {
        auto exp = static_cast<long long>(exponent);
        if (exp < 0) {
            base = 1.0 / base;
            exp = -exp;
        }

        double result = 1.0;
        while (exp > 0) {
            if (exp & 1)
                result *= base;
            
            base *= base;
            exp >>= 1;
        }

        return result;
    }
}
