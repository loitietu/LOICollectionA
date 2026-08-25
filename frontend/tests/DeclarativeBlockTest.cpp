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
    /* Parse + compile a snippet; returns false when any diagnostic is emitted. */
    bool compiles(const std::string& input) {
        DiagnosticEngine diagnostics;
        Lexer lexer(input, diagnostics);
        Parser parser(lexer, diagnostics);
        auto ast = parser.parse();
        if (diagnostics.hasErrors())
            return false;

        ir::Compiler compiler(diagnostics);
        (void)compiler.compile(*ast);
        return !diagnostics.hasErrors();
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
    auto& cc = ClassCall::getInstance();
    if (!cc.isRegistered("CustomForm"))
        cc.registerClass("CustomForm", {});

    EXPECT_TRUE(compiles("new CustomForm(\"id\", \"title\") { label(\"hi\"); closeButton(); }"));
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
