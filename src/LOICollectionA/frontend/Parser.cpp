#include <memory>
#include <string>
#include <stdexcept>
#include <unordered_set>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Lexer.h"

#include "LOICollectionA/frontend/Parser.h"

namespace LOICollection::frontend {
    Parser::Parser(Lexer& l) : lexer(l) {
        currentToken = lexer.getNextToken();
    }

    std::unique_ptr<ASTNode> Parser::parse() {
        std::unique_ptr<TemplateNode> tpl = std::make_unique<TemplateNode>();

        while (currentToken.type != TokenType::TOKEN_EOF) {
            if (tpl->parts.size() >= 100)
                throw std::runtime_error("Too many parts in template");

            tpl->addPart(parseBaseExpression());
        }

        return tpl;
    }

    std::unique_ptr<IfNode> Parser::parseIfStatement() {
        eat(TokenType::TOKEN_IF);
        eat(TokenType::TOKEN_LPAREN);

        auto cond = parseBoolExpression();

        eat(TokenType::TOKEN_RPAREN);
        eat(TokenType::TOKEN_LBRCKET);
        
        auto truePart = parseTemplateUntil(TokenType::TOKEN_COLON, true);

        eat(TokenType::TOKEN_COLON);

        auto falsePart = parseTemplateUntil(TokenType::TOKEN_RBRCKET, false);

        eat(TokenType::TOKEN_RBRCKET);
        
        return std::make_unique<IfNode>(
            std::move(cond),
            std::move(truePart),
            std::move(falsePart)
        );
    }

    std::unique_ptr<FunctionNode> Parser::parseFunction() {
        std::string namespaces = currentToken.value;

        eat(TokenType::TOKEN_IDENT);
        eat(TokenType::TOKEN_NAMESPACE);

        std::string name = currentToken.value;

        eat(TokenType::TOKEN_IDENT);
        eat(TokenType::TOKEN_LPAREN);

        std::unique_ptr<ASTNode> args = parseArgs();

        eat(TokenType::TOKEN_RPAREN);

        return std::make_unique<FunctionNode>(
            std::move(args),
            std::move(namespaces),
            std::move(name)
        );
    }

    std::unique_ptr<MacroNode> Parser::parseMacro() {
        eat(TokenType::TOKEN_LBRACE);

        std::string name = currentToken.value;

        eat(TokenType::TOKEN_IDENT);

        std::unique_ptr<ASTNode> args;
        if (currentToken.type == TokenType::TOKEN_LPAREN) {
            eat(TokenType::TOKEN_LPAREN);

            args = parseArgs();

            eat(TokenType::TOKEN_RPAREN);
        }

        eat(TokenType::TOKEN_RBRACE);

        return std::make_unique<MacroNode>(
            std::move(args),
            std::move(name)
        );
    }

    std::unique_ptr<ASTNode> Parser::parseTemplateUntil(TokenType stopToken, bool stopOnColon) {
        auto tpl = std::make_unique<TemplateNode>();

        int bracketDepth = 0;
        while (currentToken.type != TokenType::TOKEN_EOF) {
            if (currentToken.type == TokenType::TOKEN_LBRCKET) {
                bracketDepth++;
            } else if (currentToken.type == TokenType::TOKEN_RBRCKET) {
                if (bracketDepth == 0 && stopToken == TokenType::TOKEN_RBRCKET && !stopOnColon)
                    break;
                
                bracketDepth--;
            }

            if (bracketDepth == 0) {
                if (stopOnColon && currentToken.type == TokenType::TOKEN_COLON)
                    break;

                if (!stopOnColon && currentToken.type == stopToken)
                    break;
            }

            if (currentToken.type == TokenType::TOKEN_IF) {
                tpl->addPart(parseIfStatement()); 
                continue;
            }

            tpl->addPart(parseBaseExpression());
        }

        if (tpl->parts.size() == 1) {
            auto& only = tpl->parts[0];
            if (dynamic_cast<ExprNode*>(only.get()))
                return std::move(only);
        }

        return tpl;
    }

    std::unique_ptr<ValueNode> Parser::parseTranspile(TokenType stopToken) {
        eat(TokenType::TOKEN_TRANSPILE);

        std::string buffer;
        while (currentToken.type != stopToken && currentToken.type != TokenType::TOKEN_EOF) {
            buffer += currentToken.value;
            eat(currentToken.type);
        }

        buffer += currentToken.value;
        
        eat(stopToken);

        return std::make_unique<ValueNode>(std::move(buffer));
    }

    std::unique_ptr<TemplateNode> Parser::parseArgs(TokenType delimiterToken, TokenType stopToken) {
        auto tpl = std::make_unique<TemplateNode>();

        while (currentToken.type != stopToken) {
            if (tpl->parts.size() >= 100)
                throw std::runtime_error("Too many args in function call");

            if (currentToken.type == delimiterToken) {
                eat(delimiterToken);
                continue;
            }

            tpl->addPart(parseBaseExpression());
        }

        return tpl;
    }

    std::unique_ptr<ExprNode> Parser::parseBaseExpression() {
        return parseBoolExpression();
    }

    std::unique_ptr<ExprNode> Parser::parseBoolExpression() {
        return parseOrExpression();
    }

    std::unique_ptr<ExprNode> Parser::parseOrExpression() {
        auto left = parseAndExpression();

        while (currentToken.type == TokenType::TOKEN_BOOL_OP && currentToken.value == "||") {
            eat(TokenType::TOKEN_BOOL_OP);

            auto right = parseAndExpression();
            left = std::make_unique<LogicalNode>(std::move(left), std::move(right), "||");
        }
        
        return left;
    }
    
    std::unique_ptr<ExprNode> Parser::parseAndExpression() {
        auto left = parseComparison();
        
        while (currentToken.type == TokenType::TOKEN_BOOL_OP && currentToken.value == "&&") {
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
        
        if (currentToken.type == TokenType::TOKEN_OP && comparisonOps.find(currentToken.value) != comparisonOps.end()) {
            std::string op = currentToken.value;

            eat(TokenType::TOKEN_OP);

            auto right = parseAdditiveExpression();
            return std::make_unique<CompareNode>(std::move(left), std::move(right), op);
        }

        return left;
    }

    std::unique_ptr<ExprNode> Parser::parseAdditiveExpression() {
        auto left = parseMultiplicativeExpression();
        
        while (currentToken.type == TokenType::TOKEN_PLUS || currentToken.type == TokenType::TOKEN_MINUS) {
            std::string op = currentToken.value;
            
            if (currentToken.type == TokenType::TOKEN_PLUS)
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
        
        while (currentToken.type == TokenType::TOKEN_MULTIPLY || currentToken.type == TokenType::TOKEN_DIVIDE || currentToken.type == TokenType::TOKEN_MOD) {
            std::string op = currentToken.value;
            
            if (currentToken.type == TokenType::TOKEN_MULTIPLY)
                eat(TokenType::TOKEN_MULTIPLY);
            else if (currentToken.type == TokenType::TOKEN_DIVIDE)
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
        
        while (currentToken.type == TokenType::TOKEN_POWER) {
            std::string op = currentToken.value;

            eat(TokenType::TOKEN_POWER);

            auto right = parseUnaryExpression();
            left = std::make_unique<ArithmeticNode>(std::move(left), std::move(right), op);
        }
        
        return left;
    }

    std::unique_ptr<ExprNode> Parser::parseUnaryExpression() {
        if ((currentToken.type == TokenType::TOKEN_OP && currentToken.value == "!") ||
            currentToken.type == TokenType::TOKEN_PLUS || currentToken.type == TokenType::TOKEN_MINUS) {
            std::string op = currentToken.value;
            
            if (currentToken.type == TokenType::TOKEN_OP && currentToken.value == "!")
                eat(TokenType::TOKEN_OP);
            else if (currentToken.type == TokenType::TOKEN_PLUS)
                eat(TokenType::TOKEN_PLUS);
            else
                eat(TokenType::TOKEN_MINUS);
            
            auto operand = parseUnaryExpression();
            return std::make_unique<UnaryNode>(std::move(operand), op);
        }
        
        return parsePrimary();
    }

    std::unique_ptr<ExprNode> Parser::parsePrimary() {
        switch (currentToken.type) {
            case TokenType::TOKEN_IF:
                return parseIfStatement();
            case TokenType::TOKEN_LPAREN: {
                eat(TokenType::TOKEN_LPAREN);
                
                auto expr = parseBaseExpression();

                eat(TokenType::TOKEN_RPAREN);
                return expr;
            }
            case TokenType::TOKEN_IDENT: {
                if (peek() == TokenType::TOKEN_NAMESPACE)
                    return parseFunction();

                break;
            }
            case TokenType::TOKEN_TRANSPILE:
                return parseTranspile();
            case TokenType::TOKEN_LBRACE:
                return parseMacro();
            default:
                break;
        }

        return parseValue();
    }

    std::unique_ptr<ValueNode> Parser::parseValue() {
        switch (currentToken.type) {
            case TokenType::TOKEN_INT: {
                int value = std::stoi(currentToken.value);

                eat(TokenType::TOKEN_INT);
                return std::make_unique<ValueNode>(value);
            }
            case TokenType::TOKEN_FLOAT: {
                float value = std::stof(currentToken.value);

                eat(TokenType::TOKEN_FLOAT);
                return std::make_unique<ValueNode>(value);
            }
            case TokenType::TOKEN_STRING: {
                std::string str = std::move(currentToken.value);

                eat(TokenType::TOKEN_STRING);
                return std::make_unique<ValueNode>(std::move(str));
            }
            case TokenType::TOKEN_IDENT: {
                std::string text = std::move(currentToken.value);

                eat(TokenType::TOKEN_IDENT);
                return std::make_unique<ValueNode>(std::move(text));
            }
            case TokenType::TOKEN_BOOL_LIT: {
                bool val = (currentToken.value == "true");
                
                eat(TokenType::TOKEN_BOOL_LIT);
                return std::make_unique<ValueNode>(val);
            }
            default:
                throw std::runtime_error("Unexpected value type: " + currentToken.value);
        }
    }

    TokenType Parser::peek() {
        return lexer.peekNextToken().type;
    }

    void Parser::eat(TokenType expected) {
        if (currentToken.type != expected) {
            throw std::runtime_error("Syntax error at position " + std::to_string(currentToken.pos)
                + ": Expected " + getTokenName(expected) + ", got " + getTokenName(currentToken.type));
        }

        currentToken = lexer.getNextToken();
    }
    
    std::string Parser::getTokenName(TokenType type) {
        switch (type) {
            case TokenType::TOKEN_IF: return "IF";
            case TokenType::TOKEN_LPAREN: return "(";
            case TokenType::TOKEN_RPAREN: return ")";
            case TokenType::TOKEN_LBRCKET: return "[";
            case TokenType::TOKEN_RBRCKET: return "]";
            case TokenType::TOKEN_LBRACE: return "{";
            case TokenType::TOKEN_RBRACE: return "}";
            case TokenType::TOKEN_IDENT: return "IDENT";
            case TokenType::TOKEN_INT: return "NUMBER";
            case TokenType::TOKEN_FLOAT: return "FLOAT";
            case TokenType::TOKEN_STRING: return "STRING";
            case TokenType::TOKEN_OP: return "OP";
            case TokenType::TOKEN_BOOL_OP: return "BOOL_OP";
            case TokenType::TOKEN_COLON: return ":";
            case TokenType::TOKEN_BOOL_LIT: return "BOOL_LIT";
            case TokenType::TOKEN_PLUS: return "+";
            case TokenType::TOKEN_MINUS: return "-";
            case TokenType::TOKEN_MULTIPLY: return "*";
            case TokenType::TOKEN_DIVIDE: return "/";
            case TokenType::TOKEN_MOD: return "%";
            case TokenType::TOKEN_POWER: return "^";
            case TokenType::TOKEN_NAMESPACE: return "NAMESPACE";
            case TokenType::TOKEN_COMMA: return ",";
            case TokenType::TOKEN_TRANSPILE: return "TRANSPILE";
            case TokenType::TOKEN_EOF: return "EOF";
            default: return "UNKNOWN";
        }
    }
}
