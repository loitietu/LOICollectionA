#include <gtest/gtest.h>

#include "LOICollectionA/frontend/Callback.h"

#include "common/frontend/CommonTest.h"

using namespace LOICollection::frontend;

TEST(MathBuiltinsTest, BasicMath) {
    EXPECT_EQ(eval("math::abs(-10)"), "10");
    EXPECT_EQ(eval("math::min(5,3)"), "3");
    EXPECT_EQ(eval("math::max(5,3)"), "5");
    EXPECT_EQ(eval("math::sqrt(9)"), "3");
    EXPECT_EQ(eval("math::pow(2,4)"), "16");
    EXPECT_EQ(eval("math::sin(0)"), "0");
    EXPECT_EQ(eval("math::cos(0)"), "1");
}

TEST(StringBuiltinsTest, BasicString) {
    EXPECT_EQ(eval("string::length(\"hello\")"), "5");
    EXPECT_EQ(eval("string::upper(\"abc\")"), "ABC");
    EXPECT_EQ(eval("string::lower(\"ABC\")"), "abc");
    EXPECT_EQ(eval("string::substr(\"hello\", 1, 3)"), "ell");
    EXPECT_EQ(eval("string::trim(\"  hi \")"), "hi");
    EXPECT_EQ(eval("string::replace(\"abcabc\", \"a\", \"x\")"), "xbcxbc");
}

TEST(BuiltinsTest, UnregisteredFunction) {
    EXPECT_THROW(eval("nonexistent::func()"), std::runtime_error);
    EXPECT_THROW(eval("math::nonexistent(1)"), std::runtime_error);
}

TEST(MacroTest, CustomMacro) {
    MacroCall::getInstance().registerMacro("test_greet",
        [](const CallbackTypeValues&) -> std::string {
            return "Hello from macro!";
        }, {});
    EXPECT_EQ(eval("{test_greet}"), "Hello from macro!");

    MacroCall::getInstance().registerMacro("test_add",
        [](const CallbackTypeValues& args) -> std::string {
            int a = std::get<int>(args[0]);
            int b = std::get<int>(args[1]);
            return std::to_string(a + b);
        }, { ParamType::INT, ParamType::INT });
    EXPECT_EQ(eval("{test_add(6,7)}"), "13");
}