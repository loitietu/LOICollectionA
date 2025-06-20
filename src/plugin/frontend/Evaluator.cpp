#include <string>
#include <stdexcept>
#include <variant>

#include "frontend/Evaluator.h"

namespace LOICollection::frontend {
    std::string Evaluator::evaluate(const ASTNode& root) {
        return evalNode(root);
    }

    Evaluator::Value Evaluator::evalExpr(const ExprNode& expr) {
        switch (expr.getType()) {
            case ASTNode::Type::Value: {
                const auto& v = static_cast<const ValueNode&>(expr);
                return v.value;
            }
            case ASTNode::Type::Compare: {
                const auto& cmp = static_cast<const CompareNode&>(expr);
                Value left = evalExpr(*cmp.left);
                Value right = evalExpr(*cmp.right);
                return applyComparison(left, right, cmp.op);
            }
            case ASTNode::Type::Logical: {
                const auto& log = static_cast<const LogicalNode&>(expr);
                
                bool left = valueToBool(evalExpr(*log.left));

                if (log.op == "&&" && !left) return false;
                if (log.op == "||" && left) return true;
            
                bool right = valueToBool(evalExpr(*log.right));
                return (log.op == "&&") ? (left && right) : (left || right);
            }
            default:
                throw std::runtime_error("Unsupported expression node type");
        }
    }

    bool Evaluator::evalCondition(const ExprNode& expr) {
        return valueToBool(evalExpr(expr));
    }

    std::string Evaluator::evalTemplate(const TemplateNode& tpl) {
        std::string result;
        for (const auto& part : tpl.parts)
            result.append(evalNode(*part));
        return result;
    }

    std::string Evaluator::evalNode(const ASTNode& node) {
        switch (node.getType()) {
            case ASTNode::Type::Value: {
                const auto& val = static_cast<const ValueNode&>(node);
                return valueToString(val.value);
            }
            case ASTNode::Type::If: {
                const auto& ifNode = static_cast<const IfNode&>(node);
                return evalCondition(*ifNode.condition) ? 
                    evalNode(*ifNode.trueBranch) : 
                    evalNode(*ifNode.falseBranch);
            }
            case ASTNode::Type::Template: {
                const auto& tpl = static_cast<const TemplateNode&>(node);
                return evalTemplate(tpl);
            }
            default:
                throw std::runtime_error("Unsupported AST node type");
        }
    }

    std::string Evaluator::valueToString(const Value& val) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, int>)
                return std::to_string(arg);
            else if constexpr (std::is_same_v<T, std::string>)
                return arg;
        }, val);
    }

    bool Evaluator::valueToBool(const Value& val) {
        return std::visit([](auto&& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, int>)
                return arg != 0;
            else if constexpr (std::is_same_v<T, std::string>)
                return !arg.empty();
        }, val);
    }

    bool Evaluator::applyComparison(const Value& left, const Value& right, const std::string& op) {
        return std::visit([&](auto&& l, auto&& r) -> bool {
            using T = std::decay_t<decltype(l)>;
            using U = std::decay_t<decltype(r)>;
            
            if constexpr (!std::is_same_v<T, U>)
                throw std::runtime_error("Type mismatch in comparison");
            else {
                auto cmp = l <=> r;

                if (op == "==") return cmp == 0;
                if (op == "!=") return cmp != 0;
                if (op == ">") return cmp > 0;
                if (op == "<") return cmp < 0;
                if (op == ">=") return cmp >= 0;
                if (op == "<=") return cmp <= 0;
                
                throw std::runtime_error("Unsupported comparison operator: " + op);
            }
        }, left, right);
    }
}