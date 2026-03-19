#pragma once
#include <memory>
#include <vector>
#include "GameAction.h"
#include "../Commitments/Commitment.h"
#include "../Cryptosystem.h"

/**
 * Runs after the game is over. Detects cheating by verifying all game actions against a perfect-knowledge game state.
 */
class GameVerifier {
public:
    GameVerifier(const std::map<CardID, uint8_t>& localDeckContents, PlayerID localPlayer);

    void initialiseOpponentDeck(const std::vector<std::pair<Point, Scalar>>& encryptedDeck);
    [[nodiscard]] bool fullyDecryptInitialOpponentDeck(const std::vector<Scalar>& remoteKeys);
    void setPlayerGoingFirst(PlayerID playerId);

    void addAction(ActionEntry action);
    void addEnemyCommitment(std::unique_ptr<Commitment> commitment);

    // End of game
    void logLocalShuffleSeed(PlayerID deckOwner, ShuffleSeed seed); // log one of our shuffle seeds for either deck
    void addAllRemoteShuffleSeeds(PlayerID deckOwner, const std::vector<ShuffleSeed>& remoteSeeds); // add all remote shuffle seeds after the game
    [[nodiscard]] bool decryptEnemyCommitments(const std::vector<std::vector<Scalar>>& keys);

    // Getters
    [[nodiscard]] const std::vector<std::vector<Scalar>>& getLocalCommitmentKeys() const;
    [[nodiscard]] std::vector<ShuffleSeed> getLocalShuffleSeedsForOurDeck() const;
    [[nodiscard]] std::vector<ShuffleSeed> getLocalShuffleSeedsForOpponentsDeck() const;

    [[nodiscard]] bool run();

private:
    struct ShuffleSeeds {
        std::vector<ShuffleSeed> local; // seeds we generated
        std::vector<ShuffleSeed> remote; // seeds opponent generated (which we receive at game end)
    };

    GameState state;
    PlayerID localPlayer;

    std::vector<ActionEntry> actions;
    // Commitments made by the remote player (i.e., the opponent)
    std::vector<std::unique_ptr<Commitment>> enemyCommitments;

    // Keys generated as part of a commitment made by the local player. Sent to remote at end of game.
    // Each subvector pertains to one commitment. So, localCommitmentKeys[0] = keys for the first commitment we made.
    std::vector<std::vector<Scalar>> localCommitmentKeys;
    ShuffleSeeds ourDeckSeeds;
    ShuffleSeeds opponentDeckSeeds;
};
