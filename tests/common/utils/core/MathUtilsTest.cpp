#include <gtest/gtest.h>

#include "LOICollectionA/utils/core/MathUtils.h"

TEST(MathUtilsTest, PowBaseCases) {
    EXPECT_EQ(MathUtils::pow(2, 0), 1);
    EXPECT_EQ(MathUtils::pow(5, 1), 5);
    EXPECT_EQ(MathUtils::pow(3, 0), 1);
}

TEST(MathUtilsTest, PowPositiveExponent) {
    EXPECT_EQ(MathUtils::pow(2, 10), 1024);
    EXPECT_EQ(MathUtils::pow(3, 4), 81);
    EXPECT_EQ(MathUtils::pow(7, 3), 343);
    EXPECT_EQ(MathUtils::pow(10, 6), 1000000);
}
