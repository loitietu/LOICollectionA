#pragma once

#include <memory>
#include <string>
#include <vector>
#include <variant>

namespace LOICollection::frontend {
    struct ASTNode {
        enum class Type { 
            Value, Compare, Logical, If, Template, Expr,
            Arithmetic, Unary 
        };
        [[nodiscard]] virtual Type getType() const = 0;
        
        virtual ~ASTNode() = default;
    };

    struct ExprNode : ASTNode {
        [[nodiscard]] Type getType() const override {
            return Type::Expr;
        }
    };
    
    struct ValueNode : ExprNode {
        std::variant<int, std::string> value;
        explicit ValueNode(int v) : value(v) {}
        explicit ValueNode(const std::string& v) : value(v) {}

        [[nodiscard]] Type getType() const override {
            return Type::Value;
        }
    };

    struct CompareNode : ExprNode {
        std::unique_ptr<ExprNode> left;
        std::unique_ptr<ExprNode> right;
        std::string op;
        
        CompareNode(auto&& l, auto&& r, std::string o)
            : left(std::forward<decltype(l)>(l)),
              right(std::forward<decltype(r)>(r)),
              op(std::move(o)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::Compare;
        }
    };

    struct LogicalNode : ExprNode {
        std::unique_ptr<ExprNode> left;
        std::unique_ptr<ExprNode> right;
        std::string op;
        
        LogicalNode(auto&& l, auto&& r, std::string o) 
            : left(std::forward<decltype(l)>(l)),
              right(std::forward<decltype(r)>(r)),
              op(std::move(o)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::Logical;
        }
    };

    struct IfNode : ASTNode {
        std::unique_ptr<ExprNode> condition;
        std::unique_ptr<ASTNode> trueBranch;
        std::unique_ptr<ASTNode> falseBranch;
        IfNode(auto&& c, auto&& t, auto&& f)
            : condition(std::forward<decltype(c)>(c)), 
              trueBranch(std::forward<decltype(t)>(t)), 
              falseBranch(std::forward<decltype(f)>(f)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::If;
        }
    };

    struct ArithmeticNode : ExprNode {
        std::unique_ptr<ExprNode> left;
        std::unique_ptr<ExprNode> right;
        std::string op;
        
        ArithmeticNode(auto&& l, auto&& r, std::string o)
            : left(std::forward<decltype(l)>(l)),
              right(std::forward<decltype(r)>(r)),
              op(std::move(o)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::Arithmetic;
        }
    };

    struct UnaryNode : ExprNode {
        std::unique_ptr<ExprNode> operand;
        std::string op;
        
        UnaryNode(auto&& expr, std::string o)
            : operand(std::forward<decltype(expr)>(expr)),
              op(std::move(o)) {}
        
        [[nodiscard]] Type getType() const override {
            return Type::Unary;
        }
    };

    struct TemplateNode : ASTNode {
        std::vector<std::unique_ptr<ASTNode>> parts;

        [[nodiscard]] Type getType() const override {
            return Type::Template;
        }

        void addPart(auto&& part) {
            parts.emplace_back(std::forward<decltype(part)>(part));
        }
    };
}
