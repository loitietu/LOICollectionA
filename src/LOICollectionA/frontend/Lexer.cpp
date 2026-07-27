#include <string>
#include <cstddef>
#include <cstring>
#include <utility>

#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"

namespace LOICollection::frontend {
    Lexer::Lexer(std::string str, DiagnosticEngine& diag) : input(std::move(str)), position(0), line(1), column(1),
        currentChar(input.empty() ? static_cast<char>(0) : input[0]), diagnostics(diag) {}

    void Lexer::advance() {
        if (currentChar == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }

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
                case ';': return makeToken(TokenType::TOKEN_SEMICOLON);
            }

            if (std::isdigit(currentChar) || currentChar == '.') return parseNumber();
            if (std::strchr("=><!&|", currentChar)) return parseOperator();

            return parseIdentifier();
        }
        
        return { TokenType::TOKEN_EOF, "", {line, column, position} };
    }

    Token Lexer::peekNextToken() {
        size_t savedPos = position;
        size_t savedLine = line;
        size_t savedCol = column;
        char savedChar = currentChar;

        Token t = getNextToken();

        position = savedPos;
        line = savedLine;
        column = savedCol;
        currentChar = savedChar;

        return t;
    }

    Token Lexer::parseString(char delimiter) {
        advance();
        
        size_t start = position;

        SourceLocation startLoc(line, column, start);
        while (currentChar != delimiter && currentChar != 0)
            advance();

        if (currentChar != delimiter) {
            diagnostics.addError(startLoc, "Unclosed string");
            return { TokenType::TOKEN_EOF, "", startLoc };
        }

        advance();
        return { TokenType::TOKEN_STRING, input.substr(start, position - start - 1), startLoc };
    }

    Token Lexer::parseIdentifier() {
        size_t start = position;

        SourceLocation startLoc(line, column, start);
        while (currentChar != 0 && !std::isspace(currentChar) && !std::strchr("()[]{}=><!&|.:;", currentChar))
            advance();

        std::string id = input.substr(start, position - start);

        if (id == "if") return { TokenType::TOKEN_IF, std::move(id), startLoc };
        if (id == "true" || id == "false") return { TokenType::TOKEN_BOOL_LIT, std::move(id), startLoc };
        
        return { TokenType::TOKEN_IDENT, std::move(id), startLoc };
    }

    Token Lexer::parseNumber() {
        size_t start = position;

        SourceLocation startLoc(line, column, start);

        bool hasDot = false;
        while (currentChar != 0) {
            if (std::isdigit(currentChar)) {
                advance();
            } else if (currentChar == '.' && !hasDot) {
                hasDot = true;
                advance();
            } else {
                break;
            }
        }
        
        std::string num = input.substr(start, position - start);
        if (num.empty() || num == ".") {
            diagnostics.addError(startLoc, "Invalid numeric literal: '" + num + "'");
            return { TokenType::TOKEN_EOF, "", startLoc };
        }
        
        if (hasDot)
            return { TokenType::TOKEN_FLOAT, num, startLoc };

        return { TokenType::TOKEN_INT, num, startLoc };
    }

    Token Lexer::parseColon() {
        SourceLocation startLoc(line, column, position);

        advance();

        if (currentChar == ':') {
            advance();

            return { TokenType::TOKEN_NAMESPACE, "::", startLoc };
        }

        return { TokenType::TOKEN_COLON, ":", startLoc };
    }

    Token Lexer::parseOperator() {
        SourceLocation startLoc(line, column, position);

        char first = currentChar;

        advance();

        if ((first == '&' && currentChar == '&') || (first == '|' && currentChar == '|')) {
            std::string op(1, first);
            op += currentChar;
            
            advance();
            return { TokenType::TOKEN_BOOL_OP, op, startLoc };
        }

        if (currentChar == '=') {
            std::string op(1, first);
            op += '=';

            advance();
            return { TokenType::TOKEN_OP, op, startLoc };
        }

        return { TokenType::TOKEN_OP, std::string(1, first), startLoc };
    }

    void Lexer::skipWhitespace() {
        while (std::isspace(currentChar))
            advance();
    }

    Token Lexer::makeToken(TokenType type) {
        Token t{ type, std::string(1, currentChar), {line, column, position} };

        advance();
        
        return t;
    }
}
