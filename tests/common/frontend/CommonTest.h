#pragma once

#include <string>
#include <memory>

#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
#include "LOICollectionA/frontend/ir/Compiler.h"
#include "LOICollectionA/frontend/ir/VM.h"

namespace LOICollection::frontend {
    inline std::string eval(const std::string& input, const Context& ctx = {}) {
        Lexer lexer(input);
        Parser parser(lexer);

        auto ast = parser.parse();

        ir::Compiler compiler;
        ir::VM vm;

        auto bytecode = compiler.compile(*ast);
        auto result = vm.run(bytecode, ctx);
        return ir::VM::valueToString(result);
    }
}
