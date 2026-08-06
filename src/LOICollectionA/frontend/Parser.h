#pragma once

#include <memory>
#include <string>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::frontend {
    class Parser {
        Lexer& lexer;
        Token currentToken;
        
        DiagnosticEngine& diagnostics;

    public:
        LOICOLLECTION_A_API   Parser(Lexer& l, DiagnosticEngine& diag);

        LOICOLLECTION_A_NDAPI std::unique_ptr<ASTNode> parse();
    private:
        std::unique_ptr<IfNode> parseIfStatement();
        std::unique_ptr<FunctionNode> parseFunction();
        std::unique_ptr<MacroNode> parseMacro();
        std::unique_ptr<ClassNode> parseClass();
        std::unique_ptr<FunctionDefNode> parseFunctionDefinition();
        std::unique_ptr<LambdaNode> parseLambda();
        std::unique_ptr<MethodDecl> parseMethod(bool isPrivate);
        std::unique_ptr<MethodDecl> parseConstructor(const std::string& className, bool isPrivate);

        std::vector<MethodParam> parseParams();
        
        std::unique_ptr<ReturnNode> parseReturn();

        std::unique_ptr<ASTNode> parseTemplateUntil(TokenType stopToken, bool stopOnColon = false);

        std::unique_ptr<ValueNode> parseTranspile();
        std::unique_ptr<TemplateNode> parseArgs(TokenType delimiterToken = TokenType::TOKEN_COMMA, TokenType stopToken = TokenType::TOKEN_RPAREN);

        std::unique_ptr<ASTNode> parseStatement();
        
        std::unique_ptr<ExprNode> parseBaseExpression();

        std::unique_ptr<ExprNode> parseBoolExpression();
        std::unique_ptr<ExprNode> parseOrExpression();
        std::unique_ptr<ExprNode> parseAndExpression();

        std::unique_ptr<ExprNode> parseComparison();
        std::unique_ptr<ExprNode> parseAdditiveExpression();
        std::unique_ptr<ExprNode> parseMultiplicativeExpression();
        std::unique_ptr<ExprNode> parsePowerExpression();
        std::unique_ptr<ExprNode> parseUnaryExpression();
        std::unique_ptr<ExprNode> parsePostfix();
        std::unique_ptr<ExprNode> parsePrimary();

        std::unique_ptr<ValueNode> parseValue();

        TokenType peek();

        bool eat(TokenType expected);

        std::string getTokenName(TokenType type);
    };
}
