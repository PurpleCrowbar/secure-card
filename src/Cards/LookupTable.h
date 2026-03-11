#pragma once
#include <optional>
#include <unordered_map>
#include "../Cryptosystem.h"
#include "CardID.h"

class LookupTable {
public:
    static const LookupTable& instance() {
        static LookupTable table;
        return table;
    }

    [[nodiscard]] std::optional<CardID> getCardID(const Point& cardPoint) const;

    LookupTable(const LookupTable&) = delete;
    LookupTable& operator=(const LookupTable&) = delete;

private:
    LookupTable();
    bool tryLoadFromFile();
    void generateTable();
    void saveToFile() const;

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
};
