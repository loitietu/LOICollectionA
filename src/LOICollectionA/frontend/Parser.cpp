#include <memory>
#include <string>
#include <stdexcept>
#include <unordered_set>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Lexer.h"

#include "LOICollectionA/frontend/Parser.h"

namespace LOICollection::frontend {
    std::unique_ptr<ASTNode> Parser::parse() {
        std::unique_ptr<TemplateNode> tpl = std::make_unique<TemplateNode>();

        while (current_token.type != TokenType::TOKEN_EOF) {
            if (tpl->parts.size() >= 100)
                throw std::runtime_error("Too many parts in template");

            if (current_token.type == TokenType::TOKEN_IDENT) tpl->addPart(parseValue());
            if (current_token.type == TokenType::TOKEN_IF) tpl->addPart(parseIfStatement());
        }

        return tpl;
    }

    std::unique_ptr<IfNode> Parser::parseIfStatement(TokenType falsePartToken) {
        eat(TokenType::TOKEN_IF);
        eat(TokenType::TOKEN_LPAREN);

        auto cond = parseBoolExpression();

        eat(TokenType::TOKEN_RPAREN);
        eat(TokenType::TOKEN_LBRCKET);
        
        auto truePart = parseResult(TokenType::TOKEN_COLON);

        eat(TokenType::TOKEN_COLON);

        auto falsePart = parseResult(falsePartToken);

        eat(TokenType::TOKEN_RBRCKET);
        
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
        while (current_token.type == TokenType::TOKEN_BOOL_OP && current_token.value == "||") {
            eat(TokenType::TOKEN_BOOL_OP);

            auto right = parseAndExpression();
            left = std::make_unique<LogicalNode>(std::move(left), std::move(right), "||");
        }
        
        return left;
    }
    
    std::unique_ptr<ExprNode> Parser::parseAndExpression() {
        auto left = parseComparison();
        while (current_token.type == TokenType::TOKEN_BOOL_OP && current_token.value == "&&") {
            eat(TokenType::TOKEN_BOOL_OP);

            auto right = parseComparison();
            left = std::make_unique<LogicalNode>(std::move(left), std::move(right), "&&");
        }

        return left;
    }

    std::unique_ptr<ExprNode> Parser::parseComparison() {
        auto left = parseAdditiveExpression();
        
        static const std::unordered_set<std::string> comparisonOps = {
            "==", "!=", ">", "<", ">=", "<="
        };
        
        if (current_token.type == TokenType::TOKEN_OP && comparisonOps.find(current_token.value) != comparisonOps.end()) {
            std::string op = current_token.value;

            eat(TokenType::TOKEN_OP);

            auto right = parseAdditiveExpression();
            return std::make_unique<CompareNode>(std::move(left), std::move(right), op);
        }

        return left;
    }

    std::unique_ptr<ExprNode> Parser::parseAdditiveExpression() {
        auto left = parseMultiplicativeExpression();
        
        while (current_token.type == TokenType::TOKEN_PLUS || current_token.type == TokenType::TOKEN_MINUS) {
            std::string op = current_token.value;
            
            if (current_token.type == TokenType::TOKEN_PLUS)
                eat(TokenType::TOKEN_PLUS);
            else
                eat(TokenType::TOKEN_MINUS);
            
            auto right = parseMultiplicativeExpression();
            left = std::make_unique<ArithmeticNode>(std::move(left), std::move(right), op);
        }
        
        return left;
    }

    std::unique_ptr<ExprNode> Parser::parseMultiplicativeExpression() {
        auto left = parsePowerExpression();
        
        while (current_token.type == TokenType::TOKEN_MULTIPLY || current_token.type == TokenType::TOKEN_DIVIDE || current_token.type == TokenType::TOKEN_MOD) {
            std::string op = current_token.value;
            
            if (current_token.type == TokenType::TOKEN_MULTIPLY)
                eat(TokenType::TOKEN_MULTIPLY);
            else if (current_token.type == TokenType::TOKEN_DIVIDE)
                eat(TokenType::TOKEN_DIVIDE);
            else
                eat(TokenType::TOKEN_MOD);
            
            auto right = parsePowerExpression();
            left = std::make_unique<ArithmeticNode>(std::move(left), std::move(right), op);
        }
        
        return left;
    }

    std::unique_ptr<ExprNode> Parser::parsePowerExpression() {
        auto left = parseUnaryExpression();
        
        while (current_token.type == TokenType::TOKEN_POWER) {
            std::string op = current_token.value;

            eat(TokenType::TOKEN_POWER);

            auto right = parseUnaryExpression();
            left = std::make_unique<ArithmeticNode>(std::move(left), std::move(right), op);
        }
        
        return left;
    }

    std::unique_ptr<ExprNode> Parser::parseUnaryExpression() {
        if ((current_token.type == TokenType::TOKEN_OP && current_token.value == "!") ||
            current_token.type == TokenType::TOKEN_PLUS || current_token.type == TokenType::TOKEN_MINUS) {
            std::string op = current_token.value;
            
            if (current_token.type == TokenType::TOKEN_OP && current_token.value == "!")
                eat(TokenType::TOKEN_OP);
            else if (current_token.type == TokenType::TOKEN_PLUS)
                eat(TokenType::TOKEN_PLUS);
            else
                eat(TokenType::TOKEN_MINUS);
            
            auto operand = parseUnaryExpression();
            return std::make_unique<UnaryNode>(std::move(operand), op);
        }
        
        return parsePrimary();
    }

    std::unique_ptr<ExprNode> Parser::parsePrimary() {
        if (current_token.type == TokenType::TOKEN_LPAREN) {
            eat(TokenType::TOKEN_LPAREN);

            auto expr = parseBoolExpression();

            eat(TokenType::TOKEN_RPAREN);
            return expr;
        }

        return parseValue();
    }

    std::unique_ptr<ValueNode> Parser::parseValue() {
        switch (current_token.type) {
            case TokenType::TOKEN_NUMBER: {
                int value = std::stoi(current_token.value);

                eat(TokenType::TOKEN_NUMBER);
                return std::make_unique<ValueNode>(value);
            }
            case TokenType::TOKEN_STRING: {
                std::string str = std::move(current_token.value);

                eat(TokenType::TOKEN_STRING);
                return std::make_unique<ValueNode>(std::move(str));
            }
            case TokenType::TOKEN_IDENT: {
                std::string text = std::move(current_token.value);

                eat(TokenType::TOKEN_IDENT);
                return std::make_unique<ValueNode>(std::move(text));
            }
            case TokenType::TOKEN_BOOL_LIT: {
                bool val = (current_token.value == "true");
                
                eat(TokenType::TOKEN_BOOL_LIT);
                return std::make_unique<ValueNode>(val ? 1 : 0);
            }
            default:
                throw std::runtime_error("Unexpected value type: " + current_token.value);
        }
    }

    std::unique_ptr<ASTNode> Parser::parseResult(TokenType stopToken) {
        auto tpl = std::make_unique<TemplateNode>();

        std::string buffer;
        while (current_token.type != stopToken && current_token.type != TokenType::TOKEN_EOF) {
            if (current_token.type == TokenType::TOKEN_IF) {
                flushBuffer(buffer, tpl);
                
                tpl->addPart(parseIfStatement(stopToken));
            } else {
                buffer += current_token.value;
                eat(current_token.type);
            }
        }
        flushBuffer(buffer, tpl);
        
        return tpl;
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

        if (tpl->parts.size() >= 100)
            throw std::runtime_error("Too many parts in template");

        tpl->addPart(std::make_unique<ValueNode>(buffer));
        buffer.clear();
    }
    
    std::string Parser::getTokenName(TokenType type) {
        switch (type) {
            case TokenType::TOKEN_IF: return "IF";
            case TokenType::TOKEN_LPAREN: return "(";
            case TokenType::TOKEN_RPAREN: return ")";
            case TokenType::TOKEN_LBRCKET: return "[";
            case TokenType::TOKEN_RBRCKET: return "]";
            case TokenType::TOKEN_IDENT: return "IDENT";
            case TokenType::TOKEN_NUMBER: return "NUMBER";
            case TokenType::TOKEN_STRING: return "STRING";
            case TokenType::TOKEN_OP: return "OP";
            case TokenType::TOKEN_BOOL_OP: return "BOOL_OP";
            case TokenType::TOKEN_EOF: return "EOF";
            case TokenType::TOKEN_COLON: return ":";
            case TokenType::TOKEN_BOOL_LIT: return "BOOL_LIT";
            case TokenType::TOKEN_PLUS: return "+";
            case TokenType::TOKEN_MINUS: return "-";
            case TokenType::TOKEN_MULTIPLY: return "*";
            case TokenType::TOKEN_DIVIDE: return "/";
            case TokenType::TOKEN_MOD: return "%";
            case TokenType::TOKEN_POWER: return "^";
            default: return "UNKNOWN";
        }
    }
}
