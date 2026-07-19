#include <gtest/gtest.h>

#include "common/frontend/CommonTest.h"

using namespace LOICollection::frontend;

TEST(VariableTest, Evaluation) { 
    EXPECT_EQ(eval("a = 5; a"), "5");
    EXPECT_EQ(eval("a = 5; b = a; b"), "5");
    EXPECT_EQ(eval("a = \"test\"; a"), "test");
    EXPECT_EQ(eval("a = \"test\"; b = a + \"1\"; b"), "test1");
    EXPECT_EQ(eval("a = true; a"), "true");
    EXPECT_EQ(eval("a = 3.1400; a"), "3.14");
}

TEST(VariableTest, Calculate) { 
    EXPECT_EQ(eval("a = 5; b = 10; a + b"), "15");
    EXPECT_EQ(eval("a = 5; b = 10; a - b"), "-5");
    EXPECT_EQ(eval("x = 5; y = x + 5; y"), "10");
}
