#include <gtest/gtest.h>

#include <ll/api/Expected.h>

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

TEST(FunctionCallTest, IsRegistered) {
    auto& fc = FunctionCall::getInstance();
    const std::string ns = "test_reg";
    const CallbackTypeArgs args = { ParamType::INT };

    fc.registerFunction(ns, "double",
        [](const CallbackTypeValues& v) -> TypedValue {
            return std::get<int>(v[0]) * 2;
        }, args);

    EXPECT_TRUE(fc.isRegistered(ns, "double", args));
    EXPECT_FALSE(fc.isRegistered(ns, "double", { ParamType::STRING }));
    EXPECT_FALSE(fc.isRegistered(ns, "missing", args));

    fc.unregisterFunction(ns, "double", args, false);
    EXPECT_FALSE(fc.isRegistered(ns, "double", args));
}

TEST(FunctionCallTest, UnregisterCombinationCallback) {
    auto& fc = FunctionCall::getInstance();
    const std::string ns = "test_combo_unreg";

    fc.registerFunction(ns, "combo",
        [](const CallbackTypeValues&, const CallbackTypePlaces&) -> TypedValue { return 1; }, {});

    EXPECT_TRUE(fc.isRegistered(ns, "combo", {}));

    fc.unregisterFunction(ns, "combo", {}, true);

    EXPECT_FALSE(fc.isRegistered(ns, "combo", {}));
}

TEST(FunctionCallTest, WrongArgumentTypes) {
    DiagnosticEngine diagnostics;
    auto& fc = FunctionCall::getInstance();
    const std::string ns = "test_types";

    fc.registerFunction(ns, "only_int",
        [](const CallbackTypeValues& v) -> TypedValue {
            return std::get<int>(v[0]);
        }, { ParamType::INT });

    CallbackTypeValues wrong = { std::string("not int") };
    auto result = fc.callFunction(ns, "only_int", wrong, {}, diagnostics);

    EXPECT_TRUE(diagnostics.hasErrors());
    EXPECT_NE(diagnostics.getErrorMessage().find("Function not registered"), std::string::npos);

    fc.unregisterFunction(ns, "only_int", { ParamType::INT }, false);
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

TEST(CallbackTest, ValuesToTypesWithObject) {
    DiagnosticEngine diagnostics;
    auto obj = std::make_shared<Object>();
    CallbackTypeValues vals = { obj };

    auto types = valuesToTypes(vals, diagnostics);
    ASSERT_EQ(types.size(), 1u);
    EXPECT_EQ(types[0], ParamType::OBJECT);
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

TEST(MacroCallTest, CombinationWithPlaceholders) {
    DiagnosticEngine diagnostics;
    MacroCall::getInstance().registerMacro("placeholder_add",
        [](const CallbackTypeValues&, const CallbackTypePlaces& places) -> TypedValue {
            return std::any_cast<int>(places.at(0)) + 10;
        }, {});

    Context ctx(32);
    CallbackTypeValues noArgs;

    auto result = MacroCall::getInstance().callMacro("placeholder_add", noArgs, ctx.params, diagnostics);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(diagnostics.hasErrors());
    EXPECT_EQ(std::get<int>(result.value()), 42);

    MacroCall::getInstance().unregisterMacro("placeholder_add", {}, true);
}

TEST(MacroCallTest, UnregisterCombinationMacro) {
    MacroCall::getInstance().registerMacro("combo_unreg",
        [](const CallbackTypeValues&, const CallbackTypePlaces&) -> TypedValue { return "ok"; }, {});

    EXPECT_TRUE(MacroCall::getInstance().isRegistered("combo_unreg", {}));

    MacroCall::getInstance().unregisterMacro("combo_unreg", {}, true);

    EXPECT_FALSE(MacroCall::getInstance().isRegistered("combo_unreg", {}));
}

TEST(ClassCallTest, NativeCounterRegistered) {
    auto& cc = ClassCall::getInstance();

    EXPECT_TRUE(cc.isRegistered("Counter"));
    EXPECT_TRUE(cc.hasField("Counter", "count"));
    EXPECT_FALSE(cc.hasField("Counter", "nope"));
}

TEST(ClassCallTest, RegisterAndInspect) {
    auto& cc = ClassCall::getInstance();
    const std::string name = "TestNative";

    EXPECT_FALSE(cc.isRegistered(name));

    cc.registerClass(name, { "a", "b" });

    EXPECT_TRUE(cc.isRegistered(name));
    EXPECT_TRUE(cc.hasField(name, "a"));
    EXPECT_TRUE(cc.hasField(name, "b"));
    EXPECT_FALSE(cc.hasField(name, "c"));
    EXPECT_EQ(cc.getFields(name), (std::vector<std::string>{ "a", "b" }));
}

TEST(ClassCallTest, CreateAndCallMethod) {
    auto& cc = ClassCall::getInstance();
    DiagnosticEngine diagnostics;
    const std::string name = "Box";

    cc.registerClass(name, { "value" });
    cc.registerConstructor(name,
        [](const CallbackTypeValues& args) -> ll::Expected<ObjectRef> {
            auto obj = std::make_shared<Object>();
            obj->className = "Box";
            obj->fields["value"] = std::get<int>(args[0]);
            return obj;
        }, { ParamType::INT });
    cc.registerMethod(name, "get",
        [](const ObjectRef& self, const CallbackTypeValues&) -> TypedValue {
            return self->fields["value"];
        }, {});

    CallbackTypeValues args = { 7 };
    auto objResult = cc.create(name, args, {}, diagnostics);

    ASSERT_TRUE(objResult.has_value());
    EXPECT_FALSE(diagnostics.hasErrors());

    auto value = cc.callMethod(name, "get", {}, *objResult, {}, diagnostics);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::get<int>(value.value()), 7);

    auto missing = cc.callMethod(name, "nope", {}, *objResult, {}, diagnostics);
    EXPECT_FALSE(missing.has_value());
}
