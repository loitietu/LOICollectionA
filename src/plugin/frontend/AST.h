#pragma once

#include <memory>
#include <string>
#include <vector>
#include <variant>

namespace LOICollection::frontend {
    struct ASTNode {
        virtual ~ASTNode() = default;
    };

    struct ExprNode : ASTNode {};
    
    struct ValueNode : ExprNode {
        std::variant<int, std::string> value;
        explicit ValueNode(int v) : value(v) {}
        explicit ValueNode(const std::string& v) : value(v) {}
    };

    struct CompareNode : ExprNode {
        std::unique_ptr<ExprNode> left;
        std::unique_ptr<ExprNode> right;
        std::string op;
        
        CompareNode(auto&& l, auto&& r, std::string o)
            : left(std::forward<decltype(l)>(l)),
              right(std::forward<decltype(r)>(r)),
              op(std::move(o)) {}
    };

    struct LogicalNode : ExprNode {
        std::unique_ptr<ExprNode> left;
        std::unique_ptr<ExprNode> right;
        std::string op;
        
        LogicalNode(auto&& l, auto&& r, std::string o) 
            : left(std::forward<decltype(l)>(l)),
              right(std::forward<decltype(r)>(r)),
              op(std::move(o)) {}
    };

    struct IfNode : ASTNode {
        std::unique_ptr<ExprNode> condition;
        std::unique_ptr<ASTNode> trueBranch;
        std::unique_ptr<ASTNode> falseBranch;
        IfNode(auto&& c, auto&& t, auto&& f)
            : condition(std::forward<decltype(c)>(c)), 
              trueBranch(std::forward<decltype(t)>(t)), 
              falseBranch(std::forward<decltype(f)>(f)) {}
    };

    struct TemplateNode : ASTNode {
        std::vector<std::unique_ptr<ASTNode>> parts;

        void addPart(auto&& part) {
            parts.push_back(std::forward<decltype(part)>(part));
        }
    };
}