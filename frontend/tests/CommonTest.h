#pragma once

#include <string>
#include <memory>
#include <stdexcept>

#include "LOICollectionA/frontend/DiagnosticEngine.h"
#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
#include "LOICollectionA/frontend/SemanticAnalyzer.h"
#include "LOICollectionA/frontend/Callback.h"
#include "LOICollectionA/frontend/ir/Compiler.h"
#include "LOICollectionA/frontend/ir/Optimizer.h"
#include "LOICollectionA/frontend/ir/VM.h"

namespace LOICollection::frontend {
    // The native stdlib (ArrayClass etc.) is not linked into the standalone
    // core, so register just the minimal Array methods the tests rely on.
    inline void ensureNativeArrayMethods() {
        static const bool registered = [] {
            ClassCall& cc = ClassCall::getInstance();
            cc.registerClass("Array", {});
            auto push = [](const TypedValue& self, const CallbackTypeValues& args) -> Expected<TypedValue> {
                auto arr = std::get<ArrayRef>(self);
                arr->elements.push_back(args[0]);
                return static_cast<int>(arr->elements.size());
            };
            for (ParamType type : { ParamType::INT, ParamType::FLOAT, ParamType::STRING, ParamType::BOOL,
                                    ParamType::OBJECT, ParamType::FUNCTION, ParamType::ARRAY })
                cc.registerValueMethod("Array", "push", push, { type });
            return true;
        }();
        (void)registered;
    }

    inline std::string eval(const std::string& input, const Context& ctx = {}) {
        ensureNativeArrayMethods();
        DiagnosticEngine diagnostics;

        Lexer lexer(input, diagnostics);
        Parser parser(lexer, diagnostics);

        auto ast = parser.parse();
        if (diagnostics.hasErrors())
            throw std::runtime_error(diagnostics.getErrorMessage());

        if (ast->getType() == ASTNode::Type::Program) {
            SemanticAnalyzer analyzer(diagnostics);
            analyzer.analyze(static_cast<ProgramNode&>(*ast));
        }

        if (diagnostics.hasErrors())
            throw std::runtime_error(diagnostics.getErrorMessage());

        ir::Compiler compiler(diagnostics);
        ir::VM vm(diagnostics);

        auto bytecode = std::make_shared<ir::BytecodeChunk>(compiler.compile(*ast));
        if (diagnostics.hasErrors())
            throw std::runtime_error(diagnostics.getErrorMessage());

        ir::Optimizer optimizer;
        [[maybe_unused]] auto optimizeStats = optimizer.optimize(*bytecode);

        auto result = vm.run(bytecode, ctx);
        if (diagnostics.hasErrors())
            throw std::runtime_error(diagnostics.getErrorMessage());

        return ir::VM::valueToString(result);
    }
}
