#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/frontend/Context.h"
#include "LOICollectionA/frontend/ir/ByteCode.h"

namespace LOICollection::frontend::ir {
    class VM {
    public:
        LOICOLLECTION_A_NDAPI ValueNode::ValueType run(const BytecodeChunk& chunk, const Context& ctx = {});

        LOICOLLECTION_A_NDAPI static std::string valueToString(const ValueNode::ValueType& val);
        LOICOLLECTION_A_NDAPI static ValueNode::ValueType applyArithmetic(const ValueNode::ValueType& left, const ValueNode::ValueType& right, const std::string& op);
        LOICOLLECTION_A_NDAPI static ValueNode::ValueType applyUnary(const ValueNode::ValueType& operand, const std::string& op);
        LOICOLLECTION_A_NDAPI static bool valueToBool(const ValueNode::ValueType& val);
        LOICOLLECTION_A_NDAPI static bool applyComparison(const ValueNode::ValueType& left, const ValueNode::ValueType& right, const std::string& op);

    private:
        std::vector<ValueNode::ValueType> stack;
        std::unordered_map<std::string, ValueNode::ValueType> variables;

        size_t ip;

        void push(const ValueNode::ValueType& v);
        ValueNode::ValueType pop();
    };
}
