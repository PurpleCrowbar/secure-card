#pragma once
#include "CardID.h"
#include <set>

namespace CardSets {
    using enum CardID;
    static const std::set ConstructedLegalCards {
        LIGHTNING_BOLT,
        DISORGANIZE
    };
}
