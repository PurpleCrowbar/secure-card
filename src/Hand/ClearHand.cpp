#include "ClearHand.h"

#include <format>
#include <stdexcept>
#include "../Constants.h"

/**
 * @param cardId ID of card to add to hand
 * @return False if hand full, else true
 */
bool ClearHand::addCard(CardID cardId) {
    if (cards.size() >= Constants::MAX_HAND_SIZE) return false;
    cards.push_back(cardId);
    return true;
}

/**
 * @param cardId ID of card to remove
 * @return False if card not found, else true
 */
bool ClearHand::removeCard(CardID cardId) {
    auto it = std::ranges::find(cards, cardId);
    if (it == cards.end()) return false;
    cards.erase(it);
    return true;
}

std::map<CardID, uint8_t> ClearHand::getUnorderedHandContents() const {
    std::map<CardID, uint8_t> contents;
    for (const auto& card : cards) {
        contents[card]++;
    }
    return contents;
}

uint8_t ClearHand::getSize() const {
    return cards.size();
}

bool ClearHand::isFull() const {
    return cards.size() == Constants::MAX_HAND_SIZE;
}

/**
 * @param index Index of card in hand
 * @return Card ID if index specified within bounds, else throws
 */
CardID ClearHand::getCardAtIndex(uint8_t index) const {
    if (index >= cards.size()) throw std::runtime_error(std::format(
            "[ClearHand::getCardAtIndex] Index '{}' is out of bounds", index
    ));
    return cards[index];
}

/**
 * @return Vector of cards in the order they were drawn
 */
std::vector<CardID> ClearHand::ui_getHandContents() const {
    return cards;
}
