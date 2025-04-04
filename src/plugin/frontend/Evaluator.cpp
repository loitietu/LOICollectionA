#include <sstream>
#include <stdexcept>
#include <variant>

#include "frontend/Evaluator.h"

namespace LOICollection::frontend {
    std::string Evaluator::evaluate(const ASTNode& root) {
        if (auto* tpl = dynamic_cast<const TemplateNode*>(&root))
            return evalTemplate(*tpl);
        return evalNode(root);
    }

    Evaluator::Value Evaluator::evalExpr(const ExprNode& expr) {
        if (const auto* v = dynamic_cast<const ValueNode*>(&expr)) {
            return v->value;
        } else if (const auto* cmp = dynamic_cast<const CompareNode*>(&expr)) {
            Value left = evalExpr(*cmp->left);
            Value right = evalExpr(*cmp->right);
            return applyComparison(left, right, cmp->op);
        } else if (const auto* log = dynamic_cast<const LogicalNode*>(&expr)) {
            bool left = valueToBool(evalExpr(*log->left));
            
            if (log->op == "&&" && !left) return false;
            if (log->op == "||" && left) return true;
            
            bool right = valueToBool(evalExpr(*log->right));
            return (log->op == "&&") ? (left && right) : (left || right);
        }
        throw std::runtime_error("Unsupported expression node type");
    }

    bool Evaluator::evalCondition(const ExprNode& expr) {
        return valueToBool(evalExpr(expr));
    }

    std::string Evaluator::evalTemplate(const TemplateNode& tpl) {
        std::ostringstream oss;
        for (const auto& part : tpl.parts)
            oss << evalNode(*part);
        return oss.str();
    }

    std::string Evaluator::evalNode(const ASTNode& node) {
        if (const auto* val = dynamic_cast<const ValueNode*>(&node)) {
            return valueToString(val->value);
        } else if (const auto* ifNode = dynamic_cast<const IfNode*>(&node)) {
            return evalCondition(*ifNode->condition) ? 
                evalNode(*ifNode->trueBranch) : 
                evalNode(*ifNode->falseBranch);
        } else if (const auto* tpl = dynamic_cast<const TemplateNode*>(&node)) {
            return evalTemplate(*tpl);
        }
        throw std::runtime_error("Unsupported AST node type");
    }

    std::string Evaluator::valueToString(const Value& val) {
        if (std::holds_alternative<int>(val))
            return std::to_string(std::get<int>(val));
        return std::get<std::string>(val);
    }

    bool Evaluator::valueToBool(const Value& val) {
        if (std::holds_alternative<int>(val))
            return std::get<int>(val) != 0;
        return !std::get<std::string>(val).empty();
    }

    bool Evaluator::applyComparison(const Value& left, const Value& right, const std::string& op) {
        if (left.index() != right.index())
            throw std::runtime_error("Type mismatch in comparison");

        if (std::holds_alternative<int>(left)) {
            int l = std::get<int>(left);
            int r = std::get<int>(right);
            
            if (op == "==") return l == r;
            if (op == "!=") return l != r;
            if (op == ">")  return l > r;
            if (op == "<")  return l < r;
            if (op == ">=") return l >= r;
            if (op == "<=") return l <= r;
            
        } else if (std::holds_alternative<std::string>(left)) {
            const auto& l = std::get<std::string>(left);
            const auto& r = std::get<std::string>(right);
            
            if (op == "==") return l == r;
            if (op == "!=") return l != r;
            if (op == ">")  return l > r;
            if (op == "<")  return l < r;
            if (op == ">=") return l >= r;
            if (op == "<=") return l <= r;
        }
        
        throw std::runtime_error("Unsupported comparison operator: " + op);
    }
}