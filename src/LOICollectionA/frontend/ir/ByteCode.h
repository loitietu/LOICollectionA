#pragma once

#include <vector>
#include <string>

#include "LOICollectionA/frontend/AST.h"

#include "LOICollectionA/frontend/ir/OpCode.h"

namespace LOICollection::frontend::ir {
    struct Instruction {
        OpCode op;

        int operand; 
    };

    struct FuncMeta {
        std::string name;

        int argCount;
    };

    struct MacroMeta {
        std::string name;

        int argCount;
    };

    struct BytecodeChunk {
        std::vector<Instruction> code;
        std::vector<ValueNode::ValueType> constants;
        std::vector<FuncMeta> functions;
        std::vector<MacroMeta> macros;

        size_t emit(OpCode op, int operand = 0) {
            this->code.push_back({op, operand});
            return this->code.size() - 1;
        }
        
        [[nodiscard]] size_t currentIP() const {
            return this->code.size();
        }

        void patchJump(size_t instrIndex, int targetOffset) {
            this->code[instrIndex].operand = targetOffset;
        }
    };
}
