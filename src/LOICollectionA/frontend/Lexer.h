#pragma once

#include <string>

#include "LOICollectionA/base/Macro.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"

namespace LOICollection::frontend {
    enum class TokenType {
        TOKEN_IF, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRCKET,
        TOKEN_RBRCKET, TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_IDENT,
        TOKEN_INT, TOKEN_FLOAT, TOKEN_STRING, TOKEN_OP,
        TOKEN_BOOL_OP, TOKEN_COLON, TOKEN_BOOL_LIT, TOKEN_PLUS,
        TOKEN_MINUS, TOKEN_MULTIPLY, TOKEN_DIVIDE, TOKEN_MOD,
        TOKEN_POWER, TOKEN_NAMESPACE, TOKEN_COMMA, TOKEN_TRANSPILE,
        TOKEN_SEMICOLON, TOKEN_DOT, TOKEN_ARROW, TOKEN_CLASS,
        TOKEN_FUNC, TOKEN_NEW, TOKEN_THIS, TOKEN_RETURN,
        TOKEN_PUBLIC, TOKEN_PRIVATE, TOKEN_EOF
    };
    
    struct Token {
        TokenType type;
        std::string value;
        SourceLocation loc;
    };
    
    class Lexer {
        std::string input;
        size_t position;
        size_t line;
        size_t column;
        char currentChar;
        
        DiagnosticEngine& diagnostics;

    public:
        LOICOLLECTION_A_API   Lexer(std::string str, DiagnosticEngine& diag);

        LOICOLLECTION_A_API   void advance();

        LOICOLLECTION_A_NDAPI Token getNextToken();
        LOICOLLECTION_A_NDAPI Token peekNextToken();

    private:
        Token parseString(char delimiter);
        Token parseIdentifier();
        Token parseNumber();
        Token parseColon();
        Token parseOperator();

        void skipWhitespace();
        void skipComment();

        char peekChar() const;

        Token makeToken(TokenType type);
    };
}
