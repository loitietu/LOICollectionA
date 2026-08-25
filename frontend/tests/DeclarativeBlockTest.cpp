#include <gtest/gtest.h>

#include <string>

#include "LOICollectionA/frontend/Callback.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"
#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
#include "LOICollectionA/frontend/SemanticAnalyzer.h"
#include "LOICollectionA/frontend/ir/Compiler.h"

using namespace LOICollection::frontend;

namespace {
    /* Register a minimal CustomForm native class (constructor + a few methods)
     * so declarative-block tests can run the whole frontend pipeline. Safe to
     * call repeatedly: ClassCall registration is keyed by signature. */
    void registerCustomForm() {
        ClassCall& cc = ClassCall::getInstance();
        if (!cc.isRegistered("CustomForm"))
            cc.registerClass("CustomForm", {});

        cc.registerConstructor("CustomForm",
            [](const CallbackTypeValues&) -> Expected<ObjectRef> {
                auto obj = std::make_shared<Object>();
                obj->className = "CustomForm";
                return obj;
            },
            { ParamType::STRING, ParamType::ARRAY });

        auto noop = [](const ObjectRef&, const CallbackTypeValues&) -> Expected<TypedValue> {
            return static_cast<int>(0);
        };
        for (const CallbackTypeArgs& sig : {
                CallbackTypeArgs{ ParamType::STRING },
                CallbackTypeArgs{ ParamType::STRING, ParamType::OBJECT } })
            cc.registerMethod("CustomForm", "label", noop, sig);
        cc.registerMethod("CustomForm", "button", noop, { ParamType::STRING, ParamType::FUNCTION });
        cc.registerMethod("CustomForm", "close", noop, {});
    }

    /* Run parse + analyze + compile; returns the first diagnostic or "OK". */
    std::string run(const std::string& input) {
        registerCustomForm();
        DiagnosticEngine diagnostics;
        Lexer lexer(input, diagnostics);
        Parser parser(lexer, diagnostics);
        auto ast = parser.parse();
        if (diagnostics.hasErrors())
            return diagnostics.getErrorMessage();

        SemanticAnalyzer analyzer(diagnostics);
        analyzer.analyze(static_cast<ProgramNode&>(*ast));
        if (diagnostics.hasErrors())
            return diagnostics.getErrorMessage();

        ir::Compiler compiler(diagnostics);
        (void)compiler.compile(*ast);
        return diagnostics.hasErrors() ? diagnostics.getErrorMessage() : "OK";
    }
}

TEST(DeclarativeBlockTest, ParsesIntoNewNodeWithBlock) {
    DiagnosticEngine diagnostics;
    Lexer lexer("new CustomForm(\"id\", \"title\") { label(\"hi\"); }", diagnostics);
    Parser parser(lexer, diagnostics);
    auto ast = parser.parse();
    ASSERT_FALSE(diagnostics.hasErrors());

    auto program = dynamic_cast<ProgramNode*>(ast.get());
    ASSERT_NE(program, nullptr);
    ASSERT_EQ(program->parts.size(), 1u);

    auto node = dynamic_cast<NewNode*>(program->parts[0].get());
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->className, "CustomForm");
    EXPECT_NE(node->declarativeBlock, nullptr);
}

TEST(DeclarativeBlockTest, CompilesImplicitCallsToRegisteredForm) {
    EXPECT_EQ(run("new CustomForm(\"id\", [\"t\"]) { label(\"hi\"); close(); }"), "OK");
}

TEST(DeclarativeBlockTest, RejectedForNonFormClass) {
    DiagnosticEngine diagnostics;
    Lexer lexer("class A {} new A() { f(); }", diagnostics);
    Parser parser(lexer, diagnostics);
    auto ast = parser.parse();
    ASSERT_FALSE(diagnostics.hasErrors());

    SemanticAnalyzer analyzer(diagnostics);
    analyzer.analyze(static_cast<ProgramNode&>(*ast));
    EXPECT_TRUE(diagnostics.hasErrors());
    EXPECT_NE(diagnostics.getErrorMessage().find("Declarative UI block is not allowed"), std::string::npos);
}

TEST(DeclarativeBlockTest, BareCallFallsBackToGlobalFunction) {
    // A bare call that is not a form method resolves to a global function.
    EXPECT_EQ(run(
        "func helper() -> void { }\n"
        "form = new CustomForm(\"id\", [\"t\"]) { helper(); }"),
        "OK");
}

TEST(DeclarativeBlockTest, LhsNameReferencedInBlockBody) {
    // The block body may reference the form under its own LHS variable name.
    EXPECT_EQ(run(
        "form = new CustomForm(\"id\", [\"t\"]) { form.label(\"x\"); }"),
        "OK");
}

TEST(DeclarativeBlockTest, OnSugarLambdaReferencesForm) {
    // `on:` sugar with a lambda that closes over the form under its own name.
    EXPECT_EQ(run(
        "form = new CustomForm(\"id\", [\"t\"]) {\n"
        "    button(\"go\", on: func () -> void { form.close(); });\n"
        "};"),
        "OK");
}
