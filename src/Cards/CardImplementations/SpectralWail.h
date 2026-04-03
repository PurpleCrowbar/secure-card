#pragma once
#include "../Card.h"

class SpectralWail final : public Card {
public:
    SpectralWail() : Card(CardID::SPECTRAL_WAIL) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 2; }
    [[nodiscard]] std::string getName() const override { return "Spectral Wail"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Opponent discards a card at random.";
    }
};
