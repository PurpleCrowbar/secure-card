#pragma once
#include <memory>
#include "Card.h"
#include "CardID.h"
// TODO: Add "CardImplementations/All.h" for storing these headers?
#include "CardImplementations/DarkIntimations.h"
#include "CardImplementations/Disorganize.h"
#include "CardImplementations/EvilEngineering.h"
#include "CardImplementations/HealingSalve.h"
#include "CardImplementations/Jettison.h"
#include "CardImplementations/Junk.h"
#include "CardImplementations/LightningBolt.h"
#include "CardImplementations/Meditate.h"
#include "CardImplementations/Sabotage.h"
#include "CardImplementations/SpectralWail.h"
#include "CardImplementations/TheSuperweapon.h"

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
            case EVIL_ENGINEERING: return std::make_unique<EvilEngineering>();
            case DARK_INTIMATIONS: return std::make_unique<DarkIntimations>();
            case THE_SUPERWEAPON: return std::make_unique<TheSuperweapon>();
            case MEDITATE: return std::make_unique<Meditate>();
            default: throw std::runtime_error("Unknown card ID");
        }
    }
};
