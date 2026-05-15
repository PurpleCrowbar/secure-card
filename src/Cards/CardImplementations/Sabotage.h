#pragma once
#include "../Card.h"

class Sabotage final : public Card {
public:
    Sabotage() : Card(CardID::SABOTAGE) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 2; }
    [[nodiscard]] std::string getName() const override { return "Sabotage"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Shuffle three Junks into your opponent's deck.";
    }
};
