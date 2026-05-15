#pragma once
#include "../Card.h"

class TheSuperweapon final : public Card {
public:
    TheSuperweapon() : Card(CardID::THE_SUPERWEAPON) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 3; }
    [[nodiscard]] std::string getName() const override { return "The Superweapon"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Deal 100 damage to your opponent.";
    }
};
