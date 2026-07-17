#pragma once

namespace LOICollection::frontend {
    struct ASTNode;
    struct ExprNode;
    struct ValueNode;
    struct IfNode;
    struct CompareNode;
    struct LogicalNode;
    struct FunctionNode;
    struct MacroNode;
    struct ArithmeticNode;
    struct UnaryNode;
    struct TemplateNode;

    class ASTVisitor {
    public:
        virtual ~ASTVisitor() = default;

        virtual void visit(ValueNode& node) = 0;
        virtual void visit(IfNode& node) = 0;
        virtual void visit(CompareNode& node) = 0;
        virtual void visit(LogicalNode& node) = 0;
        virtual void visit(FunctionNode& node) = 0;
        virtual void visit(MacroNode& node) = 0;
        virtual void visit(ArithmeticNode& node) = 0;
        virtual void visit(UnaryNode& node) = 0;
        virtual void visit(TemplateNode& node) = 0;
    };
}
