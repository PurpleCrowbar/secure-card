#include "DarkIntimations.h"

void DarkIntimations::resolve(Game& game, PlayerID controller) {
    game.addUnencryptedCardToDeckBottom(controller, CardID::THE_SUPERWEAPON);
}
