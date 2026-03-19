#pragma once
#include "../PlayerID.h"
#include "../Cryptosystem.h"
#include "../Game/GameState.h"

/**
 * Interface from which all specific commitments derive.
 */
class Commitment {
public:
    virtual ~Commitment() = default;

    [[nodiscard]] virtual size_t getRemainingRequiredKeysCount() = 0;

    // Called after key dump to fill in missing keys.
    // The queue of keys corresponds 1:1 with the ciphertexts that
    // were missing keys (in the order they were logged).
    // TODO: return bool? true if successful, else false?
    virtual void populateRemainingKeys(const std::vector<Scalar>& keys) = 0;

    // Called during re-simulation. gameState must match the state at the moment this commitment was made
    [[nodiscard]] virtual bool verify(const GameState& gameState) = 0;

    PlayerID committer;  // who created the ciphertexts (and owes us keys)
};
