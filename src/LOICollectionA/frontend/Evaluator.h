#pragma once

#include <string>

#include "LOICollectionA/frontend/AST.h"

namespace LOICollection::frontend {
    class Evaluator final {
    public:
        std::string evaluate(const ASTNode& root);

    private:
        using Value = ValueNode::ValueType;

        Value evalExpr(const ExprNode& expr);

        bool evalCondition(const ExprNode& expr);

        std::string evalTemplate(const TemplateNode& tpl);
        std::string evalNode(const ASTNode& node);
        
        std::string valueToString(const Value& val);

        Value applyArithmetic(const Value& left, const Value& right, const std::string& op);
        Value applyUnary(const Value& operand, const std::string& op);

        bool valueToBool(const Value& val);
        bool applyComparison(const Value& left, const Value& right, const std::string& op);
    };
}
