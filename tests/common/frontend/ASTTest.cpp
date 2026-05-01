#include <gtest/gtest.h>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"

using namespace LOICollection::frontend;

TEST(ASTTest, NodeTypesFromParsing) {
    {
        Lexer lex("42");
        Parser parser(lex);

        auto ast = parser.parse();

        ASSERT_EQ(ast->getType(), ASTNode::Type::Template);

        auto& tpl = static_cast<TemplateNode&>(*ast);
        ASSERT_EQ(tpl.parts.size(), 1);
        EXPECT_EQ(tpl.parts[0]->getType(), ASTNode::Type::Value);
    }
    {
        Lexer lex("1+2");
        Parser parser(lex);

        auto ast = parser.parse();

        auto& tpl = static_cast<TemplateNode&>(*ast);
        EXPECT_EQ(tpl.parts[0]->getType(), ASTNode::Type::Arithmetic);
    }
    {
        Lexer lex("if(true)[a:b]");
        Parser parser(lex);

        auto ast = parser.parse();
        
        auto& tpl = static_cast<TemplateNode&>(*ast);
        EXPECT_EQ(tpl.parts[0]->getType(), ASTNode::Type::If);
    }
}