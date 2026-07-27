#include <gtest/gtest.h>

#include "LOICollectionA/frontend/Context.h"
#include "LOICollectionA/frontend/Callback.h"

using namespace LOICollection::frontend;

TEST(FunctionCallTest, RegisterAndCall) {
    DiagnosticEngine diagnostics;
    auto& fc = FunctionCall::getInstance();
    const std::string ns = "test_callback";

    fc.registerFunction(ns, "add",
        [](const CallbackTypeValues& v) -> TypedValue {
            return std::get<int>(v[0]) + std::get<int>(v[1]);
        }, { ParamType::INT, ParamType::INT });

    CallbackTypeValues args = { 10, 20 };

    auto r1 = fc.callFunction(ns, "add", args, {}, diagnostics);
    EXPECT_TRUE(r1.has_value());
    EXPECT_FALSE(diagnostics.hasErrors());
    EXPECT_EQ(std::get<int>(r1.value()), 30);

    auto r2 = fc.callFunction(ns, "sub", args, {}, diagnostics);
    EXPECT_TRUE(diagnostics.hasErrors());

    fc.unregisterFunction(ns, "add", { ParamType::INT, ParamType::INT }, false);
}

TEST(FunctionCallTest, CombinationFunction) {
    DiagnosticEngine diagnostics;
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

    auto result = fc.callFunction(ns, "get_placeholder", emptyArgs, ctx.params, diagnostics);
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(diagnostics.hasErrors());
    EXPECT_EQ(std::get<int>(result.value()), 84);

    fc.unregisterFunction(ns, "get_placeholder", {}, true);
}

TEST(CallbackTest, ValuesToTypes) {
    DiagnosticEngine diagnostics;
    CallbackTypeValues vals = { 1, 2.5f, std::string("hello"), true };
    
    auto types = valuesToTypes(vals, diagnostics);
    ASSERT_EQ(types.size(), 4);
    EXPECT_EQ(types[0], ParamType::INT);
    EXPECT_EQ(types[1], ParamType::FLOAT);
    EXPECT_EQ(types[2], ParamType::STRING);
    EXPECT_EQ(types[3], ParamType::BOOL);
}

TEST(MacroCallTest, RegisterAndCall) {
    DiagnosticEngine diagnostics;
    MacroCall::getInstance().registerMacro("echo",
        [](const CallbackTypeValues& args) -> std::string {
            return std::get<std::string>(args[0]);
        }, { ParamType::STRING });
    
    CallbackTypeValues args = { std::string("test123") };

    auto r1 = MacroCall::getInstance().callMacro("echo", args, {}, diagnostics);
    EXPECT_TRUE(r1.has_value());
    EXPECT_FALSE(diagnostics.hasErrors());
    EXPECT_EQ(std::get<std::string>(r1.value()), "test123");

    auto r2 = MacroCall::getInstance().callMacro("nonexistent", args, {}, diagnostics);
    EXPECT_TRUE(diagnostics.hasErrors());

    MacroCall::getInstance().unregisterMacro("echo", { ParamType::STRING }, false);
}