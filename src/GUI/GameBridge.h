#pragma once
#include <mutex>
#include <condition_variable>
#include <optional>
#include <vector>
#include <string>
#include <atomic>
#include "../Cards/CardID.h"

struct GameSnapshot {
    // local player
    int myHealth = 10;
    int myMana = 5;
    int myDeckSize = 0;
    std::vector<CardID> myHand;

    // opponent
    int oppHealth = 10;
    int oppMana = 5;
    int oppDeckSize = 0;
    int oppHandSize = 0;

    // non-player-related
    bool isMyTurn = false;
    bool gameOver = false;
    std::string statusMessage;
    std::optional<std::string> winnerMessage;
};

// This class's sole function is to bridge the gap between game logic and UI which run on separate threads
// so that the UI can render despite the game logic thread blocking.
class GameBridge {
public:
    // methods called from game thread
    [[nodiscard]] int publishStateAndWaitForInput(const GameSnapshot& snapshot);
    void publishState(const GameSnapshot& snapshot);

    // methods called from GUI thread
    [[nodiscard]] GameSnapshot getSnapshot();
    void submitInput(int choice);
    void requestQuit();

private:
    std::mutex mutex;
    std::condition_variable inputCV;
    GameSnapshot currentSnapshot;
    std::optional<int> pendingInput;
    std::atomic<bool> quit = false;
};
