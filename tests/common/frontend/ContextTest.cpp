#include <gtest/gtest.h>

#include <string>

#include "LOICollectionA/frontend/Callback.h"
#include "LOICollectionA/frontend/Context.h"

#include "common/frontend/CommonTest.h"

using namespace LOICollection::frontend;

TEST(ContextTest, PlaceholdersAvailableInMacros) {
    MacroCall::getInstance().registerMacro("ctx_value",
        [](const CallbackTypeValues&, const CallbackTypePlaces& places) -> TypedValue {
            return std::any_cast<int>(places.at(0));
        }, {});

    Context ctx(123);
    EXPECT_EQ(eval("{ctx_value}", ctx), "123");

    MacroCall::getInstance().unregisterMacro("ctx_value", {}, true);
}

TEST(ContextTest, PlaceholdersInNamespacedFunction) {
    FunctionCall::getInstance().registerFunction("test_ctx", "echo_placeholder",
        [](const CallbackTypeValues&, const CallbackTypePlaces& places) -> TypedValue {
            return std::any_cast<std::string>(places.at(0));
        }, {});

    Context ctx(std::string("from ctx"));
    EXPECT_EQ(eval("test_ctx::echo_placeholder()", ctx), "from ctx");

    FunctionCall::getInstance().unregisterFunction("test_ctx", "echo_placeholder", {}, true);
}
