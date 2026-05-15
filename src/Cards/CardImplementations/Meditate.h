#pragma once
#include "../Card.h"

class Meditate final : public Card {
public:
    Meditate() : Card(CardID::MEDITATE) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 3; }
    [[nodiscard]] std::string getName() const override { return "Meditate"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Draw 2 cards.";
    }
};
