#include <gtest/gtest.h>

#include <string>

#include "LOICollectionA/frontend/DiagnosticEngine.h"
#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
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
