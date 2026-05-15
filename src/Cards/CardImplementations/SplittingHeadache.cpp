#include "SplittingHeadache.h"

void SplittingHeadache::resolve(Game& game, PlayerID controller) {
    PlayerID opponent = PlayerIDUtils::getOpponent(controller);
    auto damageAmount = game.getHandSize(opponent);
    game.dealDamage(opponent, damageAmount);
}
