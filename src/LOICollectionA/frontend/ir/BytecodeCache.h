#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/frontend/ir/ByteCode.h"

namespace LOICollection::frontend::ir::bytecode_cache {
    /* One entry of the import graph recorded in the cache header. */
    struct ImportHash {
        std::string path;
        std::string hash;
    };

    LOICOLLECTION_A_NDAPI std::string sha256(const std::string& data);

    /* Reads only the import path list from a cache file header so callers can
     * recompute import hashes before validating (and deserializing) the entry.
     * Returns std::nullopt when the file is missing or malformed. */
    LOICOLLECTION_A_NDAPI std::optional<std::vector<std::string>> readImportPaths(const std::string& path);

    /* Writes a cache file:
     *   magic "LCUI" | format version | source hash | import hash list | chunk
     * Any change to a source or import file invalidates the entry. */
    LOICOLLECTION_A_API   bool write(
        const std::string& path,
        const BytecodeChunk& chunk,
        const std::string& sourceHash,
        const std::vector<ImportHash>& imports
    );

    /* Returns nullptr when the entry is missing, stale, incompatible or corrupt;
     * callers fall back to a full compile. */
    LOICOLLECTION_A_NDAPI std::shared_ptr<BytecodeChunk> read(
        const std::string& path,
        const std::string& sourceHash,
        const std::vector<ImportHash>& imports
    );
}
