#pragma once

#include <string>
#include <memory>

#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
#include "LOICollectionA/frontend/Evaluator.h"

namespace LOICollection::frontend {
    inline std::string eval(const std::string& input, const Context& ctx = {}) {
        Lexer lexer(input);
        Parser parser(lexer);

        auto ast = parser.parse();

        Evaluator evaluator;
        return evaluator.evaluate(*ast, ctx);
    }
}
