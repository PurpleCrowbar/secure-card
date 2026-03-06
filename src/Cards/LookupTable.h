#pragma once
#include "../Cryptosystem.h"
#include "CardID.h"
#include <optional>
#include <unordered_map>


// TODO: Must be made non-dynamic with all possible values precomputed (preferably at compile time). See "frozen" lib
/**
 * Used to get card ID values from their curve-point representations.
 */
class LookupTable {
public:
    LookupTable();

    [[nodiscard]] std::optional<CardID> getCardID(const Point& cardPoint) const;
	bool addCardID(CardID id);
	void setMaxDeckSize(uint8_t maxDeckSize);

private:

    /**
     * Hasher struct for determining how Points are hashed in the case of hash maps
     */
	// TODO: Reducing a 256 bit Point to a 64 bit hash seems like a good way to guarantee collisions... right?
	struct PointHash {
		std::size_t operator()(const Point& p) const {
			std::size_t result = 0;
			// FNV-1a hash. fast and simple to implement, so great for hash maps. non-cryptographic
			for (unsigned char byte : p) {
				result ^= byte;
				result *= 0x100000001b3; // FNV prime
			}
			return result;
		}
	};

    std::unordered_map<Point, CardID, PointHash> lookupTable;
	std::unordered_map<CardID, Nonce> currentGreatestNonce;
	uint8_t maxDeckSize = 0;
};
