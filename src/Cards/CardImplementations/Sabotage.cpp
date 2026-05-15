#include "Sabotage.h"

void Sabotage::resolve(Game& game, PlayerID controller) {
    auto shufflingPlayer = PlayerIDUtils::getOpponent(controller);
    for (int i = 0; i < 3; i++) {
        game.addUnencryptedCardToDeck(shufflingPlayer, CardID::JUNK, 0);
    }
    game.performShuffle(shufflingPlayer);
}
