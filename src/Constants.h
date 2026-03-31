#pragma once
#include <cstdint>
#include "Cards/CardID.h"

namespace Constants {
    constexpr uint16_t SCHEMA_VERSION  = 1;
    constexpr uint8_t  MAX_DECK_SIZE   = 200;
    constexpr uint16_t MAX_CARD_ID     = static_cast<std::underlying_type_t<CardID>>(CardID::End); // TODO: C++23, to_underlying
    constexpr uint8_t MAX_HAND_SIZE    = 10;
}
