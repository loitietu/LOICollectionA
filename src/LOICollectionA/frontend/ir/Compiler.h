#pragma once

#include <string>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/frontend/AST.h"
#include "LOICollectionA/frontend/ir/ByteCode.h"

namespace LOICollection::frontend::ir {
    class Compiler {
    public:
        LOICOLLECTION_A_NDAPI BytecodeChunk compile(const ASTNode& root);

    private:
        BytecodeChunk chunk;

        void compileNode(const ASTNode& node);
        void compileExpr(const ExprNode& expr);
        void compileTemplate(const TemplateNode& tpl, bool pushAsMultiple = false);

        int addConstant(const ValueNode::ValueType& val);
        int addFunction(const std::string& name, int argCount);
        int addMacro(const std::string& name, int argCount);
    };
}