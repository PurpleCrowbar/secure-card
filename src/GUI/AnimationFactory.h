#pragma once
#include "Animation.h"
#include "GameEvent.h"
#include "GameBridge.h"
#include "../Cards/CardID.h"
#include <SFML/Graphics.hpp>
#include <map>
#include <memory>

// Layout info needed by the factory to position animations correctly
struct AnimationContext {
    const std::map<CardID, sf::Texture>& cardTextures;
    const sf::Texture& cardbackTexture;
    const sf::Font& font;
    PlayerID localPlayer; // needed to map PlayerID to screen position (top vs bottom)
    int oppHandSize; // current opponent hand size from the snapshot
    int myHandSize; // local player's hand size from the snapshot
    float cardWidth;
    float cardHeight;
    float logicalWidth;
    float logicalHeight;

    // callback to compute card bounds (index, handSize) -> FloatRect
    using BoundsFunc = sf::FloatRect(*)(int index, int handSize, float logicalWidth, float cardWidth, float cardHeight);
    BoundsFunc getOpponentCardBounds;
    BoundsFunc getPlayerCardBounds;
};

std::unique_ptr<Animation> createAnimation(const GameEvent& event, const AnimationContext& ctx);
