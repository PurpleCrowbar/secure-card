#include "Jettison.h"

void Jettison::resolve(Game& game, PlayerID controller) {
    game.mill(controller, 6);
}
