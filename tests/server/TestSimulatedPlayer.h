#pragma once

#include <memory>
#include <string>

class SimulatedPlayer;

class TestSimulatedPlayer {
public:
    TestSimulatedPlayer(const std::string& playerName);
    ~TestSimulatedPlayer();

    bool create();
    bool destroy();

    SimulatedPlayer* getPlayer();

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
