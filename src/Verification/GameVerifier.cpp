#include "GameVerifier.h"

GameVerifier::GameVerifier(const std::vector<CardID>& localDeck, PlayerID localPlayer)
    : state(localDeck, localPlayer), localPlayer(localPlayer) {}

void GameVerifier::initialiseOpponentDeck(const std::vector<std::pair<Point, Scalar>>& encryptedDeck) {
    state.getOpponentPlayerData(localPlayer).deck.setEncryptedContents(encryptedDeck);
}

// TODO: Delete this? Vestige of old deck-shuffle-tracking system.
bool GameVerifier::fullyDecryptInitialOpponentDeck(const std::vector<Scalar>& remoteKeys) {
    return state.getOpponentPlayerData(localPlayer).deck.fullyDecrypt(remoteKeys);
}

void GameVerifier::setPlayerGoingFirst(PlayerID playerId) {
    state.activePlayer = playerId;
}

/**
 * Called any time a primitive game action occurs during the game (e.g., card drawn, damage taken, etc).
 * Used by the verifier to simulate the entire game by playing these game actions in sequence.
 * @param action Game action occurring
 */
void GameVerifier::addAction(ActionEntry action) {
    actions.emplace_back(action);
}

/**
 * Called any time the opponent makes a commitment mid-game. We don't log our own commitments because there is
 * no point in verifying our own commitments (we know them to be true).
 * @param commitment Pointer to heap-instantiated commitment object
 */
void GameVerifier::addEnemyCommitment(std::unique_ptr<Commitment> commitment) {
    enemyCommitments.push_back(std::move(commitment));
}

/**
 * Called any time a shuffle occurs during the game, regardless of whose deck is being shuffled.
 * @param deckOwner Player ID of the shuffled deck's owner
 * @param seed Local shuffle seed used
 */
void GameVerifier::logLocalShuffleSeed(PlayerID deckOwner, ShuffleSeed seed) {
    deckOwner == localPlayer ? ourDeckSeeds.local.push_back(seed)
        : opponentDeckSeeds.local.push_back(seed);
}

/**
 * Called at the end of the game after receiving all of the shuffle seeds used by the remote player.
 * <b>Must be called twice: once for each player.</b>
 * @param deckOwner Player ID of the shuffled deck's owner
 * @param remoteSeeds Vector of shuffle seeds used by the remote on the specified deck
 */
void GameVerifier::addAllRemoteShuffleSeeds(PlayerID deckOwner, const std::vector<ShuffleSeed>& remoteSeeds) {
    deckOwner == localPlayer ? ourDeckSeeds.remote = remoteSeeds
        : opponentDeckSeeds.remote = remoteSeeds;
}

/**
 * Called at the end of game after receiving all commitment keys from opponent. Makes all enemy commitments plaintext
 * in preparation for verification.
 * @param keys Vector of subvectors which contain keys. A key subvector contains all keys for one enemy commitment
 * @return True if successful; false if insufficient keys received
 */
bool GameVerifier::decryptEnemyCommitments(const std::vector<std::vector<Scalar>>& keys) {
    if (enemyCommitments.size() != keys.size()) return false; // insufficient keys

    for (int i = 0; i < enemyCommitments.size(); i++) {
        if (keys[i].size() == enemyCommitments[i]->getRemainingRequiredKeysCount())
            enemyCommitments[i]->populateRemainingKeys(keys[i]);
        else return false;
    }
    return true;
}

/**
 * Called at the end of game. Gets all keys used in commitments made by the local player. Sent to the opponent at the
 * end of the game so that they may verify all of our commitments.
 * @return Vector of subvectors. Each subvector contains every key for the commitment in sequence
 */
const std::vector<std::vector<Scalar>>& GameVerifier::getLocalCommitmentKeys() const {
    return localCommitmentKeys;
}

/**
 * Called at the end of the game when sending opponent data to run the verifier.
 * @return Vector of shuffle seeds used by the local player to shuffle their deck
 */
std::vector<ShuffleSeed> GameVerifier::getLocalShuffleSeedsForOurDeck() const {
    return ourDeckSeeds.local;
}

/**
 * Called at the end of the game when sending opponent data to run the verifier.
 * @return Vector of shuffle seeds used by the local player to shuffle their opponent's deck
 */
std::vector<ShuffleSeed> GameVerifier::getLocalShuffleSeedsForOpponentsDeck() const {
    return opponentDeckSeeds.local;
}

bool GameVerifier::run() {


    return true;
}
