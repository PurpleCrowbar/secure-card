#pragma once
#include "../Card.h"

class SharedKnowledge final : public Card {
public:
    SharedKnowledge() : Card(CardID::SHARED_KNOWLEDGE) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 2; }
    [[nodiscard]] std::string getName() const override { return "Shared Knowledge"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Both players draw 3 cards.";
    }
};
