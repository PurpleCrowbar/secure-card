#pragma once
#include "CardID.h"
#include "LookupTable.h"
#include <optional>
#include <queue>
#include <set>
#include <variant>
#include <vector>

class Deck {
public:
    // Constructor for empty deck
    Deck();
    // Constructor for Deck object with encrypted cards + local keys. vector should be sorted with top card at index 0
    explicit Deck(const std::vector<std::pair<Point, Scalar>>& encryptedDeck);
    // Constructor for a vector of plaintext card IDs (plaintext deck)
    explicit Deck(const std::vector<CardID>& plaintextDeck);
    void setPlaintextContents(const std::unordered_map<CardID, uint8_t>& deckContents);
    void setPlaintextContents(const std::vector<CardID>& deckContents);

    // Setters
    bool addOpponentKey(uint8_t index, const Scalar& remoteKey);
    // TODO: addRandomCard for random cards unknown by both players. would have following signature:
    // bool addRandomCard(const std::set<CardID>& setOfPossibilities, const uint32_t& localRandomNum);
    bool addUnencryptedCard(CardID id, uint8_t index = 0);
    // Add encrypted card without keys (e.g., because of opponent's Brainstorm/Donation effect)
    bool addCardSolelyEncryptedByOpponent(const Point& cardCiphertext, uint8_t index = 0);
    bool addCardSolelyEncryptedLocally(CardID id, const Scalar& localKey, uint8_t index = 0);
    bool fullyDecrypt(std::queue<Scalar>& remoteKeys);
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
    // key = card ID, val = quantity present. Tracks deck contents irrespective of order.
    // For example, if contents = {BOLT, ELIXIR, BOLT} (encrypted), plaintextContents = { {BOLT, 2}, {ELIXIR, 1} }
    std::unordered_map<CardID, uint8_t> plaintextContents;
    const LookupTable& lookupTable;
    // TODO: should we add "Freshly Shuffled" tracker which, when false, prevents calling fullyDecrypt?
};
