#pragma once
#include "CardID.h"
#include "LookupTable.h"
#include <optional>
#include <set>
#include <variant>
#include <vector>

class Deck {
public:
    // Constructor for empty deck
    Deck();
    [[deprecated("No currently known usage")]] explicit Deck(const std::vector<std::pair<Point, Scalar>>& encryptedDeck);
    // Constructor for a vector of plaintext card IDs (plaintext deck)
    explicit Deck(const std::vector<CardID>& plaintextDeck);
    void setEncryptedContents(const std::vector<std::pair<Point, Scalar>>& encryptedDeck);

    // Setters
    bool addOpponentKey(uint8_t index, const Scalar& remoteKey);
    // TODO: addRandomCard for random cards unknown by both players. would have following signature:
    // bool addRandomCard(const std::set<CardID>& setOfPossibilities, const uint32_t& localRandomNum);
    bool addUnencryptedCard(CardID id, uint8_t index = 0);
    // Add encrypted card without keys (e.g., because of opponent's Brainstorm/Donation effect)
    bool addCardSolelyEncryptedByOpponent(const Point& cardCiphertext, uint8_t index = 0);
    // As above, but with the local player's Brainstorm/Donation effects
    bool addCardSolelyEncryptedLocally(CardID id, const Scalar& localKey, uint8_t index = 0);
    bool fullyDecrypt(const std::vector<Scalar>& remoteKeys);
    [[nodiscard]] std::optional<CardID> draw();
    std::optional<CardID> removeCardAtIndex(uint8_t index);
    bool setKnownToOpponent(uint8_t index, bool known = true);

    // Getters
    [[nodiscard]] std::optional<CardID> getCardIDAtIndex(uint8_t index) const;
    [[nodiscard]] std::optional<Scalar> getLocalKey(uint8_t index) const;
    [[nodiscard]] uint8_t getSize() const;
    [[nodiscard]] std::set<uint8_t> getIndicesOfCardsSolelyEncryptedByOpponent() const;
    [[nodiscard]] std::set<uint8_t> getIndicesOfCardsSolelyEncryptedLocally() const;
    [[nodiscard]] std::unordered_map<CardID, uint8_t> getContents() const;
    [[nodiscard]] bool isKnownToOpponent(uint8_t index) const;

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
     * For example, if contents = {BOLT, ELIXIR, BOLT} (encrypted), plaintextContents = { {BOLT, 2}, {ELIXIR, 1} }
     * This is exclusively used for tracking the local player's deck contents for shuffling and similar actions.
     * Accordingly, it should be left untouched for the remote deck.
     */
    std::unordered_map<CardID, uint8_t> plaintextContents;
    const LookupTable& lookupTable;
    // TODO: should we add "Freshly Shuffled" tracker which, when false, prevents calling fullyDecrypt?
};
