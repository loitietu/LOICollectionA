#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"

#include "LOICollectionA/frontend/ir/ByteCode.h"

namespace LOICollection::frontend::ir {
    class Compiler : public ASTVisitor {
    public:
        LOICOLLECTION_A_API   Compiler(DiagnosticEngine& diag);

        LOICOLLECTION_A_NDAPI BytecodeChunk compile(ASTNode& root);

    private:
        BytecodeChunk chunk;
        
        std::reference_wrapper<BytecodeChunk> current;

        DiagnosticEngine& diagnostics;

        std::unordered_map<std::string, int> classIndices;
        std::unordered_set<int> registeredClasses;
        std::unordered_map<std::string, std::vector<int>> classMethodIndices;
        std::unordered_map<std::string, std::vector<int>> functionIndices;
        
        size_t methodCount = 0;

        void visit(ValueNode& node) override;
        void visit(VariableNode& node) override;
        void visit(AssignmentNode& node) override;
        void visit(IfNode& node) override;
        void visit(CompareNode& node) override;
        void visit(LogicalNode& node) override;
        void visit(FunctionNode& node) override;
        void visit(MacroNode& node) override;
        void visit(ArithmeticNode& node) override;
        void visit(UnaryNode& node) override;
        void visit(TemplateNode& node) override;
        void visit(ClassNode& node) override;
        void visit(ReturnNode& node) override;
        void visit(NewNode& node) override;
        void visit(MemberAccessNode& node) override;
        void visit(MethodCallNode& node) override;
        void visit(ThisNode& node) override;
        void visit(FunctionDefNode& node) override;
        void visit(FuncCallNode& node) override;
        void visit(LambdaNode& node) override;

        void registerClassMeta(ClassNode& node);
        void compileClassBodies(ClassNode& node);
        void registerFunctionMeta(FunctionDefNode& node);
        void compileFunctionBody(FunctionDefNode& node);

        int addNativeCall(const std::string& className, const std::string& name, int argCount);
        int addConstant(const ValueNode::ValueType& val);
        int addFunction(const std::string& name, int argCount);
        int addMacro(const std::string& name, int argCount);
        int addLambda(int bodyIndex, int argCount, const std::vector<std::string>& paramNames);
    };
}
