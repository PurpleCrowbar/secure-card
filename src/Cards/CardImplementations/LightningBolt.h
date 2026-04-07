#pragma once
#include "../Card.h"

class LightningBolt final : public Card {
public:
    LightningBolt() : Card(CardID::LIGHTNING_BOLT) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 1; }
    [[nodiscard]] std::string getName() const override { return "Lightning Bolt"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Deal 3 damage to your opponent.";
    }
};
