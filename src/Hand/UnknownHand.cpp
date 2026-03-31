#include "UnknownHand.h"
#include "../Constants.h"

/**
 * @return False if hand full, else true
 */
bool UnknownHand::addCard() {
    if (cardCount >= Constants::MAX_HAND_SIZE) return false;
    cardCount++;
    return true;
}

/**
 * @return False if hand empty, else true
 */
bool UnknownHand::removeCard() {
    if (cardCount == 0) return false;
    cardCount--;
    return true;
}

uint8_t UnknownHand::getSize() const {
    return cardCount;
}

bool UnknownHand::isFull() const {
    return cardCount == Constants::MAX_HAND_SIZE;
}
