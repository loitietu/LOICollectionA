#pragma once

#include <string>
#include <variant>

#include "frontend/AST.h"

namespace LOICollection::frontend {
    class Evaluator {
    public:
        std::string evaluate(const ASTNode& root);

    private:
        using Value = std::variant<int, std::string>;

        Value evalExpr(const ExprNode& expr);

        bool evalCondition(const ExprNode& expr);

        std::string evalTemplate(const TemplateNode& tpl);
        std::string evalNode(const ASTNode& node);
        
        std::string valueToString(const Value& val);

        bool valueToBool(const Value& val);
        bool applyComparison(const Value& left, const Value& right, const std::string& op);
    };
}