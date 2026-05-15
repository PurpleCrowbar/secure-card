#pragma once
#include "../Card.h"

class Junk final : public Card {
public:
    Junk() : Card(CardID::JUNK) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 2; }
    [[nodiscard]] std::string getName() const override { return "Junk"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Draw a card.";
    }
};