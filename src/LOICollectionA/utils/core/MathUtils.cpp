#include <cmath>

#include "MathUtils.h"

namespace MathUtils {
    double pow(double base, double exponent) {
        if (std::isfinite(exponent) && std::floor(exponent) == exponent && std::fabs(exponent) < 1e15) {
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

        return std::pow(base, exponent);
    }
}
