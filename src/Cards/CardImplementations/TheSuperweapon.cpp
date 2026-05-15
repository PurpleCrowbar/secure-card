#include "TheSuperweapon.h"

void TheSuperweapon::resolve(Game &game, PlayerID controller) {
    game.dealDamage(PlayerIDUtils::getOpponent(controller), 100);
}
