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
    [[nodiscard]] virtual uint8_t getMaxCopies() const { return 5; };

protected:
    CardID id;
};
