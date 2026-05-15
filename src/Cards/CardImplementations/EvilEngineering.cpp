#include "EvilEngineering.h"

void EvilEngineering::resolve(Game& game, PlayerID controller) {
    game.addUnencryptedCardToDeckBottom(controller, CardID::DARK_INTIMATIONS);
}
