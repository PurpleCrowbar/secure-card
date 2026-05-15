#pragma once
#include "../Card.h"

class DarkIntimations final : public Card {
public:
    DarkIntimations() : Card(CardID::DARK_INTIMATIONS) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 3; }
    [[nodiscard]] std::string getName() const override { return "Dark Intimations"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Add The Superweapon to the bottom of your deck.";
    }
};
