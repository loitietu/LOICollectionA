#include <set>
#include <string>
#include <stdexcept>
#include <memory>

#include "frontend/AST.h"

#include "frontend/Parser.h"

namespace LOICollection::frontend {
    std::unique_ptr<ASTNode> Parser::parse() {
        if (current_token.type == TOKEN_IF)
            return parseIfStatement();
        return parseResult(TOKEN_EOF);
    }

    std::unique_ptr<IfNode> Parser::parseIfStatement(TokenType falsePartToken) {
        eat(TOKEN_IF);
        eat(TOKEN_LPAREN);

        auto cond = parseBoolExpression();

        eat(TOKEN_RPAREN);
        eat(TOKEN_QUESTION);
        
        auto truePart = parseResult(TOKEN_COLON);

        eat(TOKEN_COLON);

        auto falsePart = parseResult(falsePartToken);
        
        return std::make_unique<IfNode>(
            std::move(cond),
            std::move(truePart),
            std::move(falsePart)
        );
    }

    std::unique_ptr<ExprNode> Parser::parseBoolExpression() {
        return parseOrExpression();
    }

    std::unique_ptr<ExprNode> Parser::parseOrExpression() {
        auto left = parseAndExpression();
        while (current_token.type == TOKEN_BOOL_OP && current_token.value == "||") {
            eat(TOKEN_BOOL_OP);
            auto right = parseAndExpression();
            left = std::make_unique<LogicalNode>(std::move(left), std::move(right), "||");
        }
        return left;
    }
    
    std::unique_ptr<ExprNode> Parser::parseAndExpression() {
        auto left = parsePrimary();
        while (current_token.type == TOKEN_BOOL_OP && current_token.value == "&&") {
            eat(TOKEN_BOOL_OP);
            auto right = parsePrimary();
            left = std::make_unique<LogicalNode>(std::move(left), std::move(right), "&&");
        }
        return left;
    }

    std::unique_ptr<ExprNode> Parser::parsePrimary() {
        if (current_token.type == TOKEN_LPAREN) {
            eat(TOKEN_LPAREN);

            auto expr = parseBoolExpression();

            eat(TOKEN_RPAREN);
            return expr;
        }
        return parseComparison();
    }

    std::unique_ptr<ExprNode> Parser::parseComparison() {
        auto left = parseValue();
    
        if (current_token.type == TOKEN_OP) { 
            std::string op = current_token.value;

            eat(TOKEN_OP);

            auto right = parseValue();
            return std::make_unique<CompareNode>(
                std::move(left),
                std::move(right),
                op
            );
        }
        return left;
    }

    std::unique_ptr<ValueNode> Parser::parseValue() {
        if (current_token.type == TOKEN_NUMBER) {
            int value = std::stoi(current_token.value);

            eat(TOKEN_NUMBER);
            return std::make_unique<ValueNode>(value);
        }
        if (current_token.type == TOKEN_STRING) {
            std::string str = current_token.value;

            eat(TOKEN_STRING);
            return std::make_unique<ValueNode>(str);
        }
        if (current_token.type == TOKEN_BOOL_LIT) {
            bool val = (current_token.value == "true");
            
            eat(TOKEN_BOOL_LIT);
            return std::make_unique<ValueNode>(val ? 1 : 0);
        }
        throw std::runtime_error("Unexpected value type: " + current_token.value);
    }

    std::unique_ptr<ASTNode> Parser::parseResult(TokenType stopToken) {
        auto tpl = std::make_unique<TemplateNode>();

        std::string buffer;
        while (current_token.type != stopToken && current_token.type != TOKEN_EOF) {
            if (current_token.type == TOKEN_IF) {
                if (!buffer.empty()) {
                    tpl->addPart(std::make_unique<ValueNode>(buffer));
                    buffer.clear();
                }
                tpl->addPart(parseIfStatement(stopToken));
            } else {
                buffer += current_token.value;
                eat(current_token.type);
                
                if (shouldAddSpaceAfter(current_token.type))
                    buffer += " ";
            }
        }
        if (!buffer.empty()) {
            tpl->addPart(std::make_unique<ValueNode>(buffer));
            buffer.clear();
        }
        
        return tpl;
    }

    bool Parser::shouldAddSpaceAfter(TokenType type) {
        static const std::set<TokenType> spaceAfterTokens = {
            TOKEN_IDENT, TOKEN_NUMBER, TOKEN_STRING, TOKEN_BOOL_LIT
        };
        return spaceAfterTokens.count(type) > 0;
    }

    void Parser::eat(TokenType expected) {
        if (current_token.type != expected) {
            throw std::runtime_error("Syntax error at position " + std::to_string(current_token.pos)
                + ": Expected " + getTokenName(expected) + ", got " + getTokenName(current_token.type));
        }
        current_token = lexer.getNextToken();
    }
    
    std::string Parser::getTokenName(TokenType type) {
        const char* names[] = {"IF", "(", ")", "IDENT", "NUMBER", "STRING", "OP", "BOOL_OP", "EOF", "?", ":", "BOOL_LIT"};
        return names[type];
    }
}