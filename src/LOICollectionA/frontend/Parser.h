#pragma once

#include <memory>
#include <string>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Lexer.h"

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::frontend {
    class Parser final {
        Lexer& lexer;
        Token current_token;

    public:
        LOICOLLECTION_A_API   Parser(Lexer& l);

        LOICOLLECTION_A_NDAPI std::unique_ptr<ASTNode> parse();
    private:
        std::unique_ptr<IfNode> parseIfStatement(TokenType falsePartToken = TokenType::TOKEN_RBRCKET);
        std::unique_ptr<FunctionNode> parseFunction();

        std::unique_ptr<TemplateNode> parseArgs(TokenType delimiterToken = TokenType::TOKEN_COMMA, TokenType stopToken = TokenType::TOKEN_RPAREN);

        std::unique_ptr<ExprNode> parseBaseExpression();

        std::unique_ptr<ExprNode> parseBoolExpression();
        std::unique_ptr<ExprNode> parseOrExpression();
        std::unique_ptr<ExprNode> parseAndExpression();

        std::unique_ptr<ExprNode> parseComparison();
        std::unique_ptr<ExprNode> parseAdditiveExpression();
        std::unique_ptr<ExprNode> parseMultiplicativeExpression();
        std::unique_ptr<ExprNode> parsePowerExpression();
        std::unique_ptr<ExprNode> parseUnaryExpression();
        std::unique_ptr<ExprNode> parsePrimary();

        std::unique_ptr<ValueNode> parseValue();

        std::unique_ptr<ASTNode> parseResult(TokenType stopToken = TokenType::TOKEN_EOF); 

        TokenType peek();

        void eat(TokenType expected);
        void flushBuffer(std::string& buffer, std::unique_ptr<TemplateNode>& tpl);

        std::string getTokenName(TokenType type);
    };
}
