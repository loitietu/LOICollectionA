#pragma once

#include <string>

class Player;

namespace CommandUtils {
    void executeCommand(Player& player, const std::string& command);
}