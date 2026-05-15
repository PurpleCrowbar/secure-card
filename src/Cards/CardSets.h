#pragma once
#include "CardID.h"
#include <set>

namespace CardSets {
    using enum CardID;

    static const std::set ConstructedLegalCards {
        LIGHTNING_BOLT,
        DISORGANIZE,
        SPECTRAL_WAIL,
        JETTISON,
        HEALING_SALVE,
        SABOTAGE
    };

    // Cards created by effects and other cards
    static const std::set CreatedCards {
        JUNK
    };
}
