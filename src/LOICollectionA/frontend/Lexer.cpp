#include <string>
#include <cstddef>
#include <cstring>
#include <utility>
#include <stdexcept>

#include "LOICollectionA/frontend/Lexer.h"

namespace LOICollection::frontend {
    Lexer::Lexer(std::string str)  : input(std::move(str)), position(0) {
        currentChar = input.empty() ? static_cast<char>(0) : input[0];
    }

    void Lexer::advance() {
        position++;
        currentChar = (position < input.size()) ? input[position] : static_cast<char>(0);
    }

    Token Lexer::getNextToken() {
        while (currentChar != 0) {
            if (isspace(currentChar)) {
                skipWhitespace();
                continue;
            }

            switch (currentChar) {
                case '"': return parseString('"');
                case '\'': return parseString('\'');
                case '(': return makeToken(TokenType::TOKEN_LPAREN);
                case ')': return makeToken(TokenType::TOKEN_RPAREN);
                case '[': return makeToken(TokenType::TOKEN_LBRCKET);
                case ']': return makeToken(TokenType::TOKEN_RBRCKET);
                case '{': return makeToken(TokenType::TOKEN_LBRACE);
                case '}': return makeToken(TokenType::TOKEN_RBRACE);
                case ':': return parseColon();
                case '+': return makeToken(TokenType::TOKEN_PLUS);
                case '-': return makeToken(TokenType::TOKEN_MINUS);
                case '*': return makeToken(TokenType::TOKEN_MULTIPLY);
                case '/': return makeToken(TokenType::TOKEN_DIVIDE);
                case '%': return makeToken(TokenType::TOKEN_MOD);
                case '^': return makeToken(TokenType::TOKEN_POWER);
                case ',': return makeToken(TokenType::TOKEN_COMMA);
                case '$': return makeToken(TokenType::TOKEN_TRANSPILE);
            }

            if (std::isdigit(currentChar) || currentChar == '.') return parseNumber();
            if (std::strchr("=><!&|", currentChar)) return parseOperator();

            return parseIdentifier();
        }
        
        return { TokenType::TOKEN_EOF, "", position };
    }

    Token Lexer::peekNextToken() {
        size_t pos = position;
        char c = currentChar;

        Token t = getNextToken();

        position = pos;
        currentChar = c;

        return t;
    }

    Token Lexer::parseString(char delimiter) {
        advance();
        
        size_t start = position;
        while (currentChar != delimiter && currentChar != 0)
            advance();

        if (currentChar != delimiter)
            throw std::runtime_error("Unclosed string");

        advance();
        return { TokenType::TOKEN_STRING, input.substr(start, position - start - 1), start };
    }

    Token Lexer::parseIdentifier() {
        size_t start = position;
        while (currentChar != 0 && !std::isspace(currentChar) && !std::strchr("()[]{}=><!&|.:", currentChar))
            advance();

        std::string id = input.substr(start, position - start);

        if (id == "if") return { TokenType::TOKEN_IF, std::move(id), start };
        if (id == "true" || id == "false") return { TokenType::TOKEN_BOOL_LIT, std::move(id), start };
        
        return { TokenType::TOKEN_IDENT, std::move(id), start };
    }

    Token Lexer::parseNumber() {
        size_t start = position;

        while (std::isdigit(currentChar) || currentChar == '.')
            advance();
        
        std::string num = input.substr(start, position - start);

        if (num.find('.') != std::string::npos)
            return { TokenType::TOKEN_FLOAT, num, start };

        return { TokenType::TOKEN_INT, num, start };
    }

    Token Lexer::parseColon() {
        size_t start = position;

        advance();

        if (currentChar == ':') {
            advance();

            return { TokenType::TOKEN_NAMESPACE, "::", start };
        }

        return { TokenType::TOKEN_COLON, ":", start };
    }

    Token Lexer::parseOperator() {
        size_t start = position;
        char first = currentChar;

        advance();

        if ((first == '&' && currentChar == '&') || (first == '|' && currentChar == '|')) {
            std::string op(1, first);
            op += currentChar;
            
            advance();
            return { TokenType::TOKEN_BOOL_OP, op, start };
        }

        if (currentChar == '=') {
            std::string op(1, first);
            op += '=';

            advance();
            return { TokenType::TOKEN_OP, op, start };
        }

        return { TokenType::TOKEN_OP, std::string(1, first), start };
    }

    void Lexer::skipWhitespace() {
        while (std::isspace(currentChar))
            advance();
    }

    Token Lexer::makeToken(TokenType type) {
        Token t{ type, std::string(1, currentChar), position };

        advance();
        
        return t;
    }
}
