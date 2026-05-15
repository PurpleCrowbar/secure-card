#include "Meditate.h"

void Meditate::resolve(Game& game, PlayerID controller) {
    game.drawCards(controller, 2);
}
