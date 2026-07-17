#include <gtest/gtest.h>

#include "LOICollectionA/frontend/Context.h"
#include "LOICollectionA/frontend/Callback.h"

using namespace LOICollection::frontend;

TEST(FunctionCallTest, RegisterAndCall) {
    auto& fc = FunctionCall::getInstance();
    const std::string ns = "test_callback";

    fc.registerFunction(ns, "add",
        [](const CallbackTypeValues& v) -> TypedValue {
            return std::get<int>(v[0]) + std::get<int>(v[1]);
        }, { ParamType::INT, ParamType::INT });

    CallbackTypeValues args = { 10, 20 };
    EXPECT_EQ(std::get<int>(fc.callFunction(ns, "add", args)), 30);
    EXPECT_THROW((void)fc.callFunction(ns, "sub", args), std::runtime_error);

    fc.unregisterFunction(ns, "add", { ParamType::INT, ParamType::INT }, false);
}

TEST(FunctionCallTest, CombinationFunction) {
    auto& fc = FunctionCall::getInstance();
    const std::string ns = "test_combo";

    fc.registerFunction(ns, "get_placeholder",
        [](const CallbackTypeValues&, const CallbackTypePlaces& places) -> TypedValue {
            if (places.count(0))
                return std::any_cast<int>(places.at(0)) * 2;

            return "missing";
        }, {});

    Context ctx(42);
    CallbackTypeValues emptyArgs;
    EXPECT_EQ(std::get<int>(fc.callFunction(ns, "get_placeholder", emptyArgs, ctx.params)), 84);

    fc.unregisterFunction(ns, "get_placeholder", {}, true);
}

TEST(CallbackTest, ValuesToTypes) {
    CallbackTypeValues vals = { 1, 2.5f, std::string("hello"), true };
    
    auto types = valuesToTypes(vals);
    ASSERT_EQ(types.size(), 4);
    EXPECT_EQ(types[0], ParamType::INT);
    EXPECT_EQ(types[1], ParamType::FLOAT);
    EXPECT_EQ(types[2], ParamType::STRING);
    EXPECT_EQ(types[3], ParamType::BOOL);
}

TEST(MacroCallTest, RegisterAndCall) {
    MacroCall::getInstance().registerMacro("echo",
        [](const CallbackTypeValues& args) -> std::string {
            return std::get<std::string>(args[0]);
        }, { ParamType::STRING });
    
    CallbackTypeValues args = { std::string("test123") };
    EXPECT_EQ(std::get<std::string>(MacroCall::getInstance().callMacro("echo", args)), "test123");
    EXPECT_THROW((void)MacroCall::getInstance().callMacro("nonexistent", args), std::runtime_error);

    MacroCall::getInstance().unregisterMacro("echo", { ParamType::STRING }, false);
}