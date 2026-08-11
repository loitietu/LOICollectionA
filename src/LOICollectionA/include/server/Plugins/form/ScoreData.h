#pragma once

#include <string>
#include <vector>

#include "LOICollectionA/frontend/AST.h"

namespace LOICollection::server::Plugins {
    struct ScoreRequirement {
        std::string objective;
        int value = 0;
    };

    LOICOLLECTION_A_NDAPI std::vector<ScoreRequirement> readScores(
        const frontend::ObjectRef& obj,
        const std::string& field = "scores"
    );
    LOICOLLECTION_A_NDAPI frontend::ArrayRef makeScoreArray(const std::vector<ScoreRequirement>& scores);
}
