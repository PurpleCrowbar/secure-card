#pragma once
#include "../Card.h"

class SplittingHeadache final : public Card {
public:
    SplittingHeadache() : Card(CardID::SPLITTING_HEADACHE) {}

    void resolve(Game& game, PlayerID controller) override;

    [[nodiscard]] int getManaCost() const override { return 2; }
    [[nodiscard]] std::string getName() const override { return "Splitting Headache"; }
    [[nodiscard]] std::string getRulesText() const override {
        return "Deal X damage to opponent, where X is the number of cards in their hand.";
    }
};
