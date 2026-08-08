#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
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
        std::unordered_map<std::string, std::reference_wrapper<ClassNode>> classNodes;
        std::unordered_set<int> registeredClasses;
        std::unordered_set<std::string> registeringClasses;
        std::unordered_map<std::string, std::vector<int>> classMethodIndices;
        std::unordered_map<std::string, std::vector<int>> classStaticMethodIndices;
        std::unordered_map<std::string, std::vector<int>> functionIndices;
        std::vector<std::reference_wrapper<ASTNode>> bodyOrder;
        
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
        void visit(ProgramNode& node) override;
        void visit(BlockNode& node) override;
        void visit(ClassNode& node) override;
        void visit(ReturnNode& node) override;
        void visit(NewNode& node) override;
        void visit(MemberAccessNode& node) override;
        void visit(MethodCallNode& node) override;
        void visit(ThisNode& node) override;
        void visit(SuperNode& node) override;
        void visit(SuperCallNode& node) override;
        void visit(InstanceOfNode& node) override;
        void visit(FunctionDefNode& node) override;
        void visit(FuncCallNode& node) override;
        void visit(LambdaNode& node) override;
        void visit(ArrayNode& node) override;
        void visit(IndexAccessNode& node) override;
        void visit(UsingNode& node) override;

        void registerClassMeta(ClassNode& node);
        void compileClassBodies(ClassNode& node);
        void registerFunctionMeta(FunctionDefNode& node);
        void compileFunctionBody(FunctionDefNode& node);
        void compileSequence(SequenceNode& node);
        void compileValue(ExprNode& node);

        [[nodiscard]] std::optional<ValueNode::ValueType> constantValue(ExprNode& node) const;

        int addNativeCall(const std::string& className, const std::string& name, int argCount, bool isStatic = false);
        int addConstant(const ValueNode::ValueType& val);
        int addFunction(const std::string& name, int argCount);
        int addMacro(const std::string& name, int argCount);
        int addLambda(int bodyIndex, int argCount, const std::vector<std::string>& paramNames);
        int addVirtualCall(int classIndex, int ordinal, int argCount);
        int addSuperCall(int constructorIndex, int argCount);

        [[nodiscard]] std::string methodSignature(const MethodDecl& method) const;
    };
}
