#pragma once
#include "Card.h"
#include "CardID.h"
#include "LightningBolt.h"
#include <memory>

namespace CardFactory {
    static std::unique_ptr<Card> create(const CardID id) {
        switch (id) {
            using enum CardID;
            case LIGHTNING_BOLT: return std::make_unique<LightningBolt>();
            default: throw std::runtime_error("Unknown card ID");
        }
    }
};
