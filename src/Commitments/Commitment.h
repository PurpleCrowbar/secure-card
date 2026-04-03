#pragma once
#include "../PlayerID.h"
#include "../Cryptosystem.h"
#include "../Game/GameState.h"

/**
 * Interface from which all commitments derive.
 */
class Commitment {
public:
    explicit Commitment(PlayerID committer) : committer(committer) {}
    virtual ~Commitment() = default;

    [[nodiscard]] virtual size_t getRemainingRequiredKeysCount() const = 0;

    /**
     * Called after the key dump to "decrypt" the commitment. The vector of keys needs to be ordered in
     * accordance with how the ciphertexts were originally ordered.
     * @param keys List of keys for this commitment
     * @return False if invalid number of keys or any card failed to decrypt
     */
    [[nodiscard]] virtual bool populateRemainingKeys(const std::vector<Scalar>& keys) = 0;

    // Called during re-simulation. gameState must match the state at the moment this commitment was made
    [[nodiscard]] virtual bool verify(const GameState& gameState) = 0;

    PlayerID committer;  // who created the ciphertexts (and owes us keys)
};
