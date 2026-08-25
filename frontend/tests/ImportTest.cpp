#include <gtest/gtest.h>

#include <string>

#include "LOICollectionA/frontend/Callback.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"
#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
#include "LOICollectionA/frontend/SemanticAnalyzer.h"
#include "LOICollectionA/frontend/ir/Compiler.h"

using namespace LOICollection::frontend;

TEST(ImportTest, LexerRecognizesImportKeyword) {
    DiagnosticEngine diagnostics;
    Lexer lexer("import", diagnostics);

    Token token = lexer.getNextToken();
    EXPECT_EQ(token.type, TokenType::TOKEN_IMPORT);
    EXPECT_EQ(token.value, "import");
}

TEST(ImportTest, ParserProducesImportNode) {
    DiagnosticEngine diagnostics;
    Lexer lexer("import \"generic.lcui\";\nx = 1;", diagnostics);
    Parser parser(lexer, diagnostics);
    auto ast = parser.parse();
    ASSERT_FALSE(diagnostics.hasErrors());

    auto program = dynamic_cast<ProgramNode*>(ast.get());
    ASSERT_NE(program, nullptr);
    ASSERT_GE(program->parts.size(), 1u);

    auto importNode = dynamic_cast<ImportNode*>(program->parts[0].get());
    ASSERT_NE(importNode, nullptr);
    EXPECT_EQ(importNode->path, "generic.lcui");
}

TEST(ImportTest, CompilerToleratesImport) {
    DiagnosticEngine diagnostics;
    Lexer lexer("import \"generic.lcui\";\nfunc f() -> int { return 1; }", diagnostics);
    Parser parser(lexer, diagnostics);
    auto ast = parser.parse();
    ASSERT_FALSE(diagnostics.hasErrors());

    ir::Compiler compiler(diagnostics);
    (void)compiler.compile(*ast);
    EXPECT_FALSE(diagnostics.hasErrors());
}

TEST(ImportTest, ImportCoexistsWithDeclarativeBlock) {
    // An import followed by a declarative UI block compiles together: the
    // import is a loader-level concern, so Sema/Compiler tolerate it while the
    // declarative block goes through the normal pipeline.
    DiagnosticEngine diagnostics;
    Lexer lexer(
        "import \"generic.lcui\";\n"
        "form = new CustomForm(\"id\", [\"t\"]) { label(\"hi\"); };\n",
        diagnostics);
    Parser parser(lexer, diagnostics);
    auto ast = parser.parse();
    ASSERT_FALSE(diagnostics.hasErrors());

    ClassCall& cc = ClassCall::getInstance();
    if (!cc.isRegistered("CustomForm")) {
        cc.registerClass("CustomForm", {});
        cc.registerConstructor("CustomForm",
            [](const CallbackTypeValues&) -> Expected<ObjectRef> {
                return std::make_shared<Object>();
            },
            { ParamType::STRING, ParamType::ARRAY });
        auto noop = [](const ObjectRef&, const CallbackTypeValues&) -> Expected<TypedValue> {
            return static_cast<int>(0);
        };
        cc.registerMethod("CustomForm", "label", noop, { ParamType::STRING });
    }

    SemanticAnalyzer analyzer(diagnostics);
    analyzer.analyze(static_cast<ProgramNode&>(*ast));
    if (diagnostics.hasErrors())
        FAIL() << diagnostics.getErrorMessage();

    ir::Compiler compiler(diagnostics);
    (void)compiler.compile(*ast);
    EXPECT_FALSE(diagnostics.hasErrors());
}
