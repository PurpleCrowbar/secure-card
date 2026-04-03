#pragma once
#include <cstddef>
#include <algorithm>
#include <sodium.h>
#include <array>
#include <map>
#include <iomanip>
#include <random>
#include <stdexcept>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Constants.h"
#include "Cards/CardID.h"

constexpr std::size_t SCALAR_BYTES = crypto_core_ristretto255_SCALARBYTES;      // 32
constexpr std::size_t POINT_BYTES = crypto_core_ristretto255_BYTES;             // 32
constexpr std::size_t HASH_INPUT_BYTES = crypto_core_ristretto255_HASHBYTES;    // 64

// TODO: rename Scalar and Point to Key and CardPoint respectively?
using Scalar = std::array<unsigned char, SCALAR_BYTES>;
using Point = std::array<unsigned char, POINT_BYTES>;
using Nonce = uint8_t;
using ShuffleSeed = uint32_t;

struct PHKeyPair {
    Scalar k;
    Scalar k_inv;
};

/**
 * @return Randomised key pair for ECPH
 */
inline PHKeyPair generateKeyPair() {
    PHKeyPair keys;
    crypto_core_ristretto255_scalar_random(keys.k.data());
    if (crypto_core_ristretto255_scalar_invert(keys.k_inv.data(), keys.k.data()) != 0) {
        throw std::runtime_error("Failed to compute scalar inverse");
    }
    return keys;
}

/**
 * Converts card ID and nonce to 64 byte hash before encoding it to curve-point.
 * @param id Card ID to encode
 * @param nonce Accompanying nonce (if needed)
 * @return Curve-point representation of data
 */
inline Point encodeToPoint(CardID id, Nonce nonce = 0) {
    auto rawId = static_cast<std::underlying_type_t<CardID>>(id);
    std::vector data {
        static_cast<unsigned char>(rawId & 0xFF),  // low byte bisection
        static_cast<unsigned char>((rawId >> 8) & 0xFF),  // high byte bisection
        nonce
    };

    std::array<unsigned char, HASH_INPUT_BYTES> hashInput {};
    crypto_generichash(
        hashInput.data(), HASH_INPUT_BYTES,
        data.data(), data.size(),
        nullptr, 0
    );
    Point result;
    crypto_core_ristretto255_from_hash(result.data(), hashInput.data());
    return result;
}

/**
 * Encrypts a single message already in curve-point form.
 */
inline Point encrypt(const Point& message, const Scalar& key) {
    Point ciphertext;
    if (crypto_scalarmult_ristretto255(ciphertext.data(), key.data(), message.data()) != 0) {
        throw std::runtime_error("Encryption failed");
    }
    return ciphertext;
}

/**
 * Helper method that takes a raw CardID and encodes it to curve-point form before encrypting it.
 * @param id ID of card being encrypted
 * @param key Key being used to encrypt the card
 * @return The encrypted card ciphertext in point representation
 */
inline Point encrypt(CardID id, const Scalar& key) {
    return encrypt(encodeToPoint(id), key);
}

/**
 * Decrypts a card ciphertext with a given key.
 * @param ciphertext Card ciphertext
 * @param key Inverse of a key used to encrypt this card. May have been used by either player
 * @return Plaintext card-point representation. Lookup table required to get card ID
 */
inline Point decrypt(const Point& ciphertext, const Scalar& key) {
    Point plaintext;
    if (crypto_scalarmult_ristretto255(plaintext.data(), key.data(), ciphertext.data()) != 0) {
        throw std::runtime_error("Decryption failed");
    }
    return plaintext;
}

/**
 * Decrypts a card ciphertext with both the local key and the remote key.
 * @param ciphertext Card ciphertext
 * @param localKey Key for this card previously used by local player
 * @param remoteKey Key for this card previously used by remote player (opponent)
 * @return Plaintext card-point representation. Lookup table required to get card ID
 */
inline Point decrypt(const Point& ciphertext, const Scalar& localKey, const Scalar& remoteKey) {
    return decrypt(decrypt(ciphertext, localKey), remoteKey);
}

// --------------- Mental Poker ---------------

/**
 * Converts the plaintext contents of a deck into their card-point forms with random, unique nonces applied.
 * @param cards Map of card IDs to convert. Key = card ID, val = quantity to process
 * @return Ordered vector of card-point representations
 */
[[nodiscard]] inline std::vector<Point> convertCardsToPoints(const std::map<CardID, uint8_t>& cards) {
    std::vector<Point> points;
    for (const auto& [cardId, quantity] : cards) {
        std::unordered_set<Nonce> usedNonces;
        for (int i = 0; i < quantity; i++) {
            Nonce nonce = static_cast<Nonce>(randombytes_uniform(Constants::MAX_DECK_SIZE));
            while (usedNonces.contains(nonce)) nonce = (nonce + 1) % Constants::MAX_DECK_SIZE;
            usedNonces.insert(nonce);
            points.push_back(encodeToPoint(cardId, nonce));
        }
    }
    return points;
}

/**
 * Converts a vector of card IDs into a vector of points. Applies unique nonces to duplicate copies of cards.
 * @param cards Vector of card IDs to turn into points
 * @return Vector of points analogous to original vector
 */
[[nodiscard]] inline std::vector<Point> convertCardsToPoints_vec(std::vector<CardID> cards) {
    std::vector<Point> points;
    points.reserve(cards.size());

    std::unordered_map<CardID, std::unordered_set<Nonce>> usedNonces;

    for (const CardID cardId : cards) {
        auto& noncesForCard = usedNonces[cardId];
        Nonce nonce = static_cast<Nonce>(randombytes_uniform(Constants::MAX_DECK_SIZE));
        while (noncesForCard.contains(nonce)) nonce = (nonce + 1) % Constants::MAX_DECK_SIZE;
        noncesForCard.insert(nonce);
        points.push_back(encodeToPoint(cardId, nonce));
    }

    return points;
}

/**
 * <b>This is not a networked function.</b> This <i>only</i> shuffles a
 * local vector. Randomises the order of items in a vector using a given seed.
 * @param deck Reference to vector of card-points to be shuffled
 * @param seed Shuffle seed to use
 */
template <typename T>
void shuffleCards(std::vector<T>& deck, const std::optional<ShuffleSeed> seed = std::nullopt) {
    std::mt19937 gen(seed.value_or(std::random_device{}()));
    std::ranges::shuffle(deck, gen);
}

/**
 * Encrypts every card in a vector of card-points with a single key.
 * @param deck Reference to list of card-points to be encrypted
 * @param key Key to encrypt the cards with
 */
inline void encryptCardsWithKey(std::vector<Point>& deck, const Scalar& key) {
    for (auto& card : deck) card = encrypt(card, key);
}

/**
 * Decrypts every card in a vector of card-points with a single key.
 * @param deck Reference to list of card-points to be decrypted
 * @param keyInverse Key to decrypt the cards with
 */
inline void decryptCardsWithKey(std::vector<Point>& deck, const Scalar& keyInverse) {
    for (auto& card : deck) card = decrypt(card, keyInverse);
}

/**
 * Takes a list of card-points and encrypts each of them with a random key. Called as part of Mental Poker protocol
 * @param deck Vector of points to be encrypted with unique keys
 * @return Vector of encrypted card-points and their corresponding keys
 */
[[nodiscard]] inline std::vector<std::pair<Point, PHKeyPair>> encryptCardsWithIndividualKeys(const std::vector<Point>& deck) {
    std::vector<std::pair<Point, PHKeyPair>> newDeck;
    newDeck.reserve(deck.size());

    for (const auto& card : deck) {
        auto key = generateKeyPair();
        newDeck.emplace_back(encrypt(card, key.k), key);
    }

    return newDeck;
}
