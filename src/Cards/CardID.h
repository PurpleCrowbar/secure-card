#pragma once
#include <cstdint>
#include <type_traits>

enum class CardID : uint16_t {
    LIGHTNING_BOLT,
    End // used to get enum val count and max enum val
};

static constexpr std::underlying_type_t<CardID> MaxCardID {static_cast<std::underlying_type_t<CardID>>(CardID::End) - 1};
