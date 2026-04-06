#include "GameBridge.h"
#include <cassert>

/**
 * Publishes the game snapshot then blocks until receives input from GUI.
 * @param snapshot Game snapshot to publish
 * @return 0 = end turn, 1 or more = play card at index, -1 = quit
 */
int GameBridge::publishStateAndWaitForInput(const GameSnapshot& snapshot) {
    std::unique_lock lock(mutex);
    currentSnapshot = snapshot;
    pendingInput.reset();

    inputCV.wait(lock, [this] { return pendingInput.has_value() || quit.load(); });

    // if the player has quit, return -1 to tell game logic to concede and exit
    if (quit.load()) return -1;
    return pendingInput.value();
}

/**
 * Like publishStateAndWaitForInput, except doesn't wait for input from the GUI. Instead, this just updates the
 * GUI. Called all the time during opponent's turn during which we can't do anything but still need to update the GUI
 * @param snapshot Game snapshot to publish
 */
void GameBridge::publishState(const GameSnapshot& snapshot) {
    std::lock_guard lock(mutex);
    currentSnapshot = snapshot;
}

/**
 * Called by GUI. Gets the current snapshot of the game to render the GUI.
 * @return Current game snapshot
 */
GameSnapshot GameBridge::getSnapshot() {
    std::lock_guard lock(mutex);
    return currentSnapshot;
}

/**
 * Called by GUI. Submits the user's input from GUI. <b>When requesting to quit, use requestQuit over this method.</b>
 * @param choice Input from user (0 = end turn, 1 or more = play card at index
 */
void GameBridge::submitInput(int choice) {
    assert(choice != -1 && "GameBridge::submitInput called with -1. Quit requests must go through requestQuit");
    std::lock_guard lock(mutex);
    pendingInput = choice;
    inputCV.notify_one();
}

/**
 * Window has been closed, tell game logic thread that we're quitting.
 */
void GameBridge::requestQuit() {
    quit.store(true);
    inputCV.notify_one();
}
