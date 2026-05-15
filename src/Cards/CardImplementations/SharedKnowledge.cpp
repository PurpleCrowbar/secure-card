#include "SharedKnowledge.h"

void SharedKnowledge::resolve(Game& game, PlayerID controller) {
    game.drawCards(controller, 3);
    game.drawCards(PlayerIDUtils::getOpponent(controller), 3);
}
