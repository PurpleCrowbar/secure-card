#include "GameVerifier.h"

GameVerifier::GameVerifier(const std::map<CardID, uint8_t>& localDeckContents, PlayerID localPlayer)
    : state(localDeckContents, localPlayer), localPlayer(localPlayer) {
    state.getOpponentPlayerData(localPlayer).hand = std::variant<ClearHand, UnknownHand>(ClearHand());
}

void GameVerifier::initialiseOpponentDeck(const std::map<CardID, uint8_t> &plaintextDeckContents) {
    state.getOpponentPlayerData(localPlayer).deck.v_setPlaintextContents(plaintextDeckContents);
}

void GameVerifier::setPlayerGoingFirst(PlayerID playerId) {
    state.activePlayer = playerId;
}

/**
 * Called any time a primitive game action occurs during the game (e.g., card drawn, damage taken, etc).
 * Used by the verifier to simulate the entire game by playing these game actions in sequence.
 * @param action Game action occurring
 */
void GameVerifier::logAction(ActionEntry action) {
    gameActionLog.emplace_back(action);
}

/**
 * Called any time the opponent makes a commitment mid-game. <b>This automatically logs the "VerifyCommitment" game action</b>.
 * We don't log our own commitments because there is no point in verifying our own commitments (we know them to be true).
 * @param commitment Pointer to heap-instantiated commitment object
 */
void GameVerifier::logEnemyCommitment(std::unique_ptr<Commitment> commitment) {
    enemyCommitments.push_back(std::move(commitment));
    gameActionLog.push_back(Action::VerifyCommitment(enemyCommitments.size() - 1));
}

void GameVerifier::setLocalDeckCommitmentKey(const DeckHashKey& key) {
    localDeckCommitmentKey = key;
}

void GameVerifier::setRemoteDeckCommitment(const DeckHash& hash) {
    remoteDeckCommitment = hash;
}

const DeckHashKey& GameVerifier::getLocalDeckCommitmentKey() const {
    return localDeckCommitmentKey;
}

/**
 * Verifies that the opponent's revealed deck contents and key match the hash they committed to at game start.
 * @param remoteKey Key revealed by opponent at game end
 * @param remoteDeckContents Deck contents revealed by opponent at game end
 * @return True if the commitment is valid (opponent didn't modify their deck), else false
 */
bool GameVerifier::verifyRemoteDeckContents(const DeckHashKey& remoteKey, const std::map<CardID, uint8_t>& remoteDeckContents) const {
    return verifyDeckCommitment(remoteDeckCommitment, remoteKey, remoteDeckContents);
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

/**
 * A simulation-only helper method that shuffles the deck of the local player.
 * @param shuffleSeedIndex Index of current shuffle seed. Equal to the number of shuffles done to this deck minus one
 */
void GameVerifier::v_shuffleLocalDeck(size_t shuffleSeedIndex) {
    state.getPlayerData(localPlayer).deck.v_shuffleWithSeeds(
        ourDeckSeeds.local[shuffleSeedIndex], ourDeckSeeds.remote[shuffleSeedIndex]
    );
}

/**
 * A simulation-only helper method that shuffles the opponent's deck.
 * @param shuffleSeedIndex Index of current shuffle seed. Equal to the number of shuffles done to this deck minus one
 */
void GameVerifier::v_shuffleOpponentsDeck(size_t shuffleSeedIndex) {
    state.getOpponentPlayerData(localPlayer).deck.v_shuffleWithSeeds(
        opponentDeckSeeds.remote[shuffleSeedIndex], opponentDeckSeeds.local[shuffleSeedIndex]
    );
}

// the code for this helper type was taken from https://en.cppreference.com/w/cpp/utility/variant/visit2.html
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

/**
 * Resimulates the entire game using the logged sequence of every game action and commitment. Uses shuffle seeds
 * distributed at the end of the game to deterministically shuffle decks.
 * @return False if a discrepancy was detected
 */
bool GameVerifier::run() {
    size_t currentTurn = 0;
    size_t currentLocalDeckShuffleSeed = 0;
    size_t currentOpponentDeckShuffleSeed = 0;

    for (size_t i = 0; i < gameActionLog.size(); ++i) {
        bool ok = std::visit(overloaded {

            [&](const Action::DealDamage& a) -> bool {
                auto& pd = state.getPlayerData(a.target);
                pd.currentHealth -= a.amount;
                return true;
            },

            [&](const Action::GainLife& a) -> bool {
                auto& pd = state.getPlayerData(a.target);
                pd.currentHealth += a.amount;
                return true;
            },

            [&](const Action::DrawCards& a) -> bool {
                auto& pd = state.getPlayerData(a.player);
                for (uint8_t j = 0; j < a.count; ++j) {
                    auto& hand = std::get<ClearHand>(pd.hand);
                    if (hand.isFull()) return false; // return false means discrepancy detected. so don't log draw action on full hand
                    auto card = pd.deck.draw();
                    // if deck empty, apply fatigue damage. else, add card to hand
                    if (!card.has_value()) pd.currentHealth -= ++pd.fatigueCount;
                    else hand.addCard(card.value());
                }
                return true;
            },

            [&](const Action::PlayCard& a) -> bool {
                auto& pd = state.getPlayerData(a.player);
                if (!std::get<ClearHand>(pd.hand).removeCard(a.card)) return false;
                pd.graveyard.push_back(a.card);
                return true;
            },

            [&](const Action::Discard& a) -> bool {
                // auto& pd = state.getPlayerData(a.player);
                // auto it = std::ranges::find(pd.handContents, a.card);
                // if (it == pd.handContents.end()) return false;
                // pd.handContents.erase(it);
                // pd.graveyard.push_back(a.card);
                return true;
            },

            [&](const Action::EndTurn& a) -> bool {
                state.activePlayer = PlayerIDUtils::getOpponent(a.player);
                currentTurn++;
                return true;
            },

            [&](const Action::Shuffle& a) -> bool {
                if (a.deckOwner == localPlayer) v_shuffleLocalDeck(currentLocalDeckShuffleSeed++);
                else v_shuffleOpponentsDeck(currentOpponentDeckShuffleSeed++);
                return true;
            },

            [&](const Action::VerifyCommitment& a) -> bool {
                if (a.commitmentIndex >= enemyCommitments.size()) return false;
                return enemyCommitments[a.commitmentIndex]->verify(state);
            },

        }, gameActionLog[i]);

        if (!ok) {
            // TODO: print something like "verification failed at action i"
            return false;
        }
    }

    return true;
}
