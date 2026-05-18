#pragma once
#include "CardID.h"
#include "LookupTable.h"
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

class Deck {
public:
    // Constructor for empty deck
    Deck();
    [[deprecated("No known usage")]] explicit Deck(const std::vector<std::pair<Point, Scalar>>& encryptedDeck);
    // Constructor for a vector of plaintext card IDs (plaintext deck)
    explicit Deck(const std::map<CardID, uint8_t>& plaintextDeckContents);
    void setEncryptedContents(const std::vector<std::pair<Point, Scalar>>& encryptedDeck);

    // Setters
    void addOpponentKey(uint8_t index, const Scalar& remoteKey);
    // TODO: addRandomCard for random cards unknown by both players. would have following signature:
    // bool addRandomCard(const std::set<CardID>& setOfPossibilities, const uint32_t& localRandomNum);
    void addUnencryptedCard(CardID id, uint8_t index = 0);
    // Add encrypted card without keys (e.g., because of opponent's Brainstorm/Donation effect)
    void addCardSolelyEncryptedByOpponent(const Point& cardCiphertext, uint8_t index = 0);
    // As above, but with the local player's Brainstorm/Donation effects
    void addCardSolelyEncryptedLocally(CardID id, const Scalar& localKey, uint8_t index = 0);
    [[nodiscard]] std::optional<CardID> draw();
    [[nodiscard]] std::vector<CardID> mill(uint8_t count);
    std::optional<CardID> removeCardAtIndex(uint8_t index);
    bool setKnownToOpponent(uint8_t index, bool known = true);
    void setKnownToOpponentAtIndices(const std::set<uint8_t>& indices);
    void disableContentsTracking();

    // Getters
    [[nodiscard]] std::optional<CardID> getCardIDAtIndex(uint8_t index) const;
    [[nodiscard]] std::optional<Scalar> getLocalKey(uint8_t index) const;
    [[nodiscard]] uint8_t getSize() const;
    [[nodiscard]] std::set<uint8_t> getIndicesOfCardsSolelyEncryptedByOpponent() const;
    [[nodiscard]] std::set<uint8_t> getIndicesOfCardsSolelyEncryptedLocally() const;
    [[nodiscard]] std::map<CardID, uint8_t> getContents() const;
    [[nodiscard]] bool isKnownToOpponent(uint8_t index) const;
    [[nodiscard]] std::vector<Scalar> getLocalKeysAtIndices(const std::set<uint8_t>& indices) const;
    [[nodiscard]] std::set<uint8_t> getIndicesOfLocallyUnknown(std::optional<uint8_t> indexRange = std::nullopt) const;
    [[nodiscard]] std::set<uint8_t> getIndicesOfRemotelyUnknown(std::optional<uint8_t> indexRange = std::nullopt) const;

    // Methods used during post-game verification
    void v_setPlaintextContents(const std::map<CardID, uint8_t>& newPlaintextContents);
    void v_shuffleWithSeeds(ShuffleSeed ownerSeed, ShuffleSeed enemySeed);

private:
    struct CardEntry {
        // CardID = plaintext, Point = ciphertext. variant tracks local encryption status of card
        std::variant<CardID, Point> card;
        // first = local, second = remote
        std::pair<std::optional<Scalar>, std::optional<Scalar>> keys;
        bool knownToOpponent = false;
    };

    // Ordered top-to-bottom, meaning index 0 = top of deck
    std::vector<CardEntry> contents;
    /**
     * key = card ID, val = quantity present. Tracks deck contents irrespective of order.
     * For example, if contents = {BOLT, ELIXIR, BOLT} (encrypted), then plaintextContents = { {BOLT, 2}, {ELIXIR, 1} }.
     * This is used for tracking the local player's deck contents for shuffling and similar actions, and is used for
     * both players' decks during verification. Accordingly, it should be std::nullopt for the remote deck at gametime.
     */
    std::optional<std::map<CardID, uint8_t>> plaintextContents {std::in_place};
    const LookupTable& lookupTable;
};
