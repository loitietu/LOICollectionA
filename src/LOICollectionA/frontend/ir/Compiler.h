#pragma once

#include <string>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/ir/ByteCode.h"

namespace LOICollection::frontend::ir {
    class Compiler : public ASTVisitor {
    public:
        LOICOLLECTION_A_NDAPI BytecodeChunk compile(ASTNode& root);

    private:
        BytecodeChunk chunk;

        void visit(ValueNode& node) override;
        void visit(IfNode& node) override;
        void visit(CompareNode& node) override;
        void visit(LogicalNode& node) override;
        void visit(FunctionNode& node) override;
        void visit(MacroNode& node) override;
        void visit(ArithmeticNode& node) override;
        void visit(UnaryNode& node) override;
        void visit(TemplateNode& node) override;

        int addConstant(const ValueNode::ValueType& val);
        int addFunction(const std::string& name, int argCount);
        int addMacro(const std::string& name, int argCount);
    };
}