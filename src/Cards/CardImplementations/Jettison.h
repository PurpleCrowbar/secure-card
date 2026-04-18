#pragma once
#include "../Card.h"

class Jettison final : public Card {
public:
    Jettison() : Card(CardID::JETTISON) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 1; }
    [[nodiscard]] std::string getName() const override { return "Jettison"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Remove the top six cards from your deck.";
    }
};
