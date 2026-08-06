#pragma once

#include <string>
#include <memory>
#include <stdexcept>

#include "LOICollectionA/frontend/DiagnosticEngine.h"
#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
#include "LOICollectionA/frontend/SemanticAnalyzer.h"
#include "LOICollectionA/frontend/ir/Compiler.h"
#include "LOICollectionA/frontend/ir/Optimizer.h"
#include "LOICollectionA/frontend/ir/VM.h"

namespace LOICollection::frontend {
    inline std::string eval(const std::string& input, const Context& ctx = {}) {
        DiagnosticEngine diagnostics;

        Lexer lexer(input, diagnostics);
        Parser parser(lexer, diagnostics);

        auto ast = parser.parse();
        if (diagnostics.hasErrors())
            throw std::runtime_error(diagnostics.getErrorMessage());

        if (auto tpl = dynamic_cast<TemplateNode*>(ast.get())) {
            SemanticAnalyzer analyzer(diagnostics);
            analyzer.analyze(*tpl);
        }

        if (diagnostics.hasErrors())
            throw std::runtime_error(diagnostics.getErrorMessage());

        ir::Compiler compiler(diagnostics);
        ir::VM vm;

        auto bytecode = compiler.compile(*ast);
        if (diagnostics.hasErrors())
            throw std::runtime_error(diagnostics.getErrorMessage());

        ir::Optimizer optimizer;
        [[maybe_unused]] auto optimizeStats = optimizer.optimize(bytecode);

        auto result = vm.run(bytecode, ctx, diagnostics);
        if (diagnostics.hasErrors())
            throw std::runtime_error(diagnostics.getErrorMessage());

        return ir::VM::valueToString(result);
    }
}
