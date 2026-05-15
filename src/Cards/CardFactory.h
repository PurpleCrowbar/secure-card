#pragma once
#include <memory>
#include "Card.h"
#include "CardID.h"
// TODO: Add "CardImplementations/All.h" for storing these headers?
#include "CardImplementations/Disorganize.h"
#include "CardImplementations/HealingSalve.h"
#include "CardImplementations/Jettison.h"
#include "CardImplementations/Junk.h"
#include "CardImplementations/LightningBolt.h"
#include "CardImplementations/Sabotage.h"
#include "CardImplementations/SpectralWail.h"

namespace CardFactory {
    static std::unique_ptr<Card> create(const CardID id) {
        switch (id) {
            using enum CardID;
            case LIGHTNING_BOLT: return std::make_unique<LightningBolt>();
            case DISORGANIZE: return std::make_unique<Disorganize>();
            case SPECTRAL_WAIL: return std::make_unique<SpectralWail>();
            case JETTISON: return std::make_unique<Jettison>();
            case HEALING_SALVE: return std::make_unique<HealingSalve>();
            case SABOTAGE: return std::make_unique<Sabotage>();
            case JUNK: return std::make_unique<Junk>();
            default: throw std::runtime_error("Unknown card ID");
        }
    }
};
