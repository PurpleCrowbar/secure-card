#include "LookupTable.h"
#include <fstream>
#include <format>
#include <iostream>
#include <stdexcept>
#include "../Constants.h"

static constexpr const char* LOOKUP_TABLE_FILE = "lookup_table";

LookupTable::LookupTable() {
    if (tryLoadFromFile()) {
        std::cout << "Loaded lookup table from file.\n";
        return;
    }
    std::cout << "Generating lookup table...\n";
    generateTable();
    saveToFile();
    std::cout << "Lookup table generated and saved.\n";
}

/**
 * @return False if file missing, corrupted, or header vals are mismatched
 */
bool LookupTable::tryLoadFromFile() {
    std::ifstream file(LOOKUP_TABLE_FILE, std::ios::binary);
    if (!file) return false;  // file doesn't exist

    // Schema version comes first. if it doesn't match, don't bother trying to read the rest
    uint16_t schemaVersion;
    file.read(reinterpret_cast<char*>(&schemaVersion), sizeof(schemaVersion));
    if (!file || schemaVersion != Constants::SCHEMA_VERSION) {
        std::cout << "Schema version mismatch, regenerating.\n";
        return false;
    }
    // max deck size = maximum number of cards that can exist in a deck. the same as "max nonce"
    uint8_t maxDeckSize;
    file.read(reinterpret_cast<char*>(&maxDeckSize), sizeof(maxDeckSize));
    if (!file || maxDeckSize != Constants::MAX_DECK_SIZE) {
        std::cout << "Max deck size mismatch, regenerating.\n";
        return false;
    }
    // the max card ID constant actually gets the ID of "None", the final enum value. this is totally fine, but must
    // be taken into consideration during implementation
    uint16_t maxCardId;
    file.read(reinterpret_cast<char*>(&maxCardId), sizeof(maxCardId));
    if (!file || maxCardId != Constants::MAX_CARD_ID) {
        std::cout << "Max card ID mismatch, regenerating.\n";
        return false;
    }
    // now that header is read and validated, we can read the entry data from the file
    lookupTable.reserve(static_cast<size_t>(Constants::MAX_CARD_ID) * Constants::MAX_DECK_SIZE);
    for (uint16_t cardId = 0; cardId < Constants::MAX_CARD_ID; ++cardId) {
        for (Nonce nonce = 0; nonce < Constants::MAX_DECK_SIZE; ++nonce) {
            Point point;
            uint16_t storedId;
            file.read(reinterpret_cast<char*>(point.data()), POINT_BYTES);
            file.read(reinterpret_cast<char*>(&storedId), sizeof(storedId));
            if (!file) {
                std::cout << "Unexpected EOF while reading, regenerating.\n";
                lookupTable.clear();
                return false;
            }
            lookupTable.emplace(point, static_cast<CardID>(storedId));
        }
    }

    return true;
}

void LookupTable::generateTable() {
    lookupTable.reserve(static_cast<size_t>(Constants::MAX_CARD_ID) * Constants::MAX_DECK_SIZE);
    for (uint16_t cardId = 0; cardId < Constants::MAX_CARD_ID; ++cardId) {
        for (Nonce nonce = 0; nonce < Constants::MAX_DECK_SIZE; ++nonce) {
            Point point = encodeToPoint(static_cast<CardID>(cardId), nonce);
            if (!lookupTable.emplace(point, static_cast<CardID>(cardId)).second) {
                throw std::runtime_error(std::format(
                    "Collision during generation: cardId={}, nonce={}", cardId, nonce
                ));
            }
        }
    }
}

// TODO: Program should continue to operate even when serialisation impossible (e.g., because in read-only directory)
void LookupTable::saveToFile() const {
    std::ofstream file(LOOKUP_TABLE_FILE, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open lookup table file for writing.");

    // write the header constants to the file
    const uint16_t schemaVersion = Constants::SCHEMA_VERSION;
    const uint8_t  maxDeckSize = Constants::MAX_DECK_SIZE;
    const uint16_t maxCardId = Constants::MAX_CARD_ID;
    file.write(reinterpret_cast<const char*>(&schemaVersion), sizeof(schemaVersion));
    file.write(reinterpret_cast<const char*>(&maxDeckSize), sizeof(maxDeckSize));
    file.write(reinterpret_cast<const char*>(&maxCardId), sizeof(maxCardId));

    // write entry data. must iterate in same order as generation so file contents are deterministic
    for (uint16_t cardId = 0; cardId < Constants::MAX_CARD_ID; ++cardId) {
        for (Nonce nonce = 0; nonce < Constants::MAX_DECK_SIZE; ++nonce) {
            Point point = encodeToPoint(static_cast<CardID>(cardId), nonce);
            file.write(reinterpret_cast<const char*>(point.data()), POINT_BYTES);
            file.write(reinterpret_cast<const char*>(&cardId),      sizeof(cardId));
        }
    }

    if (!file) throw std::runtime_error("Failed to write lookup table to disk.");
}

std::optional<CardID> LookupTable::getCardID(const Point& cardPoint) const {
    auto it = lookupTable.find(cardPoint);
    return it != lookupTable.end() ? std::optional(it->second) : std::nullopt;
}
