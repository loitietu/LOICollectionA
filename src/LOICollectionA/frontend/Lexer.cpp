#include <string>
#include <cstddef>
#include <cstring>
#include <utility>
#include <stdexcept>

#include "LOICollectionA/frontend/Lexer.h"

namespace LOICollection::frontend {
    Lexer::Lexer(std::string str)  : input(std::move(str)), position(0) {
        current_char = input.empty() ? static_cast<char>(0) : input[0];
    }

    void Lexer::advance() {
        position++;
        current_char = (position < input.size()) ? input[position] : static_cast<char>(0);
    }

    Token Lexer::getNextToken() {
        while (current_char != 0) {
            if (isspace(current_char)) {
                skipWhitespace();
                continue;
            }

            switch (current_char) {
                case '"': return parseString('"');
                case '\'': return parseString('\'');
                case '(': return makeToken(TokenType::TOKEN_LPAREN);
                case ')': return makeToken(TokenType::TOKEN_RPAREN);
                case '[': return makeToken(TokenType::TOKEN_LBRCKET);
                case ']': return makeToken(TokenType::TOKEN_RBRCKET);
                case ':': return parseColon();
                case '+': return makeToken(TokenType::TOKEN_PLUS);
                case '-': return makeToken(TokenType::TOKEN_MINUS);
                case '*': return makeToken(TokenType::TOKEN_MULTIPLY);
                case '/': return makeToken(TokenType::TOKEN_DIVIDE);
                case '%': return makeToken(TokenType::TOKEN_MOD);
                case '^': return makeToken(TokenType::TOKEN_POWER);
                case ',': return makeToken(TokenType::TOKEN_COMMA);
            }

            if (std::isdigit(current_char) || current_char == '.') return parseNumber();
            if (std::strchr("=><!&|", current_char)) return parseOperator();

            return parseIdentifier();
        }
        
        return { TokenType::TOKEN_EOF, "", position };
    }

    Token Lexer::peekNextToken() {
        size_t pos = position;
        char c = current_char;

        Token t = getNextToken();

        position = pos;
        current_char = c;

        return t;
    }

    Token Lexer::parseString(char delimiter) {
        advance();
        
        size_t start = position;
        while (current_char != delimiter && current_char != 0)
            advance();

        if (current_char != delimiter)
            throw std::runtime_error("Unclosed string");

        advance();
        return { TokenType::TOKEN_STRING, input.substr(start, position - start - 1), start };
    }

    Token Lexer::parseIdentifier() {
        size_t start = position;
        while (current_char != 0 && !std::isspace(current_char) && !std::strchr("()[]=><!&|.:", current_char))
            advance();

        std::string id = input.substr(start, position - start);

        if (id == "if") return { TokenType::TOKEN_IF, std::move(id), start };
        if (id == "true" || id == "false") return { TokenType::TOKEN_BOOL_LIT, std::move(id), start };
        
        return { TokenType::TOKEN_IDENT, std::move(id), start };
    }

    Token Lexer::parseNumber() {
        size_t start = position;

        while (std::isdigit(current_char) || current_char == '.')
            advance();
        
        std::string num = input.substr(start, position - start);

        if (num.find('.') != std::string::npos)
            return { TokenType::TOKEN_FLOAT, num, start };

        return { TokenType::TOKEN_INT, num, start };
    }

    Token Lexer::parseColon() {
        size_t start = position;

        advance();

        if (current_char == ':') {
            advance();

            return { TokenType::TOKEN_NAMESPACE, "::", start };
        }

        return { TokenType::TOKEN_COLON, ":", start };
    }

    Token Lexer::parseOperator() {
        size_t start = position;
        char first = current_char;

        advance();

        if ((first == '&' && current_char == '&') || (first == '|' && current_char == '|')) {
            std::string op(1, first);
            op += current_char;
            
            advance();
            return { TokenType::TOKEN_BOOL_OP, op, start };
        }

        if (current_char == '=') {
            std::string op(1, first);
            op += '=';

            advance();
            return { TokenType::TOKEN_OP, op, start };
        }

        return { TokenType::TOKEN_OP, std::string(1, first), start };
    }

    void Lexer::skipWhitespace() {
        while (std::isspace(current_char))
            advance();
    }

    Token Lexer::makeToken(TokenType type) {
        Token t{ type, std::string(1, current_char), position };
        advance();
        return t;
    }
}
