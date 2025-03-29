#include <string>
#include <stdexcept>
#include <cstring>

#include "frontend/Lexer.h"

namespace LOICollection::frontend {
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

            if (current_char == '"') return parseString();
            if (isalpha(current_char)) return parseIdentifier();
            if (isdigit(current_char)) return parseNumber();
            if (current_char == '(') return makeToken(TOKEN_LPAREN);
            if (current_char == ')') return makeToken(TOKEN_RPAREN);
            if (current_char == '[') return makeToken(TOKEN_LBRACKET);
            if (current_char == ']') return makeToken(TOKEN_RBRACKET);
            if (current_char == ',') return makeToken(TOKEN_COMMA);
            if (strchr("=><!&|", current_char)) return parseOperator();

            throw std::runtime_error("Unexpected character: " + std::string(1, current_char));
        }
        return {TOKEN_EOF, "", position};
    }

    Token Lexer::parseString() {
        advance();
        size_t start = position;
        while (current_char != '"' && current_char != 0)
            advance();
        if (current_char != '"')
            throw std::runtime_error("Unclosed string");
        advance();
        return {TOKEN_STRING, input.substr(start, position - start - 1), start};
    }

    Token Lexer::parseIdentifier() {
        size_t start = position;
        while (isalnum(current_char) || current_char == '_')
            advance();
        std::string id = input.substr(start, position - start);
        if (id == "if")
            return {TOKEN_IF, id, start};
        return {TOKEN_IDENT, id, start};
    }

    Token Lexer::parseNumber() {
        size_t start = position;
        while (isdigit(current_char))
            advance();
        return {TOKEN_NUMBER, input.substr(start, position - start), start};
    }

    Token Lexer::parseOperator() {
        size_t start = position;
        char first = current_char;
        advance();

        if ((first == '&' && current_char == '&') || (first == '|' && current_char == '|')) {
            std::string op(1, first); op += current_char;
            advance();
            return {TOKEN_BOOL_OP, op, start};
        }

        if (current_char == '=') {
            std::string op(1, first); op += '=';
            advance();
            return {TOKEN_OP, op, start};
        }

        return {TOKEN_OP, std::string(1, first), start};
    }

    void Lexer::skipWhitespace() {
        while (isspace(current_char))
            advance();
    }

    Token Lexer::makeToken(TokenType type) {
        Token t{type, std::string(1, current_char), position};
        advance();
        return t;
    }
}