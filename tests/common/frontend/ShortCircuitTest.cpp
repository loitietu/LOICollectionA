#include <gtest/gtest.h>

#include "LOICollectionA/frontend/Callback.h"

#include "common/frontend/CommonTest.h"

using namespace LOICollection::frontend;

class ShortCircuitTest : public testing::Test {
protected:
    inline static bool fail_called = false;
    inline static bool side_called = false;

protected:
    static void SetUpTestSuite() {
        MacroCall::getInstance().registerMacro("test_fail",
            [](const CallbackTypeValues&) -> std::string {
                fail_called = true;

                throw std::runtime_error("fail() shold not be called in short-circuit");
            }, {});
        MacroCall::getInstance().registerMacro("test_side",
            [](const CallbackTypeValues&) -> std::string {
                side_called = true;

                return "ok";
            }, {});
        MacroCall::getInstance().registerMacro("test_true",
            [](const CallbackTypeValues&) -> std::string {
                return "true";
            }, {});
        MacroCall::getInstance().registerMacro("test_false",
            [](const CallbackTypeValues&) -> std::string {
                return "false";
            }, {});
    }

    static void TearDownTestSuite() {
        MacroCall::getInstance().unregisterMacro("test_fail", {}, false);
        MacroCall::getInstance().unregisterMacro("test_side", {}, false);
        MacroCall::getInstance().unregisterMacro("test_true", {}, false);
        MacroCall::getInstance().unregisterMacro("test_false", {}, false);
    }

    void SetUp() override {
       fail_called = false;
       side_called = false;
    }
};

TEST_F(ShortCircuitTest, ConstantFalseAnd) {
    EXPECT_NO_THROW({
        auto result = eval("false && {test_fail}");
        EXPECT_EQ(result, "false");
        EXPECT_FALSE(fail_called);
    });
}

TEST_F(ShortCircuitTest, ConstantTrueOr) {
    EXPECT_NO_THROW({
        auto result = eval("true || {test_fail}");
        EXPECT_EQ(result, "true");
        EXPECT_FALSE(fail_called);
    });
}

TEST_F(ShortCircuitTest, ConstantTrueAnd) {
    EXPECT_NO_THROW({
        auto result = eval("true && {test_side}");
        EXPECT_EQ(result, "true");
        EXPECT_TRUE(side_called);
    });
}

TEST_F(ShortCircuitTest, ConstantFalseOr) {
    EXPECT_NO_THROW({
        auto result = eval("false || {test_side}");
        EXPECT_EQ(result, "true");
        EXPECT_TRUE(side_called);
    });
}

TEST_F(ShortCircuitTest, RuntimeFalseAnd) {
    EXPECT_NO_THROW({
        auto result = eval("{test_false} && {test_fail}");
        EXPECT_EQ(result, "false");
        EXPECT_FALSE(fail_called);
    });
}

TEST_F(ShortCircuitTest, RuntimeTrueOr) {
    EXPECT_NO_THROW({
        auto result = eval("{test_true} || {test_fail}");
        EXPECT_EQ(result, "true");
        EXPECT_FALSE(fail_called);
    });
}

TEST_F(ShortCircuitTest, RuntimeTrueAnd) {
    EXPECT_NO_THROW({
        auto result = eval("{test_true} && {test_side}");
        EXPECT_EQ(result, "true");
        EXPECT_TRUE(side_called);
    });
}

TEST_F(ShortCircuitTest, RuntimeFalseOr) {
    EXPECT_NO_THROW({
        auto result = eval("{test_false} || {test_side}");
        EXPECT_EQ(result, "true");
        EXPECT_TRUE(side_called);
    });
}

TEST_F(ShortCircuitTest, NestedAndOr) {
    EXPECT_NO_THROW({
        auto result = eval("(false || true) && {test_side}");
        EXPECT_EQ(result, "true");
        EXPECT_TRUE(side_called);
    });
}

TEST_F(ShortCircuitTest, NestedShortCircuit) {
    EXPECT_NO_THROW({
        auto result = eval("(false && {test_fail}) || {test_side}");
        EXPECT_EQ(result, "true");
        EXPECT_FALSE(fail_called);
        EXPECT_TRUE(side_called);
    });
}
