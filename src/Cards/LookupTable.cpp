#include "LookupTable.h"
#include "CardFactory.h"
#include "CardSets.h"
#include <format>

LookupTable::LookupTable() {
    // Note: The lookup table is only populated with constructed-legal cards on initialisation. Created cards are added when necessary
    for (const auto id : CardSets::ConstructedLegalCards) {
        for (Nonce nonce = 0; nonce < CardFactory::create(id)->getMaxCopies(); nonce++) {
            Point pointRepresentation = encodeToPoint(id, nonce);
            if (lookupTable.insert({pointRepresentation, id}).second) {
                currentGreatestNonce.insert_or_assign(id, nonce);
                continue;
            }
            throw std::runtime_error(std::format(
                "Hash collision detected:\nPrevious: {}\nNew: {}",
                static_cast<uint16_t>(lookupTable.at(pointRepresentation)), static_cast<uint16_t>(id)
            ));
        }
    }
}

std::optional<CardID> LookupTable::getCardID(const Point& cardPoint) const {
    auto it = lookupTable.find(cardPoint);
    return it != lookupTable.end() ? std::optional(it->second) : std::nullopt;
}

/**
 * Adds specified card ID to lookup table. If already present, adds value with incrementation of greatest nonce
 * @param id Card ID of card being added to lookup table
 * @return True if insertion was successful, false if insertion unnecessary. Failure results in error
 */
bool LookupTable::addCardID(CardID id) {
    if (!maxDeckSize) [[unlikely]] throw std::runtime_error("Maximum deck size undefined in LookupTable. Call setMaxDeckSize in Game instantiation.");

    if (!currentGreatestNonce.contains(id)) {
        // If card ID not already present in lookup table, add it with nonce = 0
        Point pointRep = encodeToPoint(id, 0);
        if (lookupTable.insert({pointRep, id}).second) {
            currentGreatestNonce.insert({id, 0});
            return true;
        }
        throw std::runtime_error(std::format(
            "LookupTable maps are out of sync. Hash collision detected:\nPrevious: {}\nNew: {}",
            static_cast<uint16_t>(lookupTable.at(pointRep)), static_cast<uint16_t>(id)
        ));
    }

    Nonce newGreatestNonce = currentGreatestNonce.at(id) + 1;
    if (newGreatestNonce >= maxDeckSize) return false; // no insertion necessary
    Point pointRep = encodeToPoint(id, newGreatestNonce);
    if (lookupTable.insert({pointRep, id}).second) {
        currentGreatestNonce.insert_or_assign(id, newGreatestNonce);
        return true;
    }
    throw std::runtime_error(std::format(
        "LookupTable maps are out of sync. Hash collision detected:\nPrevious: {}\nNew: {}",
        static_cast<uint16_t>(lookupTable.at(pointRep)), static_cast<uint16_t>(id)
    ));
}

/**
 * Must be set during the instantiation of the Game object; this class isn't responsible for this info.
 */
void LookupTable::setMaxDeckSize(uint8_t maxDeckSize) {
    this->maxDeckSize = maxDeckSize;
}
