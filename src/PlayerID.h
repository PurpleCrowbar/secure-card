#pragma once
#include <cstdint>

enum class PlayerID : std::uint8_t {
    ONE,
    TWO
};

namespace PlayerIDUtils {
    [[nodiscard]] static PlayerID getOpponent(PlayerID playerId) {return static_cast<PlayerID>(static_cast<std::uint8_t>(playerId) ^ 1);}
}

