#pragma once
#include <variant>

#include "../PlayerID.h"
#include "../Cards/CardID.h"

namespace Action {
    struct DealDamage       { PlayerID target; int amount; };
    struct GainLife         { PlayerID target; int amount; };
    struct DrawCards        { PlayerID player; uint8_t count; };
    struct PlayCard         { PlayerID player; CardID card; };
    struct Discard          { PlayerID player; CardID card; };
    struct EndTurn          { PlayerID player; };
    struct Shuffle          { PlayerID deckOwner; };
    struct VerifyCommitment { size_t commitmentIndex; }; // TODO: omit index? commitments evaluated sequentially anyway
}

using ActionEntry = std::variant<
    Action::DealDamage,
    Action::GainLife,
    Action::DrawCards,
    Action::PlayCard,
    Action::Discard,
    Action::EndTurn,
    Action::Shuffle,
    Action::VerifyCommitment
>;
