#pragma once

#include <any>
#include <string>
#include <unordered_map>

#include "LOICollectionA/frontend/AST.h"

#include "LOICollectionA/base/Macro.h"

namespace LOICollection::frontend {
    struct Context {
        std::unordered_map<int, std::any> params;

        template <typename... Args>
        Context(Args&&... args) {
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((params[Is] = std::forward<Args>(args)), ...);
            }(std::make_index_sequence<sizeof...(Args)>{});
        }

        Context(const Context&) = delete;
        Context(Context&&) = delete;
        Context& operator=(const Context&) = delete;
        Context& operator=(Context&&) = delete;
    };

    class Evaluator {
    public:
        LOICOLLECTION_A_NDAPI std::string evaluate(const ASTNode& root, const Context& ctx = {});

    private:
        using Value = ValueNode::ValueType;

    private:
        Value evalExpr(const ExprNode& expr, const Context& ctx);

        bool evalCondition(const ExprNode& expr, const Context& ctx);

        Value evalTemplate(const TemplateNode& tpl, const Context& ctx);
        Value evalIf(const IfNode& ifNode, const Context& ctx);
        
        std::string evalFunction(const FunctionNode& func, const Context& ctx);
        std::string evalMacro(const MacroNode& macro, const Context& ctx);

        Value evalNode(const ASTNode& node, const Context& ctx);
        
        std::string valueToString(const Value& val);

        Value stringToValue(const std::string& str);

        Value applyArithmetic(const Value& left, const Value& right, const std::string& op);
        Value applyUnary(const Value& operand, const std::string& op);

        bool valueToBool(const Value& val);
        bool applyComparison(const Value& left, const Value& right, const std::string& op);
    };
}
