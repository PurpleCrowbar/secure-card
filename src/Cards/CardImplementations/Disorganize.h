#pragma once
#include "../Card.h"

class Disorganize final : public Card {
public:
    Disorganize() : Card(CardID::DISORGANIZE) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 1; }
    [[nodiscard]] std::string getName() const override { return "Disorganize"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Shuffle your deck, then draw a card.";
    }
};
