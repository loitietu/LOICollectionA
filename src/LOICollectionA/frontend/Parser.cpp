#include <memory>
#include <string>
#include <unordered_set>

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Lexer.h"

#include "LOICollectionA/frontend/Parser.h"

namespace LOICollection::frontend {
    Parser::Parser(Lexer& l, DiagnosticEngine& diag) : lexer(l), diagnostics(diag) {
        currentToken = lexer.getNextToken();
    }

    std::unique_ptr<ASTNode> Parser::parse() {
        std::unique_ptr<TemplateNode> tpl = std::make_unique<TemplateNode>();

        while (currentToken.type != TokenType::TOKEN_EOF) {
            auto stmt = parseStatement();
            if (!stmt)
                return nullptr;

            tpl->addPart(std::move(stmt));

            if (currentToken.type == TokenType::TOKEN_SEMICOLON) {
                if (!eat(TokenType::TOKEN_SEMICOLON))
                    return nullptr;
            } else if (currentToken.type != TokenType::TOKEN_EOF) {
                diagnostics.addError(currentToken.loc,
                    "Expected ';' or EOF after statement, got " + getTokenName(currentToken.type) + " (" + currentToken.value + ")");
                
                return nullptr;
            }
        }

        return tpl;
    }

    std::unique_ptr<IfNode> Parser::parseIfStatement() {
        if (!eat(TokenType::TOKEN_IF)) return nullptr;
        if (!eat(TokenType::TOKEN_LPAREN)) return nullptr;

        auto cond = parseBoolExpression();
        if (!cond)
            return nullptr;

        if (!eat(TokenType::TOKEN_RPAREN)) return nullptr;
        if (!eat(TokenType::TOKEN_LBRCKET)) return nullptr;
        
        auto truePart = parseTemplateUntil(TokenType::TOKEN_COLON, true);
        if (!truePart)
            return nullptr;

        if (!eat(TokenType::TOKEN_COLON)) return nullptr;

        auto falsePart = parseTemplateUntil(TokenType::TOKEN_RBRCKET, false);
        if (!falsePart)
            return nullptr;

        if (!eat(TokenType::TOKEN_RBRCKET)) return nullptr;
        
        return std::make_unique<IfNode>(
            std::move(cond),
            std::move(truePart),
            std::move(falsePart)
        );
    }

    std::unique_ptr<FunctionNode> Parser::parseFunction() {
        std::string namespaces = currentToken.value;

        if (!eat(TokenType::TOKEN_IDENT)) return nullptr;
        if (!eat(TokenType::TOKEN_NAMESPACE)) return nullptr;

        std::string name = currentToken.value;

        if (!eat(TokenType::TOKEN_IDENT)) return nullptr;
        if (!eat(TokenType::TOKEN_LPAREN)) return nullptr;

        std::unique_ptr<TemplateNode> args = parseArgs();
        if (!args)
            return nullptr;

        if (!eat(TokenType::TOKEN_RPAREN)) return nullptr;

        return std::make_unique<FunctionNode>(
            std::move(args),
            std::move(namespaces),
            std::move(name)
        );
    }

    std::unique_ptr<MacroNode> Parser::parseMacro() {
        if (!eat(TokenType::TOKEN_LBRACE)) return nullptr;

        std::string name = currentToken.value;

        if (!eat(TokenType::TOKEN_IDENT)) return nullptr;

        std::unique_ptr<TemplateNode> args;
        if (currentToken.type == TokenType::TOKEN_LPAREN) {
            if (!eat(TokenType::TOKEN_LPAREN)) return nullptr;

            args = parseArgs();
            if (!args)
                return nullptr;

            if (!eat(TokenType::TOKEN_RPAREN)) return nullptr;
        }

        if (!eat(TokenType::TOKEN_RBRACE)) return nullptr;

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

            auto stmt = parseStatement();
            if (!stmt)
                return nullptr;

            tpl->addPart(std::move(stmt));

            if (currentToken.type != TokenType::TOKEN_COLON && currentToken.type != TokenType::TOKEN_RBRCKET) {
                if (!eat(TokenType::TOKEN_SEMICOLON))
                    return nullptr;
            }
        }

        if (tpl->parts.size() == 1) {
            auto& only = tpl->parts[0];
            if (dynamic_cast<ExprNode*>(only.get()))
                return std::move(only);
        }

        return tpl;
    }

    std::unique_ptr<ValueNode> Parser::parseTranspile() {
        if (!eat(TokenType::TOKEN_TRANSPILE)) return nullptr;

        std::string buffer;
        while (currentToken.type != TokenType::TOKEN_EOF) {
            if (currentToken.type == TokenType::TOKEN_RBRACE || currentToken.type == TokenType::TOKEN_SEMICOLON) {
                buffer += currentToken.value;

                eat(currentToken.type);

                break;
            }

            buffer += currentToken.value;
            eat(currentToken.type);
        }

        return std::make_unique<ValueNode>(std::move(buffer));
    }

    std::unique_ptr<TemplateNode> Parser::parseArgs(TokenType delimiterToken, TokenType stopToken) {
        auto tpl = std::make_unique<TemplateNode>();

        while (currentToken.type != stopToken) {
            if (tpl->parts.size() >= 100) {
                diagnostics.addError(currentToken.loc, "Too many args in function call");
                return nullptr;
            }

            if (currentToken.type == delimiterToken) {
                if (!eat(delimiterToken)) return nullptr;
                continue;
            }

            auto expr = parseBaseExpression();
            if (!expr)
                return nullptr;

            tpl->addPart(std::move(expr));
        }

        return tpl;
    }

    std::unique_ptr<ASTNode> Parser::parseStatement() {
        if (currentToken.type == TokenType::TOKEN_IDENT) {
            Token next = lexer.peekNextToken();
            if (next.type == TokenType::TOKEN_OP && next.value == "=") {
                std::string varName = currentToken.value;

                if (!eat(TokenType::TOKEN_IDENT)) return nullptr;
                if (!eat(TokenType::TOKEN_OP)) return nullptr;

                auto right = parseBaseExpression();
                if (!right)
                    return nullptr;

                return std::make_unique<AssignmentNode>(varName, std::move(right));
            }
        }

        return parseBaseExpression();
    }

    std::unique_ptr<ExprNode> Parser::parseBaseExpression() {
        return parseBoolExpression();
    }

    std::unique_ptr<ExprNode> Parser::parseBoolExpression() {
        return parseOrExpression();
    }

    std::unique_ptr<ExprNode> Parser::parseOrExpression() {
        auto left = parseAndExpression();
        if (!left)
            return nullptr;

        while (currentToken.type == TokenType::TOKEN_BOOL_OP && currentToken.value == "||") {
            if (!eat(TokenType::TOKEN_BOOL_OP)) return nullptr;

            auto right = parseAndExpression();
            if (!right)
                return nullptr;

            left = std::make_unique<LogicalNode>(std::move(left), std::move(right), "||");
        }
        
        return left;
    }
    
    std::unique_ptr<ExprNode> Parser::parseAndExpression() {
        auto left = parseComparison();
        if (!left)
            return nullptr;
        
        while (currentToken.type == TokenType::TOKEN_BOOL_OP && currentToken.value == "&&") {
            if (!eat(TokenType::TOKEN_BOOL_OP)) return nullptr;

            auto right = parseComparison();
            if (!right)
                return nullptr;

            left = std::make_unique<LogicalNode>(std::move(left), std::move(right), "&&");
        }

        return left;
    }

    std::unique_ptr<ExprNode> Parser::parseComparison() {
        auto left = parseAdditiveExpression();
        if (!left)
            return nullptr;
        
        static const std::unordered_set<std::string> comparisonOps = {
            "==", "!=", ">", "<", ">=", "<="
        };
        
        if (currentToken.type == TokenType::TOKEN_OP && comparisonOps.find(currentToken.value) != comparisonOps.end()) {
            std::string op = currentToken.value;

            if (!eat(TokenType::TOKEN_OP)) return nullptr;

            auto right = parseAdditiveExpression();
            if (!right)
                return nullptr;

            return std::make_unique<CompareNode>(std::move(left), std::move(right), op);
        }

        return left;
    }

    std::unique_ptr<ExprNode> Parser::parseAdditiveExpression() {
        auto left = parseMultiplicativeExpression();
        if (!left)
            return nullptr;
        
        while (currentToken.type == TokenType::TOKEN_PLUS || currentToken.type == TokenType::TOKEN_MINUS) {
            std::string op = currentToken.value;
            
            if (currentToken.type == TokenType::TOKEN_PLUS) {
                if (!eat(TokenType::TOKEN_PLUS))  return nullptr;
            } else {
                if (!eat(TokenType::TOKEN_MINUS)) return nullptr;
            }
            
            auto right = parseMultiplicativeExpression();
            if (!right)
                return nullptr;

            left = std::make_unique<ArithmeticNode>(std::move(left), std::move(right), op);
        }
        
        return left;
    }

    std::unique_ptr<ExprNode> Parser::parseMultiplicativeExpression() {
        auto left = parsePowerExpression();
        if (!left)
            return nullptr;
        
        while (currentToken.type == TokenType::TOKEN_MULTIPLY || currentToken.type == TokenType::TOKEN_DIVIDE || currentToken.type == TokenType::TOKEN_MOD) {
            std::string op = currentToken.value;
            
            if (currentToken.type == TokenType::TOKEN_MULTIPLY) {
                if (!eat(TokenType::TOKEN_MULTIPLY)) return nullptr;
            } else if (currentToken.type == TokenType::TOKEN_DIVIDE) {
                if (!eat(TokenType::TOKEN_DIVIDE)) return nullptr;
            } else {
                if (!eat(TokenType::TOKEN_MOD)) return nullptr;
            }
            
            auto right = parsePowerExpression();
            if (!right)
                return nullptr;

            left = std::make_unique<ArithmeticNode>(std::move(left), std::move(right), op);
        }
        
        return left;
    }

    std::unique_ptr<ExprNode> Parser::parsePowerExpression() {
        auto left = parseUnaryExpression();
        if (!left)
            return nullptr;
        
        if (currentToken.type == TokenType::TOKEN_POWER) {
            std::string op = currentToken.value;

            if (!eat(TokenType::TOKEN_POWER)) return nullptr;

            auto right = parsePowerExpression();
            if (!right)
                return nullptr;

            return std::make_unique<ArithmeticNode>(std::move(left), std::move(right), op);
        }
        
        return left;
    }

    std::unique_ptr<ExprNode> Parser::parseUnaryExpression() {
        if ((currentToken.type == TokenType::TOKEN_OP && currentToken.value == "!") ||
            currentToken.type == TokenType::TOKEN_PLUS || currentToken.type == TokenType::TOKEN_MINUS) {
            std::string op = currentToken.value;
            
            if (currentToken.type == TokenType::TOKEN_OP && currentToken.value == "!") {
                if (!eat(TokenType::TOKEN_OP)) return nullptr;
            } else if (currentToken.type == TokenType::TOKEN_PLUS) {
                if (!eat(TokenType::TOKEN_PLUS))  return nullptr;
            } else {
                if (!eat(TokenType::TOKEN_MINUS)) return nullptr;
            }
            
            auto operand = parseUnaryExpression();
            if (!operand)
                return nullptr;

            return std::make_unique<UnaryNode>(std::move(operand), op);
        }
        
        return parsePrimary();
    }

    std::unique_ptr<ExprNode> Parser::parsePrimary() {
        switch (currentToken.type) {
            case TokenType::TOKEN_IF:
                return parseIfStatement();
            case TokenType::TOKEN_LPAREN: {
                if (!eat(TokenType::TOKEN_LPAREN)) return nullptr;
                
                auto expr = parseBaseExpression();
                if (!expr)
                    return nullptr;

                if (!eat(TokenType::TOKEN_RPAREN)) return nullptr;
                return expr;
            }
            case TokenType::TOKEN_IDENT: {
                if (peek() == TokenType::TOKEN_NAMESPACE)
                    return parseFunction();

                std::string name = currentToken.value;

                if (!eat(TokenType::TOKEN_IDENT)) return nullptr;

                return std::make_unique<VariableNode>(std::move(name));
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
                int value;
                auto [ptr, ec] = std::from_chars(currentToken.value.data(), currentToken.value.data() + currentToken.value.size(), value);

                if (ec != std::errc() || ptr != currentToken.value.data() + currentToken.value.size()) {
                    diagnostics.addError(currentToken.loc, "Invalid integer literal: " + currentToken.value);

                    eat(TokenType::TOKEN_INT);
                    return std::make_unique<ValueNode>(0);
                }

                eat(TokenType::TOKEN_INT);
                return std::make_unique<ValueNode>(value);
            }
            case TokenType::TOKEN_FLOAT: {
                float value;
                auto [ptr, ec] = std::from_chars(currentToken.value.data(), currentToken.value.data() + currentToken.value.size(), value);

                if (ec != std::errc() || ptr != currentToken.value.data() + currentToken.value.size()) {
                    diagnostics.addError(currentToken.loc, "Invalid float literal: " + currentToken.value);
                    
                    eat(TokenType::TOKEN_FLOAT);
                    return std::make_unique<ValueNode>(0.0f);
                }

                eat(TokenType::TOKEN_FLOAT);
                return std::make_unique<ValueNode>(value);
            }
            case TokenType::TOKEN_STRING: {
                std::string str = std::move(currentToken.value);

                eat(TokenType::TOKEN_STRING);
                return std::make_unique<ValueNode>(std::move(str));
            }
            case TokenType::TOKEN_BOOL_LIT: {
                bool val = (currentToken.value == "true");
                
                eat(TokenType::TOKEN_BOOL_LIT);
                return std::make_unique<ValueNode>(val);
            }
            default:
                diagnostics.addError(currentToken.loc, "Unexpected value type: " + currentToken.value);
                return std::make_unique<ValueNode>(0);
        }
    }

    TokenType Parser::peek() {
        return lexer.peekNextToken().type;
    }

    bool Parser::eat(TokenType expected) {
        if (currentToken.type != expected) {
            diagnostics.addError(currentToken.loc,
                "Syntax error: Expected " + getTokenName(expected) + ", got " + getTokenName(currentToken.type));
            return false;
        }

        currentToken = lexer.getNextToken();
        return true;
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
            case TokenType::TOKEN_SEMICOLON: return ";";
            case TokenType::TOKEN_EOF: return "EOF";
            default: return "UNKNOWN";
        }
    }
}
