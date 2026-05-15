#pragma once
#include <cstdint>
#include <type_traits>

enum class CardID : uint16_t {
    LIGHTNING_BOLT,
    DISORGANIZE,
    SPECTRAL_WAIL,
    JETTISON,
    HEALING_SALVE,
    SABOTAGE,
    JUNK,
    EVIL_ENGINEERING,
    DARK_INTIMATIONS,
    THE_SUPERWEAPON,
    MEDITATE,
    SHARED_KNOWLEDGE,
    End // used to get enum val count and max enum val
};

static constexpr std::underlying_type_t<CardID> MaxCardID {static_cast<std::underlying_type_t<CardID>>(CardID::End) - 1};
