#include <string>
#include <stdexcept>
#include <unordered_set>
#include <memory>

#include "LOICollectionA/frontend/AST.h"

#include "LOICollectionA/frontend/Parser.h"

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
            std::string str = std::move(current_token.value);

            eat(TOKEN_STRING);
            return std::make_unique<ValueNode>(std::move(str));
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
                flushBuffer(buffer, tpl);
                
                tpl->addPart(parseIfStatement(stopToken));
            } else {
                buffer += current_token.value;
                eat(current_token.type);
                
                if (shouldAddSpaceAfter(current_token.type))
                    buffer += " ";
            }
        }
        flushBuffer(buffer, tpl);
        
        return tpl;
    }

    bool Parser::shouldAddSpaceAfter(TokenType type) {
        static const std::unordered_set<TokenType> spaceAfterTokens = {
            TOKEN_IDENT, TOKEN_NUMBER, TOKEN_STRING, TOKEN_BOOL_LIT
        };
        return spaceAfterTokens.find(type) != spaceAfterTokens.end();
    }

    void Parser::eat(TokenType expected) {
        if (current_token.type != expected) {
            throw std::runtime_error("Syntax error at position " + std::to_string(current_token.pos)
                + ": Expected " + getTokenName(expected) + ", got " + getTokenName(current_token.type));
        }
        current_token = lexer.getNextToken();
    }

    void Parser::flushBuffer(std::string& buffer, std::unique_ptr<TemplateNode>& tpl) {
        if (buffer.empty())
            return;

        tpl->addPart(std::make_unique<ValueNode>(buffer));
        buffer.clear();
    }
    
    std::string Parser::getTokenName(TokenType type) {
        switch (type) {
            case TOKEN_IF: return "IF";
            case TOKEN_LPAREN: return "(";
            case TOKEN_RPAREN: return ")";
            case TOKEN_IDENT: return "IDENT";
            case TOKEN_NUMBER: return "NUMBER";
            case TOKEN_STRING: return "STRING";
            case TOKEN_OP: return "OP";
            case TOKEN_BOOL_OP: return "BOOL_OP";
            case TOKEN_EOF: return "EOF";
            case TOKEN_QUESTION: return "?";
            case TOKEN_COLON: return ":";
            case TOKEN_BOOL_LIT: return "BOOL_LIT";
            default: return "UNKNOWN";
        }
    }
}