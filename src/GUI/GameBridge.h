#pragma once
#include <mutex>
#include <condition_variable>
#include <optional>
#include <vector>
#include <string>
#include <atomic>
#include "../Cards/CardID.h"
#include "GameEvent.h"

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

// A snapshot paired with the events that preceded it. The GUI should play the events as animations, then update its
// display to this snapshot once the animations complete
struct SnapshotUpdate {
    std::vector<GameEvent> events;
    GameSnapshot snapshotAfter;
};

// This class's sole function is to bridge the gap between game logic and UI which run on separate threads
// so that the UI can render despite the game logic thread blocking.
class GameBridge {
public:
    // methods called from game thread
    [[nodiscard]] int publishStateAndWaitForInput(const GameSnapshot& snapshot);
    void publishState(const GameSnapshot& snapshot);
    void enqueueEvent(GameEvent event);

    // methods called from GUI thread
    [[nodiscard]] std::vector<SnapshotUpdate> drainUpdates();
    void submitInput(int choice);
    void requestQuit();

private:
    std::mutex mutex;
    std::condition_variable inputCV;
    std::vector<GameEvent> pendingEvents;
    std::vector<SnapshotUpdate> updateQueue;
    std::optional<int> pendingInput;
    std::atomic<bool> quit = false;
};
