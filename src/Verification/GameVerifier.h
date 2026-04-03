#pragma once
#include <memory>
#include <vector>
#include "GameAction.h"
#include "../Commitments/Commitment.h"
#include "../Cryptosystem.h"
#include "../DeckCommitment.h"

/**
 * Runs after the game is over. Detects cheating by verifying all game actions against a perfect-knowledge game state.
 */
class GameVerifier {
public:
    GameVerifier(const std::map<CardID, uint8_t>& localDeckContents, PlayerID localPlayer);

    void initialiseOpponentDeck(const std::map<CardID, uint8_t>& plaintextDeckContents);
    void setPlayerGoingFirst(PlayerID playerId);

    void logAction(ActionEntry action);
    void logEnemyCommitment(std::unique_ptr<Commitment> commitment);
    /**
     * Logs the local commitment keys for this commitment. <b>Must use inverse keys as this is for decryption.</b>
     * Not to be confused with setLocalDeckCommitmentKey, which is used to verify the contents of players' decks haven't
     * been illegally modified during the game. Programmer's note: Templates must be defined in headers.
     * @param keys Keys for this commitment
     */
    template<std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, Scalar>
    void logLocalCommitmentKeys(R&& keys) {
        localCommitmentKeys.emplace_back(std::ranges::begin(keys), std::ranges::end(keys));
    }

    // Deck commitment (keyed hash of deck contents)
    void setLocalDeckCommitmentKey(const DeckHashKey& key);
    void setRemoteDeckCommitment(const DeckHash& hash);
    [[nodiscard]] const DeckHashKey& getLocalDeckCommitmentKey() const;
    [[nodiscard]] bool verifyRemoteDeckContents(const DeckHashKey& remoteKey, const std::map<CardID, uint8_t>& remoteDeckContents) const;

    // Shuffle seeds
    void logLocalShuffleSeed(PlayerID deckOwner, ShuffleSeed seed);
    void addAllRemoteShuffleSeeds(PlayerID deckOwner, const std::vector<ShuffleSeed>& remoteSeeds);
    [[nodiscard]] std::vector<ShuffleSeed> getLocalShuffleSeedsForOurDeck() const;
    [[nodiscard]] std::vector<ShuffleSeed> getLocalShuffleSeedsForOpponentsDeck() const;

    // Commitment keys
    [[nodiscard]] bool decryptEnemyCommitments(const std::vector<std::vector<Scalar>>& keys);
    [[nodiscard]] const std::vector<std::vector<Scalar>>& getLocalCommitmentKeys() const;

    // TODO: this could return a sophisticated, custom error type
    [[nodiscard]] bool run();

private:
    void v_shuffleLocalDeck(size_t shuffleSeedIndex);
    void v_shuffleOpponentsDeck(size_t shuffleSeedIndex);

    struct ShuffleSeeds {
        std::vector<ShuffleSeed> local; // seeds we generated
        std::vector<ShuffleSeed> remote; // seeds opponent generated (which we receive at game end)
    };

    GameState state;
    PlayerID localPlayer;

    // Sequential log of every game action taken
    std::vector<ActionEntry> gameActionLog;
    // Commitments made by the remote player (i.e., the opponent)
    std::vector<std::unique_ptr<Commitment>> enemyCommitments;

    // Keys generated as part of a commitment made by the local player. Sent to remote at end of game.
    // Each subvector pertains to one commitment. So, localCommitmentKeys[0] = keys for the first commitment we made.
    std::vector<std::vector<Scalar>> localCommitmentKeys;
    ShuffleSeeds ourDeckSeeds;
    ShuffleSeeds opponentDeckSeeds;

    // Deck commitment data
    DeckHashKey localDeckCommitmentKey {}; // our random key for our deck hash. revealed to opponent at game end
    DeckHash remoteDeckCommitment {}; // opponent's deck hash, received at game start
};
