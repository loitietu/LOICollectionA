#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>

#include <mc/world/scores/Objective.h>
#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>
#include <mc/world/scores/ScoreboardOperationResult.h>
#include <mc/world/scores/PlayerScoreSetFunction.h>

#include "LOICollectionA/utils/ScoreboardUtils.h"

namespace ScoreboardUtils {
    bool hasScoreboard(const std::string& name) {
        optional_ref<Level> level = ll::service::getLevel();
        return level->getScoreboard().getObjective(name) ? true : false;
    }

    int getScore(Player& player, const std::string& name) {
        optional_ref<Level> level = ll::service::getLevel();
        ScoreboardId identity = level->getScoreboard().getScoreboardId(player);
        if (identity.mRawID == ScoreboardId::INVALID().mRawID)
            return 0;
        
        Objective* obj = level->getScoreboard().getObjective(name);
        return !obj ? 0 : obj->getPlayerScore(identity).mValue;
    }

    void modifyScore(ScoreboardId& identity, const std::string& name, int score, ScoreType action) {
        optional_ref<Level> level = ll::service::getLevel();
        Objective* obj = level->getScoreboard().getObjective(name);
            
        if (!obj) {
            obj = static_cast<Objective*>(level->getScoreboard().addObjective(
                name, name, *level->getScoreboard().getCriteria("dummy")
            ));
        }

        ScoreboardOperationResult result;
        level->getScoreboard().modifyPlayerScore(result, identity, *obj, score, 
            action == ScoreType::set ? PlayerScoreSetFunction::Set : 
            action == ScoreType::add ? PlayerScoreSetFunction::Add : 
            PlayerScoreSetFunction::Subtract
        );
    }

    void addScore(Player& player, const std::string &name, int score) {
        ScoreboardId identity = ll::service::getLevel()->getScoreboard().getScoreboardId(player);
        if (identity.mRawID == ScoreboardId::INVALID().mRawID) 
            identity = ll::service::getLevel()->getScoreboard().createScoreboardId(player);
            
        modifyScore(identity, name, score, ScoreType::add);
    }

    void reduceScore(Player& player, const std::string &name, int score) {
        ScoreboardId identity = ll::service::getLevel()->getScoreboard().getScoreboardId(player);
        if (identity.mRawID == ScoreboardId::INVALID().mRawID)
            identity = ll::service::getLevel()->getScoreboard().createScoreboardId(player);

        modifyScore(identity, name, score, ScoreType::reduce);
    }
}