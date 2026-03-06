#pragma once
#include "CardID.h"
#include "../PlayerID.h"
#include "../Game/Game.h"
#include <string>

class Card {
public:
    explicit Card(CardID id) : id(id) {}
    virtual ~Card() = default;

    virtual void resolve(Game& game, PlayerID controller) = 0;

    [[nodiscard]] virtual int getManaCost() const = 0;
    [[nodiscard]] virtual std::string getName() const = 0;
    [[nodiscard]] virtual std::string getRulesText() const = 0;
    // TODO: make this a lower number (e.g., 3) when there are more cards in the game
    [[nodiscard]] virtual uint8_t getMaxCopies() const { return 30; };

protected:
    CardID id;
};
