#include <cmath>
#include <string>
#include <vector>
#include <variant>
#include <charconv>
#include <stdexcept>
#include <type_traits>

#include "LOICollectionA/utils/core/MathUtils.h"

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/Callback.h"

#include "LOICollectionA/frontend/Evaluator.h"

namespace LOICollection::frontend {
    std::string Evaluator::evaluate(const ASTNode& root, const Context& ctx) {
        return evalNode(root, ctx);
    }

    Evaluator::Value Evaluator::evalExpr(const ExprNode& expr, const Context& ctx) {
        switch (expr.getType()) {
            case ASTNode::Type::Value: return static_cast<const ValueNode&>(expr).value;
            case ASTNode::Type::Macro: return stringToValue(evalMacro(static_cast<const MacroNode&>(expr), ctx));
            case ASTNode::Type::Compare: {
                const auto& cmp = static_cast<const CompareNode&>(expr);

                Value left = evalExpr(*cmp.left, ctx);
                Value right = evalExpr(*cmp.right, ctx);

                return applyComparison(left, right, cmp.op);
            }
            case ASTNode::Type::Logical: {
                const auto& log = static_cast<const LogicalNode&>(expr);
                
                bool left = valueToBool(evalExpr(*log.left, ctx));

                if (log.op == "&&" && !left) return false;
                if (log.op == "||" && left) return true;
            
                bool right = valueToBool(evalExpr(*log.right, ctx));
                return (log.op == "&&") ? (left && right) : (left || right);
            }
            case ASTNode::Type::Arithmetic: {
                const auto& arith = static_cast<const ArithmeticNode&>(expr);

                Value left = evalExpr(*arith.left, ctx);
                Value right = evalExpr(*arith.right, ctx);

                return applyArithmetic(left, right, arith.op);
            }
            case ASTNode::Type::Unary: {
                const auto& unary = static_cast<const UnaryNode&>(expr);

                Value operand = evalExpr(*unary.operand, ctx);

                return applyUnary(operand, unary.op);
            }
            default:
                throw std::runtime_error("Unsupported expression node type");
        }
    }

    bool Evaluator::evalCondition(const ExprNode& expr, const Context& ctx) {
        return valueToBool(evalExpr(expr, ctx));
    }

    std::string Evaluator::evalTemplate(const TemplateNode& tpl, const Context& ctx) {
        std::string result;
        for (const auto& part : tpl.parts)
            result.append(evalNode(*part, ctx));
        return result;
    }

    std::string Evaluator::evalIf(const IfNode& ifNode, const Context& ctx) {
        return evalCondition(*ifNode.condition, ctx) ? 
            evalNode(*ifNode.trueBranch, ctx) :
            evalNode(*ifNode.falseBranch, ctx);
    }

    std::string Evaluator::evalFunction(const FunctionNode& func, const Context& ctx) {
        const auto& args = static_cast<const TemplateNode&>(*func.args);

        std::vector<std::string> values;

        if (func.args && !args.parts.empty()) {
            values.reserve(args.parts.size());
            for (const auto& arg : args.parts)
                values.push_back(evalNode(*arg, ctx));
        }

        return FunctionCall::getInstance().callFunction(func.namespaces, func.name, values, ctx.params);
    }

    std::string Evaluator::evalMacro(const MacroNode& macro, const Context& ctx) {
        const auto& args = static_cast<const TemplateNode&>(*macro.args);

        std::vector<std::string> values;

        if (macro.args && !args.parts.empty()) {
            values.reserve(args.parts.size());
            for (const auto& arg : args.parts)
                values.push_back(evalNode(*arg, ctx));
        }

        return MacroCall::getInstance().callMacro(macro.name, values, ctx.params);
    }

    std::string Evaluator::evalNode(const ASTNode& node, const Context& ctx) {
        switch (node.getType()) {
            case ASTNode::Type::Value: return valueToString(static_cast<const ValueNode&>(node).value);
            case ASTNode::Type::If: return evalIf(static_cast<const IfNode&>(node), ctx);
            case ASTNode::Type::Function: return evalFunction(static_cast<const FunctionNode&>(node), ctx);
            case ASTNode::Type::Macro: return evalMacro(static_cast<const MacroNode&>(node), ctx);
            case ASTNode::Type::Template: return evalTemplate(static_cast<const TemplateNode&>(node), ctx);
            default:
                throw std::runtime_error("Unsupported AST node type");
        }
    }

    std::string Evaluator::valueToString(const Value& val) {
        return std::visit([](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<std::remove_cv_t<T>, int>)
                return std::to_string(arg);
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, float>) {
                std::string result = std::to_string(arg);

                result.erase(result.find_last_not_of('0') + 1, std::string::npos);
                if (result.back() == '.')
                    result.pop_back();

                return result;
            }
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, std::string>)
                return arg;
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, bool>)
                return arg ? "true" : "false";
        }, val);
    }

    Evaluator::Value Evaluator::stringToValue(const std::string& str) {
        if (str == "true") return true;
        if (str == "false") return false;

        if (str.find('.') != std::string::npos) {
            float result;
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);

            if (ec == std::errc() && ptr == str.data() + str.size())
                return result;
        } else {
            int result;
            auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);

            if (ec == std::errc() && ptr == str.data() + str.size())
                return result;
        }

        return str;
    }

    Evaluator::Value Evaluator::applyArithmetic(const Value& left, const Value& right, const std::string& op) {
        return std::visit([&](auto&& l, auto&& r) -> Value {
            using T = std::decay_t<decltype(l)>;
            using U = std::decay_t<decltype(r)>;
            
            if constexpr (!std::is_same_v<T, U>)
                throw std::runtime_error("Type mismatch in arithmetic operation");
            else {
                if (op == "+") return l + r;

                if constexpr (
                    (std::is_same_v<std::remove_cv_t<T>, int> && std::is_same_v<std::remove_cv_t<U>, int>) ||
                    (std::is_same_v<std::remove_cv_t<T>, float> && std::is_same_v<std::remove_cv_t<U>, float>)
                ) {
                    if (op == "-") return l - r;
                    if (op == "*") return l * r;
                    if (op == "/") return l / r;
                }

                if constexpr (std::is_same_v<std::remove_cv_t<T>, int> && std::is_same_v<std::remove_cv_t<U>, int>) {
                    if (op == "%") return l % r;
                    if (op == "^") return MathUtils::pow(l, r);
                }

                if constexpr (std::is_same_v<std::remove_cv_t<T>, float> && std::is_same_v<std::remove_cv_t<U>, float>)
                    if (op == "^") return std::pow(l, r);
                
                throw std::runtime_error("Unsupported arithmetic operator: " + op);
            }
        }, left, right);
    }

    Evaluator::Value Evaluator::applyUnary(const Value& operand, const std::string& op) {
        return std::visit([&](auto&& arg) -> Value {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<std::remove_cv_t<T>, int> || std::is_same_v<std::remove_cv_t<T>, float>) {
                if (op == "+") return arg;
                if (op == "-") return -arg;
            }

            if (op == "!") return !valueToBool(arg);

            throw std::runtime_error("Unsupported unary operator: " + op);
        }, operand);
    }

    bool Evaluator::valueToBool(const Value& val) {
        return std::visit([](auto&& arg) -> bool {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<std::remove_cv_t<T>, int>)
                return arg != 0;
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, float>)
                return std::abs(arg) > std::numeric_limits<float>::epsilon();
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, std::string>)
                return !arg.empty();
            else if constexpr (std::is_same_v<std::remove_cv_t<T>, bool>)
                return arg;
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
