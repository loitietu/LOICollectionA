#include <gtest/gtest.h>

#include "LOICollectionA/frontend/Callback.h"
#include "LOICollectionA/frontend/Evaluator.h"

using namespace LOICollection::frontend;

TEST(FunctionCallTest, RegisterAndCall) {
    auto& fc = FunctionCall::getInstance();
    const std::string ns = "test_callback";

    fc.registerFunction(ns, "add",
        [](const CallbackTypeValues& v) -> std::string {
            return std::to_string(std::get<int>(v[0]) + std::get<int>(v[1]));
        }, { ParamType::INT, ParamType::INT });

    CallbackTypeValues args = { 10, 20 };
    EXPECT_EQ(fc.callFunction(ns, "add", args), "30");
    EXPECT_THROW((void)fc.callFunction(ns, "sub", args), std::runtime_error);
}

TEST(FunctionCallTest, CombinationFunction) {
    auto& fc = FunctionCall::getInstance();
    const std::string ns = "test_combo";

    fc.registerFunction(ns, "get_placeholder",
        [](const CallbackTypeValues&, const CallbackTypePlaces& places) -> std::string {
            if (places.count(0)) {
                return std::to_string(std::any_cast<int>(places.at(0)) * 2);
            }
            return "missing";
        }, {});

    Context ctx(42);
    CallbackTypeValues emptyArgs;
    std::string result = fc.callFunction(ns, "get_placeholder", emptyArgs, ctx.params);
    EXPECT_EQ(result, "84");
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
    EXPECT_EQ(MacroCall::getInstance().callMacro("echo", args), "test123");
    EXPECT_THROW((void)MacroCall::getInstance().callMacro("nonexistent", args), std::runtime_error);
}