#include "Junk.h"

void Junk::resolve(Game& game, PlayerID controller) {
    game.drawCards(controller, 1);
}
