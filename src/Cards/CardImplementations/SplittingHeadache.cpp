#include "SplittingHeadache.h"

void SplittingHeadache::resolve(Game& game, PlayerID controller) {
    PlayerID opponent = PlayerIDUtils::getOpponent(controller);
    auto damageAmount = game.getHandSize(opponent);
    if (damageAmount < 2) damageAmount = 0;
    else damageAmount -= 2;
    game.dealDamage(opponent, damageAmount);
}
