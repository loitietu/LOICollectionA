#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <unistd.h>

#include "LOICollectionA/frontend/ir/BytecodeCache.h"
#include "LOICollectionA/frontend/ir/OpCode.h"

using namespace LOICollection::frontend::ir;

namespace {
    std::string tempPath(const std::string& tag) {
        return "/tmp/lcui_cache_" + tag + "_" + std::to_string(::getpid()) + ".bin";
    }

    BytecodeChunk sampleChunk() {
        BytecodeChunk chunk;
        chunk.emit(OpCode::PUSH_INT, 42);
        chunk.emit(OpCode::HALT, 0);
        chunk.constants.emplace_back(std::string("cache-key"));
        return chunk;
    }
}

TEST(BytecodeCacheTest, Sha256IsDeterministic) {
    const std::string a = bytecode_cache::sha256("hello");
    const std::string b = bytecode_cache::sha256("hello");
    const std::string c = bytecode_cache::sha256("hello!");

    EXPECT_EQ(a, b);
    EXPECT_EQ(a.size(), 32u);
    EXPECT_NE(a, c);
}

TEST(BytecodeCacheTest, WriteAndReadRoundTrip) {
    const std::string path = tempPath("roundtrip");
    const std::string sourceHash = bytecode_cache::sha256("source");
    const std::vector<bytecode_cache::ImportHash> imports = {
        { "generic.lcui", bytecode_cache::sha256("generic") }
    };

    ASSERT_TRUE(bytecode_cache::write(path, sampleChunk(), sourceHash, imports));

    auto loaded = bytecode_cache::read(path, sourceHash, imports);
    ASSERT_NE(loaded, nullptr);
    ASSERT_EQ(loaded->code.size(), 2u);
    EXPECT_EQ(loaded->code[0].op, OpCode::PUSH_INT);
    EXPECT_EQ(loaded->code[0].operand, 42);
    EXPECT_EQ(loaded->code[1].op, OpCode::HALT);
    ASSERT_EQ(loaded->constants.size(), 1u);
    EXPECT_EQ(std::get<std::string>(loaded->constants[0]), "cache-key");

    auto importPaths = bytecode_cache::readImportPaths(path);
    ASSERT_TRUE(importPaths.has_value());
    ASSERT_EQ(importPaths->size(), 1u);
    EXPECT_EQ((*importPaths)[0], "generic.lcui");

    std::remove(path.c_str());
}

TEST(BytecodeCacheTest, StaleSourceHashInvalidates) {
    const std::string path = tempPath("stale_source");
    ASSERT_TRUE(bytecode_cache::write(path, sampleChunk(), bytecode_cache::sha256("v1"), {}));

    EXPECT_EQ(bytecode_cache::read(path, bytecode_cache::sha256("v2"), {}), nullptr);

    std::remove(path.c_str());
}

TEST(BytecodeCacheTest, StaleImportHashInvalidates) {
    const std::string path = tempPath("stale_import");
    const std::string sourceHash = bytecode_cache::sha256("source");
    const std::vector<bytecode_cache::ImportHash> oldImports = {
        { "generic.lcui", bytecode_cache::sha256("old") }
    };
    ASSERT_TRUE(bytecode_cache::write(path, sampleChunk(), sourceHash, oldImports));

    const std::vector<bytecode_cache::ImportHash> newImports = {
        { "generic.lcui", bytecode_cache::sha256("new") }
    };
    EXPECT_EQ(bytecode_cache::read(path, sourceHash, newImports), nullptr);

    std::remove(path.c_str());
}

TEST(BytecodeCacheTest, MissingFileIsMiss) {
    EXPECT_EQ(bytecode_cache::read("/tmp/lcui_cache_missing.bin", "x", {}), nullptr);
}
