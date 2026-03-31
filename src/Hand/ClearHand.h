#pragma once
#include <map>
#include <vector>
#include "../Cards/CardID.h"

/**
 * Used by the local player during the game. Used for both players by the verifier at the end of the game.
 */
class ClearHand {
public:
    ClearHand() = default;

    // Setters
    bool addCard(CardID cardId);
    bool removeCard(CardID cardId);

    // Getters
    [[nodiscard]] std::map<CardID, uint8_t> getUnorderedHandContents() const;
    [[nodiscard]] uint8_t getSize() const;
    [[nodiscard]] bool isFull() const;
    [[nodiscard]] CardID getCardAtIndex(uint8_t index) const;

    // UI
    [[nodiscard]] std::vector<CardID> ui_getHandContents() const;

private:
    // Vector of cards in hand in the order drawn
    std::vector<CardID> cards {};
};
