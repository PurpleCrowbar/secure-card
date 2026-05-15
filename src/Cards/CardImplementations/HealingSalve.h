#pragma once
#include "../Card.h"

class HealingSalve final : public Card {
public:
    HealingSalve() : Card(CardID::HEALING_SALVE) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 1; }
    [[nodiscard]] std::string getName() const override { return "Healing Salve"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "You heal 4 life.";
    }
};
