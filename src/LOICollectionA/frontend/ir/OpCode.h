#pragma once

#include <cstdint>

namespace LOICollection::frontend::ir {
    enum class OpCode : uint8_t {
        PUSH_INT, PUSH_FLOAT, PUSH_STR, PUSH_BOOL,
        POP, DUP,

        LOAD_VAR, STORE_VAR,

        ADD, SUB, MUL, DIV, MOD, POW,

        CMP_EQ, CMP_NE, CMP_GT, CMP_LT, CMP_GE, CMP_LE,

        LOGIC_AND, LOGIC_OR,

        NEG, NOT,

        CALL,
        CALL_MACRO,

        JMP_IF_FALSE, JMP_IF_TRUE, 
        JMP,
        HALT
    };
}
