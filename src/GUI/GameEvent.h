#pragma once
#include "../Cards/CardID.h"
#include "../PlayerID.h"
#include <variant>

struct CardPlayedEvent {
    CardID card;
    PlayerID player;
};

struct CardDiscardedEvent {
    CardID card;
    PlayerID player;
};

struct DamageDealtEvent {
    PlayerID target;
    int amount;
};

struct LifeGainedEvent {
    PlayerID target;
    int amount;
};

struct CardsMilledEvent {
    PlayerID player;
    int count;
};

using GameEvent = std::variant<
    CardPlayedEvent,
    CardDiscardedEvent,
    DamageDealtEvent,
    LifeGainedEvent,
    CardsMilledEvent
>;
