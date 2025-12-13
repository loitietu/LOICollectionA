#pragma once

#include <string>

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::frontend {
    enum class TokenType {
        TOKEN_IF, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRCKET,
        TOKEN_RBRCKET, TOKEN_IDENT, TOKEN_INT, TOKEN_FLOAT, 
        TOKEN_STRING, TOKEN_OP, TOKEN_BOOL_OP, TOKEN_EOF,
        TOKEN_COLON, TOKEN_BOOL_LIT, TOKEN_PLUS, TOKEN_MINUS,
        TOKEN_MULTIPLY, TOKEN_DIVIDE, TOKEN_MOD, TOKEN_POWER, 
        TOKEN_NAMESPACE, TOKEN_COMMA
    };
    
    struct Token {
        TokenType type;
        std::string value;
        size_t pos;
    };
    
    class Lexer final {
        std::string input;
        size_t position;
        char current_char;

    public:
        LOICOLLECTION_A_API   Lexer(std::string str);

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

        Token makeToken(TokenType type);
    };
}
