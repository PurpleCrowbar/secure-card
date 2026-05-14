#include "GameBridge.h"
#include <cassert>

/**
 * Publishes the game snapshot then blocks until receives input from GUI.
 * Any events enqueued since the last publish are bundled with this snapshot.
 * @param snapshot Game snapshot to publish
 * @return 0 = end turn, 1 or more = play card at index, -1 = quit
 */
int GameBridge::publishStateAndWaitForInput(const GameSnapshot& snapshot) {
    std::unique_lock lock(mutex);
    updateQueue.push_back({std::move(pendingEvents), snapshot});
    pendingEvents.clear();
    pendingInput.reset();

    inputCV.wait(lock, [this] { return pendingInput.has_value() || quit.load(); });

    // if the player has quit, return -1 to tell game logic to concede and exit
    if (quit.load()) return -1;
    return pendingInput.value();
}

/**
 * Like publishStateAndWaitForInput, except doesn't wait for input from the GUI. Instead, this just updates the
 * GUI. Called all the time during opponent's turn during which we can't do anything but still need to update the GUI.
 * Any events enqueued since the last publish are bundled with this snapshot.
 * @param snapshot Game snapshot to publish
 */
void GameBridge::publishState(const GameSnapshot& snapshot) {
    std::lock_guard lock(mutex);
    updateQueue.push_back({std::move(pendingEvents), snapshot});
    pendingEvents.clear();
}

/**
 * Called by the game thread to enqueue a visual event (card played, damage dealt, etc).
 * Events accumulate until the next publishState/publishStateAndWaitForInput bundles them with a snapshot.
 */
void GameBridge::enqueueEvent(GameEvent event) {
    std::lock_guard lock(mutex);
    pendingEvents.push_back(std::move(event));
}

/**
 * Called by GUI. Drains all pending snapshot updates from the bridge.
 * Each update contains the events that occurred and the snapshot state after those events.
 * @return Vector of snapshot updates to process in order
 */
std::vector<SnapshotUpdate> GameBridge::drainUpdates() {
    std::lock_guard lock(mutex);
    std::vector<SnapshotUpdate> result = std::move(updateQueue);
    updateQueue.clear();
    return result;
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
