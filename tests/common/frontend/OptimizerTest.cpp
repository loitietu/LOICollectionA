#include <gtest/gtest.h>

#include <string>

#include "LOICollectionA/frontend/Lexer.h"
#include "LOICollectionA/frontend/Parser.h"
#include "LOICollectionA/frontend/DiagnosticEngine.h"
#include "LOICollectionA/frontend/SemanticAnalyzer.h"

#include "LOICollectionA/frontend/ir/Compiler.h"
#include "LOICollectionA/frontend/ir/Optimizer.h"
#include "LOICollectionA/frontend/ir/VM.h"

using namespace LOICollection::frontend;
using namespace LOICollection::frontend::ir;

namespace {
    struct CompiledProgram {
        std::shared_ptr<BytecodeChunk> chunk;
        DiagnosticEngine diagnostics;
        Optimizer::Stats stats;
    };

    CompiledProgram compileAndOptimize(const std::string& source) {
        CompiledProgram out;

        Lexer lexer(source, out.diagnostics);
        Parser parser(lexer, out.diagnostics);

        auto ast = parser.parse();
        if (ast->getType() == ASTNode::Type::Program) {
            SemanticAnalyzer analyzer(out.diagnostics);
            analyzer.analyze(static_cast<ProgramNode&>(*ast));
        }

        Compiler compiler(out.diagnostics);
        out.chunk = std::make_shared<BytecodeChunk>(compiler.compile(*ast));

        Optimizer optimizer;
        out.stats = optimizer.optimize(*out.chunk);

        return out;
    }
}

TEST(OptimizerTest, ConstantFolding) {
    auto program = compileAndOptimize("1 + 2 * 3");

    EXPECT_FALSE(program.diagnostics.hasErrors());
    EXPECT_GE(program.stats.folded, 2u);

    ASSERT_EQ(program.chunk->code.size(), 2u);
    EXPECT_EQ(program.chunk->code[0].op, OpCode::PUSH_INT);
    EXPECT_EQ(program.chunk->code[1].op, OpCode::HALT);
    EXPECT_EQ(std::get<int>(program.chunk->constants[program.chunk->code[0].operand]), 7);

    VM vm(program.diagnostics);
    EXPECT_EQ(VM::valueToString(vm.run(program.chunk, {})), "7");
    EXPECT_FALSE(program.diagnostics.hasErrors());
}

TEST(OptimizerTest, ConstantConditionElimination) {
    auto program = compileAndOptimize("if(true)[1:2]");

    EXPECT_FALSE(program.diagnostics.hasErrors());
    EXPECT_GE(program.stats.folded, 1u);

    VM vm(program.diagnostics);
    EXPECT_EQ(VM::valueToString(vm.run(program.chunk, {})), "1");
}

TEST(OptimizerTest, UnreachableBranchRemoved) {
    auto program = compileAndOptimize("if(false)[1:2]");

    EXPECT_FALSE(program.diagnostics.hasErrors());
    EXPECT_GT(program.stats.removed, 0u);

    VM vm(program.diagnostics);
    EXPECT_EQ(VM::valueToString(vm.run(program.chunk, {})), "2");
}

TEST(OptimizerTest, FoldsInsideFunctionBodies) {
    auto program = compileAndOptimize("func f() -> int { return 1 + 2; } f()");

    EXPECT_FALSE(program.diagnostics.hasErrors());
    EXPECT_GE(program.stats.folded, 1u);

    VM vm(program.diagnostics);
    EXPECT_EQ(VM::valueToString(vm.run(program.chunk, {})), "3");
}

TEST(OptimizerTest, FoldsLogicalConstants) {
    auto program = compileAndOptimize("true && true");

    EXPECT_FALSE(program.diagnostics.hasErrors());
    EXPECT_GE(program.stats.folded, 1u);

    VM vm(program.diagnostics);
    EXPECT_EQ(VM::valueToString(vm.run(program.chunk, {})), "true");
}

TEST(OptimizerTest, ErroneousConstantsAreNotFolded) {
    auto program = compileAndOptimize("10.0 % 3.0");

    EXPECT_FALSE(program.diagnostics.hasErrors());

    VM vm(program.diagnostics);
    [[maybe_unused]] auto result = vm.run(program.chunk, {});
    EXPECT_TRUE(program.diagnostics.hasErrors());
}

TEST(OptimizerTest, FoldsConstantArrayLiteral) {
    auto program = compileAndOptimize("[1, 2, 3]");

    EXPECT_FALSE(program.diagnostics.hasErrors());
    EXPECT_GE(program.stats.folded, 1u);

    ASSERT_EQ(program.chunk->code.size(), 2u);
    EXPECT_EQ(program.chunk->code[0].op, OpCode::PUSH_INT);
    EXPECT_EQ(program.chunk->code[1].op, OpCode::HALT);

    const auto& value = program.chunk->constants[program.chunk->code[0].operand];
    ASSERT_TRUE(std::holds_alternative<ArrayRef>(value));
    EXPECT_EQ(std::get<ArrayRef>(value)->elements.size(), 3u);

    VM vm(program.diagnostics);
    EXPECT_EQ(VM::valueToString(vm.run(program.chunk, {})), "[1, 2, 3]");
    EXPECT_FALSE(program.diagnostics.hasErrors());
}

TEST(OptimizerTest, FoldsConstantArrayIndex) {
    auto program = compileAndOptimize("[1, 2, 3][1]");

    EXPECT_FALSE(program.diagnostics.hasErrors());
    EXPECT_GE(program.stats.folded, 2u);

    ASSERT_EQ(program.chunk->code.size(), 2u);
    EXPECT_EQ(program.chunk->code[0].op, OpCode::PUSH_INT);
    EXPECT_EQ(std::get<int>(program.chunk->constants[program.chunk->code[0].operand]), 2);

    VM vm(program.diagnostics);
    EXPECT_EQ(VM::valueToString(vm.run(program.chunk, {})), "2");
    EXPECT_FALSE(program.diagnostics.hasErrors());
}

TEST(OptimizerTest, ConstantArrayIsClonedPerEvaluation) {
    auto program = compileAndOptimize(
        "func make() { return [1, 2]; } "
        "a = make(); "
        "b = make(); "
        "a[0] = 9; "
        "b[0]");

    EXPECT_FALSE(program.diagnostics.hasErrors());

    VM vm(program.diagnostics);
    EXPECT_EQ(VM::valueToString(vm.run(program.chunk, {})), "1");
    EXPECT_FALSE(program.diagnostics.hasErrors());
}

TEST(OptimizerTest, NestedConstantArrayIsClonedDeeply) {
    auto program = compileAndOptimize(
        "func make() { return [[1]]; } "
        "a = make(); "
        "b = make(); "
        "a[0][0] = 9; "
        "b[0][0]");

    EXPECT_FALSE(program.diagnostics.hasErrors());

    VM vm(program.diagnostics);
    EXPECT_EQ(VM::valueToString(vm.run(program.chunk, {})), "1");
    EXPECT_FALSE(program.diagnostics.hasErrors());
}

TEST(OptimizerTest, FoldsTypeIntrospectionOpcodes) {
    ir::BytecodeChunk chunk;
    chunk.constants.push_back(42);
    chunk.constants.push_back(std::monostate{});

    chunk.code = {
        { OpCode::PUSH_INT, 0 },
        { OpCode::TYPE_OF, 0 },
        { OpCode::PUSH_NONE, 1 },
        { OpCode::HAS_VALUE, 0 },
        { OpCode::PUSH_INT, 0 },
        { OpCode::UNWRAP, 0 },
        { OpCode::HALT, 0 }
    };

    Optimizer optimizer;
    auto stats = optimizer.optimize(chunk);

    EXPECT_GE(stats.folded, 3u);

    ASSERT_EQ(chunk.code.size(), 4u);
    EXPECT_EQ(chunk.code[0].op, OpCode::PUSH_STR);
    EXPECT_EQ(chunk.code[1].op, OpCode::PUSH_BOOL);
    EXPECT_EQ(chunk.code[2].op, OpCode::PUSH_INT);
    EXPECT_EQ(chunk.code[3].op, OpCode::HALT);

    EXPECT_EQ(std::get<std::string>(chunk.constants[chunk.code[0].operand]), "int");
    EXPECT_EQ(std::get<bool>(chunk.constants[chunk.code[1].operand]), false);
    EXPECT_EQ(std::get<int>(chunk.constants[chunk.code[2].operand]), 42);
}

TEST(OptimizerTest, DoesNotFoldUnwrapOfEmptyOptional) {
    ir::BytecodeChunk chunk;
    chunk.constants.push_back(std::monostate{});

    chunk.code = {
        { OpCode::PUSH_NONE, 0 },
        { OpCode::UNWRAP, 0 },
        { OpCode::HALT, 0 }
    };

    Optimizer optimizer;
    auto stats = optimizer.optimize(chunk);

    EXPECT_EQ(stats.folded, 0u);
    ASSERT_EQ(chunk.code.size(), 3u);
    EXPECT_EQ(chunk.code[1].op, OpCode::UNWRAP);

    DiagnosticEngine diag;
    VM vm(diag);
    [[maybe_unused]] auto result = vm.run(std::make_shared<BytecodeChunk>(chunk), {});
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_NE(diag.getErrorMessage().find("Optional value is empty"), std::string::npos);
}
