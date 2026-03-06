#include "LightningBolt.h"

void LightningBolt::resolve(Game& game, PlayerID controller) {
    game.dealDamage(PlayerIDUtils::getOpponent(controller), 3);
}
