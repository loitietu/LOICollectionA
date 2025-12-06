#pragma once

#include <string>
#include <utility>

namespace LOICollection::frontend {
    enum class TokenType {
        TOKEN_IF, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRCKET,
        TOKEN_RBRCKET, TOKEN_IDENT, TOKEN_NUMBER, TOKEN_STRING,
        TOKEN_OP, TOKEN_BOOL_OP, TOKEN_EOF, TOKEN_COLON,
        TOKEN_BOOL_LIT, TOKEN_PLUS, TOKEN_MINUS, TOKEN_MULTIPLY, 
        TOKEN_DIVIDE, TOKEN_MOD, TOKEN_POWER
    };
    
    struct Token {
        TokenType type;
        std::string value;
        size_t pos;
    };
    
    class Lexer {
        std::string input;
        size_t position;
        char current_char;

    public:
        Lexer(std::string str) : input(std::move(str)), position(0) {
            current_char = input.empty() ? static_cast<char>(0) : input[0];
        }

        void advance();

        Token getNextToken();

    private:
        Token parseString(char delimiter);
        Token parseIdentifier();
        Token parseNumber();
        Token parseOperator();

        void skipWhitespace();

        Token makeToken(TokenType type);
    };
}
