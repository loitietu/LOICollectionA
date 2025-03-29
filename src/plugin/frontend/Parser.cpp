#include <string>
#include <stdexcept>
#include <variant>

#include "frontend/Lexer.h"

#include "frontend/Parser.h"

namespace LOICollection::frontend {
    std::string Parser::parse() {
        return parseIfStatement();
    }

    std::string Parser::parseIfStatement() {
        eat(TOKEN_IF);
        eat(TOKEN_LPAREN);
        bool condition = parseBoolExpression();
        eat(TOKEN_RPAREN);
        eat(TOKEN_LBRACKET);
        
        std::string true_branch = parseResult();
        eat(TOKEN_COMMA);
        std::string false_branch = parseResult();
        
        eat(TOKEN_RBRACKET);
        return condition ? true_branch : false_branch;
    }

    bool Parser::parseBoolExpression() {
        return parseOrExpression();
    }

    bool Parser::parseOrExpression() {
        bool result = parseAndExpression();
        while (current_token.type == TOKEN_BOOL_OP && current_token.value == "||") {
            eat(TOKEN_BOOL_OP);
            bool right = parseAndExpression(); 
            result = result || right;
        }
        return result;
    }
    
    bool Parser::parseAndExpression() {
        bool result = parsePrimary();
        while (current_token.type == TOKEN_BOOL_OP && current_token.value == "&&") {
            eat(TOKEN_BOOL_OP);
            bool right = parsePrimary();
            result = result && right;
        }
        return result;
    }

    bool Parser::parsePrimary() {
        if (current_token.type == TOKEN_LPAREN) {
            eat(TOKEN_LPAREN);
            bool result = parseBoolExpression();
            if (current_token.type != TOKEN_RPAREN)
                throw std::runtime_error("Missing closing parenthesis");
            eat(TOKEN_RPAREN);
            return result;
        }
        return parseComparison();
    }

    bool Parser::parseComparison() {
        auto left = parseValue();
        std::string op = current_token.value;
        eat(TOKEN_OP);
        auto right = parseValue();

        if (op == "==") return left == right;
        if (op == "!=") return left != right;
        if (op == ">") return left > right;
        if (op == "<") return left < right;
        if (op == ">=") return left >= right;
        if (op == "<=") return left <= right;
        
        throw std::runtime_error("Invalid operator: " + op);
    }

    Parser::Value Parser::parseValue() {
        Parser::Value v;
        if (current_token.type == TOKEN_NUMBER) {
            v.data = std::stoi(current_token.value);
            eat(TOKEN_NUMBER);
        } else if (current_token.type == TOKEN_STRING) {
            v.data = current_token.value;
            eat(TOKEN_STRING);
        } else 
            throw std::runtime_error("Unexpected value type in condition");
        return v;
    }

    std::string Parser::parseResult() {
        std::string result;
        while (current_token.type != TOKEN_COMMA && 
               current_token.type != TOKEN_RBRACKET &&
               current_token.type != TOKEN_EOF) {
            if (current_token.type == TOKEN_IF)
                result += parseIfStatement();
            else {
                result += current_token.value;
                eat(current_token.type);
                
                if (current_token.type == TOKEN_IDENT || 
                    current_token.type == TOKEN_STRING ||
                    current_token.type == TOKEN_NUMBER) {
                    result += " ";
                }
            }
        }
        if (!result.empty() && result.back() == ' ')
            result.pop_back();
        return result;
    }

    void Parser::eat(TokenType expected) {
        if (current_token.type != expected) {
            throw std::runtime_error("Syntax error at position " + std::to_string(current_token.pos)
                + ": Expected " + getTokenName(expected) + ", got " + getTokenName(current_token.type));
        }
        current_token = lexer.getNextToken();
    }
    
    std::string Parser::getTokenName(TokenType type) {
        const char* names[] = {"IF", "(", ")", "[", "]", ",", "IDENT", "NUMBER", "STRING", "OP", "BOOL_OP", "EOF"};
        return names[type];
    }
}