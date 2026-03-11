#include "Disorganize.h"

void Disorganize::resolve(Game &game, PlayerID controller) {
    game.performShuffle(controller);
    game.drawCards(controller, 1);
}
