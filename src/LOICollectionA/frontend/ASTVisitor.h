#pragma once

namespace LOICollection::frontend {
    struct ASTNode;
    struct ExprNode;
    struct ValueNode;
    struct VariableNode;
    struct AssignmentNode;
    struct IfNode;
    struct CompareNode;
    struct LogicalNode;
    struct FunctionNode;
    struct MacroNode;
    struct ArithmeticNode;
    struct UnaryNode;
    struct TemplateNode;
    struct ClassNode;
    struct ReturnNode;
    struct NewNode;
    struct MemberAccessNode;
    struct MethodCallNode;
    struct ThisNode;
    struct FunctionDefNode;
    struct FuncCallNode;
    struct LambdaNode;

    class ASTVisitor {
    public:
        virtual ~ASTVisitor() = default;

        virtual void visit(ValueNode& node) = 0;
        virtual void visit(VariableNode& node) = 0;
        virtual void visit(AssignmentNode& node) = 0;
        virtual void visit(IfNode& node) = 0;
        virtual void visit(CompareNode& node) = 0;
        virtual void visit(LogicalNode& node) = 0;
        virtual void visit(FunctionNode& node) = 0;
        virtual void visit(MacroNode& node) = 0;
        virtual void visit(ArithmeticNode& node) = 0;
        virtual void visit(UnaryNode& node) = 0;
        virtual void visit(TemplateNode& node) = 0;
        virtual void visit(ClassNode& node) = 0;
        virtual void visit(ReturnNode& node) = 0;
        virtual void visit(NewNode& node) = 0;
        virtual void visit(MemberAccessNode& node) = 0;
        virtual void visit(MethodCallNode& node) = 0;
        virtual void visit(ThisNode& node) = 0;
        virtual void visit(FunctionDefNode& node) = 0;
        virtual void visit(FuncCallNode& node) = 0;
        virtual void visit(LambdaNode& node) = 0;
    };
}
