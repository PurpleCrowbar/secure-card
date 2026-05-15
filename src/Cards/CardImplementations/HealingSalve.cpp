#include "HealingSalve.h"

void HealingSalve::resolve(Game& game, PlayerID controller) {
    game.gainLife(controller, 4);
}
