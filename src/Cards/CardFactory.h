#pragma once
#include <memory>
#include "Card.h"
#include "CardID.h"
// TODO: Add "CardImplementations/All.h" for storing these headers?
#include "CardImplementations/Disorganize.h"
#include "CardImplementations/LightningBolt.h"

namespace CardFactory {
    static std::unique_ptr<Card> create(const CardID id) {
        switch (id) {
            using enum CardID;
            case LIGHTNING_BOLT: return std::make_unique<LightningBolt>();
            case DISORGANIZE: return std::make_unique<Disorganize>();
            default: throw std::runtime_error("Unknown card ID");
        }
    }
};
