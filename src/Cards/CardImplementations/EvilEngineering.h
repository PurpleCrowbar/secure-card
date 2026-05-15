#pragma once
#include "../Card.h"

class EvilEngineering final : public Card {
public:
    EvilEngineering() : Card(CardID::EVIL_ENGINEERING) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 3; }
    [[nodiscard]] std::string getName() const override { return "Evil Engineering"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Add Dark Intimations to the bottom of your deck.";
    }
};
