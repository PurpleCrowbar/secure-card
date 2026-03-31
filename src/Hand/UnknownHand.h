#pragma once
#include "../Cards/CardID.h"

/**
 * Used to represent the remote player's hand during the game.
 */
class UnknownHand {
public:
    UnknownHand() = default;

    // Setters
    bool addCard();
    bool removeCard();

    // Getters
    [[nodiscard]] uint8_t getSize() const;
    [[nodiscard]] bool isFull() const;

private:
    uint8_t cardCount = 0;
};
