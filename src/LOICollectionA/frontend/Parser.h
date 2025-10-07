#pragma once

#include <memory>
#include <string>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Lexer.h"

namespace LOICollection::frontend {
    class Parser {
        Lexer& lexer;
        Token current_token;

    public:
        Parser(Lexer& l) : lexer(l) {
            current_token = lexer.getNextToken();
        }

        std::unique_ptr<ASTNode> parse();
    private:
        std::unique_ptr<IfNode> parseIfStatement(TokenType falsePartToken = TOKEN_EOF);

        std::unique_ptr<ExprNode> parseBoolExpression();
        std::unique_ptr<ExprNode> parseOrExpression();
        std::unique_ptr<ExprNode> parseAndExpression();
        std::unique_ptr<ExprNode> parsePrimary();
        std::unique_ptr<ExprNode> parseComparison();

        std::unique_ptr<ValueNode> parseValue();

        std::unique_ptr<ASTNode> parseResult(TokenType stopToken = TOKEN_EOF); 

        bool shouldAddSpaceAfter(TokenType type);

        void eat(TokenType expected);
        void flushBuffer(std::string& buffer, std::unique_ptr<TemplateNode>& tpl);

        std::string getTokenName(TokenType type);
    };
}
