#pragma once
#include <queue>
#include "../PlayerID.h"
#include "../Cryptosystem.h"
#include "../Game/GameState.h"

class Commitment {
public:
    virtual ~Commitment() = default;

    // Called after key dump to fill in missing keys.
    // The queue of keys corresponds 1:1 with the ciphertexts that
    // were missing keys (in the order they were logged).
    virtual void populateRemainingKeys(const std::queue<Scalar>& keys) = 0;

    // Called during re-simulation. gameState is the state at the
    // moment this commitment was made.
    [[nodiscard]] virtual bool verify(const GameState& gameState) = 0;

    PlayerID committer;  // who created the ciphertexts (and owes us keys)
};
