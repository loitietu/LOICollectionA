#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility>

#include <ll/api/Expected.h>

#include "LOICollectionA/frontend/ir/BytecodeCache.h"

namespace LOICollection::frontend::ir::bytecode_cache {
    namespace {
        constexpr std::string_view kMagic = "LCUI";
        constexpr uint32_t         kFormatVersion = 1;

        /* ------------------------------------------------------------------ */
        /* SHA-256 (FIPS 180-4)                                               */
        /* ------------------------------------------------------------------ */

        constexpr std::array<uint32_t, 64> kSha256K = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        uint32_t rotateRight(uint32_t value, int bits) {
            return (value >> bits) | (value << (32 - bits));
        }

        struct Sha256 {
            std::array<uint32_t, 8> state = {
                0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
            };
            std::array<uint8_t, 64> buffer{};
            uint64_t bitCount = 0;

            void transform(const uint8_t block[64]) {
                std::array<uint32_t, 64> words{};
                for (size_t i = 0; i < 16; ++i)
                    words[i] = static_cast<uint32_t>(block[i * 4]) << 24 |
                               static_cast<uint32_t>(block[i * 4 + 1]) << 16 |
                               static_cast<uint32_t>(block[i * 4 + 2]) << 8 |
                               static_cast<uint32_t>(block[i * 4 + 3]);

                for (size_t i = 16; i < 64; ++i) {
                    uint32_t s0 = rotateRight(words[i - 15], 7) ^ rotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3);
                    uint32_t s1 = rotateRight(words[i - 2], 17) ^ rotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10);
                    words[i] = words[i - 16] + s0 + words[i - 7] + s1;
                }

                auto [a, b, c, d, e, f, g, h] = state;

                for (size_t i = 0; i < 64; ++i) {
                    uint32_t s1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^ rotateRight(e, 25);
                    uint32_t ch = (e & f) ^ (~e & g);
                    uint32_t temp1 = h + s1 + ch + kSha256K[i] + words[i];
                    uint32_t s0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^ rotateRight(a, 22);
                    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                    uint32_t temp2 = s0 + maj;

                    h = g;
                    g = f;
                    f = e;
                    e = d + temp1;
                    d = c;
                    c = b;
                    b = a;
                    a = temp1 + temp2;
                }

                state = {
                    state[0] + a, state[1] + b, state[2] + c, state[3] + d,
                    state[4] + e, state[5] + f, state[6] + g, state[7] + h
                };
            }

            void update(const std::string& data) {
                size_t offset = 0;
                size_t remaining = data.size();

                while (remaining > 0) {
                    size_t fill = (bitCount / 8) % 64;
                    size_t chunk = std::min<size_t>(64 - fill, remaining);
                    std::memcpy(buffer.data() + fill, data.data() + offset, chunk);
                    bitCount += static_cast<uint64_t>(chunk) * 8;
                    offset += chunk;
                    remaining -= chunk;

                    if ((bitCount / 8) % 64 == 0)
                        transform(buffer.data());
                }
            }

            std::string digest() {
                size_t fill = (bitCount / 8) % 64;
                std::array<uint8_t, 64> finalBlock{};

                std::memcpy(finalBlock.data(), buffer.data(), fill);
                finalBlock[fill] = 0x80;

                if (fill >= 56) {
                    transform(finalBlock.data());
                    finalBlock.fill(0);
                }

                uint64_t bits = bitCount;
                for (int i = 0; i < 8; ++i)
                    finalBlock[63 - i] = static_cast<uint8_t>(bits >> (i * 8));

                transform(finalBlock.data());

                std::string result;
                result.reserve(64);
                for (uint32_t word : state) {
                    for (int shift = 24; shift >= 0; shift -= 8)
                        result.push_back(static_cast<char>((word >> shift) & 0xff));
                }
                return result;
            }
        };
    }

    std::string sha256(const std::string& data) {
        Sha256 ctx;
        ctx.update(data);
        return ctx.digest();
    }

    /* ---------------------------------------------------------------------- */
    /* Binary serialization                                                   */
    /* ---------------------------------------------------------------------- */

    namespace {
        class ByteWriter {
            std::string buffer;

        public:
            void raw(const void* data, size_t size) {
                buffer.append(static_cast<const char*>(data), size);
            }

            void u8(uint8_t value) { this->raw(&value, sizeof(value)); }
            void u32(uint32_t value) { this->raw(&value, sizeof(value)); }
            void i32(int32_t value) { this->raw(&value, sizeof(value)); }
            void u64(uint64_t value) { this->raw(&value, sizeof(value)); }
            void f32(float value) { this->raw(&value, sizeof(value)); }

            void str(const std::string& value) {
                this->u32(static_cast<uint32_t>(value.size()));
                this->raw(value.data(), value.size());
            }

            template <typename T, typename Serialize>
            void vec(const std::vector<T>& values, Serialize&& serialize) {
                this->u32(static_cast<uint32_t>(values.size()));
                for (const auto& value : values)
                    serialize(value);
            }

            [[nodiscard]] const std::string& data() const { return this->buffer; }
        };

        class ByteReader {
            const std::string& buffer;
            size_t pos = 0;

        public:
            explicit ByteReader(const std::string& data) : buffer(data) {}

            [[nodiscard]] size_t remaining() const { return this->buffer.size() - this->pos; }

            template <typename T>
            [[nodiscard]] bool read(T& value) {
                if (this->remaining() < sizeof(T))
                    return false;

                std::memcpy(&value, this->buffer.data() + this->pos, sizeof(T));
                this->pos += sizeof(T);
                return true;
            }

            [[nodiscard]] bool bytes(std::string& value, size_t size) {
                if (this->remaining() < size)
                    return false;

                value.assign(this->buffer.data() + this->pos, size);
                this->pos += size;
                return true;
            }

            [[nodiscard]] bool str(std::string& value) {
                uint32_t size = 0;
                if (!this->read(size))
                    return false;

                return this->bytes(value, size);
            }

            template <typename T, typename Deserialize>
            [[nodiscard]] bool vec(std::vector<T>& values, Deserialize&& deserialize) {
                uint32_t size = 0;
                if (!this->read(size))
                    return false;

                values.reserve(size);
                for (uint32_t i = 0; i < size; ++i) {
                    T value;
                    if (!deserialize(value))
                        return false;
                    values.push_back(std::move(value));
                }
                return true;
            }
        };

        void writeValue(ByteWriter& writer, const ValueNode::ValueType& value) {
            std::visit([&](const auto& arg) {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, int>) {
                    writer.u8(0);
                    writer.i32(static_cast<int32_t>(arg));
                } else if constexpr (std::is_same_v<T, float>) {
                    writer.u8(1);
                    writer.f32(arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    writer.u8(2);
                    writer.str(arg);
                } else if constexpr (std::is_same_v<T, bool>) {
                    writer.u8(3);
                    writer.u8(arg ? 1 : 0);
                } else {
                    writer.u8(4); // None
                }
            }, value);
        }

        bool readValue(ByteReader& reader, ValueNode::ValueType& value) {
            uint8_t tag = 0;
            if (!reader.read(tag))
                return false;

            switch (tag) {
                case 0: {
                    int32_t v = 0;
                    if (!reader.read(v))
                        return false;
                    value = v;
                    return true;
                }
                case 1: {
                    float v = 0;
                    if (!reader.read(v))
                        return false;
                    value = v;
                    return true;
                }
                case 2: {
                    std::string v;
                    if (!reader.str(v))
                        return false;
                    value = std::move(v);
                    return true;
                }
                case 3: {
                    uint8_t v = 0;
                    if (!reader.read(v))
                        return false;
                    value = v != 0;
                    return true;
                }
                case 4:
                    value = std::monostate{};
                    return true;
                default:
                    return false;
            }
        }

        void writeLocation(ByteWriter& writer, const SourceLocation& loc) {
            writer.u64(loc.line);
            writer.u64(loc.column);
            writer.u64(loc.offset);
        }

        bool readLocation(ByteReader& reader, SourceLocation& loc) {
            uint64_t line = 0, column = 0, offset = 0;
            if (!reader.read(line) || !reader.read(column) || !reader.read(offset))
                return false;

            loc = { static_cast<size_t>(line), static_cast<size_t>(column), static_cast<size_t>(offset) };
            return true;
        }

        void writeInstruction(ByteWriter& writer, const Instruction& instruction) {
            writer.u8(static_cast<uint8_t>(instruction.op));
            writer.i32(instruction.operand);
            writeLocation(writer, instruction.loc);
        }

        bool readInstruction(ByteReader& reader, Instruction& instruction) {
            uint8_t op = 0;
            if (!reader.read(op))
                return false;

            instruction.op = static_cast<OpCode>(op);
            return reader.read(instruction.operand) && readLocation(reader, instruction.loc);
        }

        void writeChunk(ByteWriter& writer, const BytecodeChunk& chunk) {
            writer.vec(chunk.code, [&](const Instruction& instruction) {
                writeInstruction(writer, instruction);
            });
            writer.vec(chunk.constants, [&](const ValueNode::ValueType& value) {
                writeValue(writer, value);
            });
            writer.vec(chunk.functions, [&](const FuncMeta& meta) {
                writer.str(meta.name);
                writer.i32(meta.argCount);
            });
            writer.vec(chunk.macros, [&](const MacroMeta& meta) {
                writer.str(meta.name);
                writer.i32(meta.argCount);
            });
            writer.vec(chunk.classes, [&](const ClassMeta& meta) {
                writer.str(meta.name);
                writer.i32(meta.baseClassIndex);
                writer.vec(meta.fieldNames, [&](const std::string& name) { writer.str(name); });
                writer.vec(meta.defaults, [&](const ValueNode::ValueType& value) { writeValue(writer, value); });
                writer.vec(meta.hasDefault, [&](bool value) { writer.u8(value ? 1 : 0); });
                writer.vec(meta.staticFieldNames, [&](const std::string& name) { writer.str(name); });
                writer.vec(meta.staticDefaults, [&](const ValueNode::ValueType& value) { writeValue(writer, value); });
                writer.vec(meta.staticHasDefault, [&](bool value) { writer.u8(value ? 1 : 0); });
                writer.i32(meta.constructorIndex);
                writer.vec(meta.methods, [&](int index) { writer.i32(index); });
                writer.vec(meta.methodSignatures, [&](const std::string& signature) { writer.str(signature); });
                writer.vec(meta.staticMethods, [&](int index) { writer.i32(index); });
                writer.vec(meta.staticMethodSignatures, [&](const std::string& signature) { writer.str(signature); });
                writer.vec(meta.ancestorIndices, [&](int index) { writer.i32(index); });
            });
            writer.vec(chunk.methods, [&](const MethodMeta& meta) {
                writer.str(meta.name);
                writer.vec(meta.paramNames, [&](const std::string& name) { writer.str(name); });
                writer.i32(meta.argCount);
                writer.i32(meta.classIndex);
                writer.i32(meta.bodyIndex);
            });
            writer.vec(chunk.nativeCalls, [&](const NativeCallMeta& meta) {
                writer.str(meta.className);
                writer.str(meta.name);
                writer.i32(meta.argCount);
                writer.u8(meta.isStatic ? 1 : 0);
            });
            writer.vec(chunk.virtualCalls, [&](const VirtualCallMeta& meta) {
                writer.i32(meta.classIndex);
                writer.i32(meta.ordinal);
                writer.i32(meta.argCount);
            });
            writer.vec(chunk.superCalls, [&](const SuperCallMeta& meta) {
                writer.i32(meta.constructorIndex);
                writer.i32(meta.argCount);
            });
            writer.vec(chunk.lambdas, [&](const LambdaMeta& meta) {
                writer.i32(meta.bodyIndex);
                writer.i32(meta.argCount);
                writer.vec(meta.paramNames, [&](const std::string& name) { writer.str(name); });
            });
            writer.vec(chunk.methodBodies, [&](const BytecodeChunk& body) {
                writeChunk(writer, body);
            });
        }

        bool readChunk(ByteReader& reader, BytecodeChunk& chunk) {
            if (!reader.vec(chunk.code, [&](Instruction& instruction) {
                    return readInstruction(reader, instruction);
                }))
                return false;
            if (!reader.vec(chunk.constants, [&](ValueNode::ValueType& value) {
                    return readValue(reader, value);
                }))
                return false;
            if (!reader.vec(chunk.functions, [&](FuncMeta& meta) {
                    return reader.str(meta.name) && reader.read(meta.argCount);
                }))
                return false;
            if (!reader.vec(chunk.macros, [&](MacroMeta& meta) {
                    return reader.str(meta.name) && reader.read(meta.argCount);
                }))
                return false;
            if (!reader.vec(chunk.classes, [&](ClassMeta& meta) {
                    if (!reader.str(meta.name) || !reader.read(meta.baseClassIndex))
                        return false;
                    if (!reader.vec(meta.fieldNames, [&](std::string& name) { return reader.str(name); }))
                        return false;
                    if (!reader.vec(meta.defaults, [&](ValueNode::ValueType& value) { return readValue(reader, value); }))
                        return false;
                    if (!reader.vec(meta.hasDefault, [&](bool& value) {
                            uint8_t byte = 0;
                            if (!reader.read(byte))
                                return false;
                            value = byte != 0;
                            return true;
                        }))
                        return false;
                    if (!reader.vec(meta.staticFieldNames, [&](std::string& name) { return reader.str(name); }))
                        return false;
                    if (!reader.vec(meta.staticDefaults, [&](ValueNode::ValueType& value) { return readValue(reader, value); }))
                        return false;
                    if (!reader.vec(meta.staticHasDefault, [&](bool& value) {
                            uint8_t byte = 0;
                            if (!reader.read(byte))
                                return false;
                            value = byte != 0;
                            return true;
                        }))
                        return false;
                    if (!reader.read(meta.constructorIndex))
                        return false;
                    if (!reader.vec(meta.methods, [&](int& index) { return reader.read(index); }))
                        return false;
                    if (!reader.vec(meta.methodSignatures, [&](std::string& signature) { return reader.str(signature); }))
                        return false;
                    if (!reader.vec(meta.staticMethods, [&](int& index) { return reader.read(index); }))
                        return false;
                    if (!reader.vec(meta.staticMethodSignatures, [&](std::string& signature) { return reader.str(signature); }))
                        return false;
                    return reader.vec(meta.ancestorIndices, [&](int& index) { return reader.read(index); });
                }))
                return false;
            if (!reader.vec(chunk.methods, [&](MethodMeta& meta) {
                    if (!reader.str(meta.name))
                        return false;
                    if (!reader.vec(meta.paramNames, [&](std::string& name) { return reader.str(name); }))
                        return false;
                    return reader.read(meta.argCount) && reader.read(meta.classIndex) && reader.read(meta.bodyIndex);
                }))
                return false;
            if (!reader.vec(chunk.nativeCalls, [&](NativeCallMeta& meta) {
                    if (!reader.str(meta.className) || !reader.str(meta.name) || !reader.read(meta.argCount))
                        return false;
                    uint8_t isStatic = 0;
                    if (!reader.read(isStatic))
                        return false;
                    meta.isStatic = isStatic != 0;
                    return true;
                }))
                return false;
            if (!reader.vec(chunk.virtualCalls, [&](VirtualCallMeta& meta) {
                    return reader.read(meta.classIndex) && reader.read(meta.ordinal) && reader.read(meta.argCount);
                }))
                return false;
            if (!reader.vec(chunk.superCalls, [&](SuperCallMeta& meta) {
                    return reader.read(meta.constructorIndex) && reader.read(meta.argCount);
                }))
                return false;
            if (!reader.vec(chunk.lambdas, [&](LambdaMeta& meta) {
                    if (!reader.read(meta.bodyIndex) || !reader.read(meta.argCount))
                        return false;
                    return reader.vec(meta.paramNames, [&](std::string& name) { return reader.str(name); });
                }))
                return false;
            return reader.vec(chunk.methodBodies, [&](BytecodeChunk& body) {
                return readChunk(reader, body);
            });
        }
    }

    bool write(
        const std::string& path,
        const BytecodeChunk& chunk,
        const std::string& sourceHash,
        const std::vector<ImportHash>& imports
    ) {
        ByteWriter writer;

        writer.raw(kMagic.data(), kMagic.size());
        writer.u32(kFormatVersion);
        writer.str(sourceHash);
        writer.u32(static_cast<uint32_t>(imports.size()));
        for (const auto& import : imports) {
            writer.str(import.path);
            writer.str(import.hash);
        }

        writeChunk(writer, chunk);

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
        if (ec)
            return false;

        std::ofstream file(path, std::ios::binary);
        if (!file)
            return false;

        file.write(writer.data().data(), static_cast<std::streamsize>(writer.data().size()));
        return static_cast<bool>(file);
    }

    std::optional<std::vector<std::string>> readImportPaths(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return std::nullopt;

        std::string content(
            (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()
        );

        ByteReader reader(content);

        std::string magic;
        if (!reader.bytes(magic, kMagic.size()) || magic != kMagic)
            return std::nullopt;

        uint32_t version = 0;
        if (!reader.read(version) || version != kFormatVersion)
            return std::nullopt;

        std::string storedSource;
        if (!reader.str(storedSource))
            return std::nullopt;

        uint32_t count = 0;
        if (!reader.read(count))
            return std::nullopt;

        std::vector<std::string> paths;
        paths.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            std::string importPath, hash;
            if (!reader.str(importPath) || !reader.str(hash))
                return std::nullopt;

            paths.push_back(std::move(importPath));
        }

        return paths;
    }

    std::shared_ptr<BytecodeChunk> read(
        const std::string& path,
        const std::string& sourceHash,
        const std::vector<ImportHash>& imports
    ) {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return nullptr;

        std::string content(
            (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()
        );
        if (content.empty())
            return nullptr;

        ByteReader reader(content);

        std::string magic;
        if (!reader.bytes(magic, kMagic.size()) || magic != kMagic)
            return nullptr;

        uint32_t version = 0;
        if (!reader.read(version) || version != kFormatVersion)
            return nullptr;

        std::string storedSource;
        if (!reader.str(storedSource) || storedSource != sourceHash)
            return nullptr;

        uint32_t importCount = 0;
        if (!reader.read(importCount) || importCount != imports.size())
            return nullptr;

        for (const auto& expected : imports) {
            std::string path, hash;
            if (!reader.str(path) || !reader.str(hash) || path != expected.path || hash != expected.hash)
                return nullptr;
        }

        auto chunk = std::make_shared<BytecodeChunk>();
        if (!readChunk(reader, *chunk))
            return nullptr;

        return chunk;
    }
}
