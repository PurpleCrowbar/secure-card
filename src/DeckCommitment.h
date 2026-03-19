#pragma once
#include <array>
#include <map>
#include <vector>
#include <sodium.h>
#include "Cards/CardID.h"

constexpr std::size_t DECK_HASH_BYTES = crypto_generichash_BYTES; // 32
constexpr std::size_t DECK_HASH_KEY_BYTES = crypto_generichash_KEYBYTES; // 32

using DeckHash = std::array<unsigned char, DECK_HASH_BYTES>;
using DeckHashKey = std::array<unsigned char, DECK_HASH_KEY_BYTES>;

// TODO: Add these methods to a unique namespace?

/**
 * Deterministically serialises deck contents into a byte buffer.
 * @param contents Map of plaintext deck contents
 * @return Contents of the deck serialised to a vector of bytes
 */
inline std::vector<unsigned char> serialiseDeckContents(const std::map<CardID, uint8_t>& contents) {
    std::vector<unsigned char> data;
    data.reserve(contents.size() * 3); // 2 bytes CardID + 1 byte quantity per entry = 3 bytes per entry
    for (const auto& [id, quantity] : contents) {
        auto raw = static_cast<uint16_t>(id);
        data.push_back(static_cast<unsigned char>(raw & 0xFF)); // low byte bisection
        data.push_back(static_cast<unsigned char>((raw >> 8) & 0xFF)); // high byte bisection
        data.push_back(quantity);
    }
    return data;
}

/**
 * @return Random 32 byte key for use with libsodium's crypto_generichash keyed hash function function (BLAKE2b)
 */
inline DeckHashKey generateDeckHashKey() {
    DeckHashKey key;
    randombytes_buf(key.data(), key.size());
    return key;
}

/**
 * Computes a BLAKE2b keyed hash over the serialised deck contents which is sent
 * to our opponent at the start of the game, to be verified at the end.
 * @param key Random 32 byte key (must be stored and sent to opponent post-game)
 * @param contents Contents of deck as it existed before drawing any cards
 * @return 32 byte BLAKE2b hash of the given deck
 */
inline DeckHash computeDeckHash(const DeckHashKey& key, const std::map<CardID, uint8_t>& contents) {
    auto data = serialiseDeckContents(contents);
    DeckHash hash;
    crypto_generichash(
        hash.data(), DECK_HASH_BYTES,
        data.data(), data.size(),
        key.data(), DECK_HASH_KEY_BYTES
    );
    return hash;
}

/**
 * Verifies that the given deck contents and key produce the expected hash. Used on opponent's deck after the game.
 * @param expectedHash Hash received at the start of the game
 * @param key Key received at the end of the game
 * @param contents Plaintext deck contents received at the end of the game
 * @return True if the commitment is valid, else false
 */
inline bool verifyDeckCommitment(const DeckHash& expectedHash, const DeckHashKey& key, const std::map<CardID, uint8_t>& contents) {
    DeckHash recomputed = computeDeckHash(key, contents);
    return sodium_memcmp(recomputed.data(), expectedHash.data(), DECK_HASH_BYTES) == 0;
}
