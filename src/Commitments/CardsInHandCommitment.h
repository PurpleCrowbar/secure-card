#pragma once
#include "Commitment.h"

class CardsInHandCommitment final : public Commitment {
public:
    // Commitment for hidden cards in hand
    explicit CardsInHandCommitment(PlayerID committer, const std::vector<Point>& ciphertexts)
    : Commitment(committer), cards(ciphertexts.begin(), ciphertexts.end()) {}
    // Commitment for revealed cards in hand
    explicit CardsInHandCommitment(PlayerID committer, const std::vector<CardID>& plaintexts)
    : Commitment(committer), cards(plaintexts.begin(), plaintexts.end()) {}

    [[nodiscard]] size_t getRemainingRequiredKeysCount() const override {
        size_t count = 0;
        for (const auto& card : cards) {
            if (std::holds_alternative<Point>(card)) count++;
        }
        return count;
    }

    [[nodiscard]] bool populateRemainingKeys(const std::vector<Scalar>& keys) override {
        if (keys.size() != getRemainingRequiredKeysCount()) return false;

        size_t curKey = 0;
        for (auto& card : cards) {
            if (std::holds_alternative<CardID>(card)) continue;
            auto decryptedCard = LookupTable::instance().getCardID(decrypt(
                std::get<Point>(card), keys[curKey++]
            ));
            if (!decryptedCard.has_value()) return false;
            card = decryptedCard.value();
        }
        return true;
    }

    [[nodiscard]] bool verify(const GameState& gameState) override {
        if (getRemainingRequiredKeysCount() != 0) return false;

        auto hand = std::get<ClearHand>(gameState.getImmutablePlayerData(committer).hand).getUnorderedHandContents();
        for (const auto& card : cards) {
            const auto& cardId = std::get<CardID>(card);
            // hand doesn't contain committed card; verification failed, return false
            if (!hand.contains(cardId)) return false;
            // otherwise decrement quantity of this card in hand (it is accounted for)
            if (--hand[cardId] == 0) hand.erase(cardId);
        }
        // true if all cards in hand are accounted for, else false
        return hand.empty();
    }

private:
    std::vector<std::variant<Point, CardID>> cards;
};
