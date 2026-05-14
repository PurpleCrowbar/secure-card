#pragma once
#include "Animation.h"
#include "../Cards/CardID.h"
#include "../PlayerID.h"
#include <SFML/Graphics.hpp>

// TODO: Could split this into multiple files. For simplicity, currently keeping as a single header

// Card slides from a start position to center, holds, then fades out; used for both card plays (white) and discards (red tint)
class CardSlideAnimation : public Animation {
public:
    CardSlideAnimation(CardID card, bool isDiscard, sf::Vector2f startPos, sf::Vector2f endPos, const sf::Texture& texture, float cardWidth, float cardHeight);

    bool update(float dt) override;
    void render(sf::RenderWindow& window, const sf::View& gameView) override;

private:
    CardID card;
    bool isDiscard;
    sf::Vector2f startPos;
    sf::Vector2f endPos;
    const sf::Texture& texture;
    float cardWidth;
    float cardHeight;
    float elapsed = 0.f;

    static constexpr float SLIDE_DURATION = 0.5f;
    static constexpr float HOLD_DURATION = 0.5f;
    static constexpr float FADE_DURATION = 0.4f;

    [[nodiscard]] float totalDuration() const { return SLIDE_DURATION + HOLD_DURATION + FADE_DURATION; }
};

// Floating damage number that drifts upward and fades out
class DamageNumberAnimation : public Animation {
public:
    DamageNumberAnimation(int amount, sf::Vector2f position, const sf::Font& font);

    bool update(float dt) override;
    void render(sf::RenderWindow& window, const sf::View& gameView) override;

private:
    int amount;
    sf::Vector2f position;
    const sf::Font& font;
    float elapsed = 0.f;

    static constexpr float DURATION = 0.8f;
    static constexpr float DRIFT_DISTANCE = 40.f;
};

// Floating life gain number (green) that drifts upward and fades out
class LifeGainAnimation : public Animation {
public:
    LifeGainAnimation(int amount, sf::Vector2f position, const sf::Font& font);

    bool update(float dt) override;
    void render(sf::RenderWindow& window, const sf::View& gameView) override;

private:
    int amount;
    sf::Vector2f position;
    const sf::Font& font;
    float elapsed = 0.f;

    static constexpr float DURATION = 0.8f;
    static constexpr float DRIFT_DISTANCE = 40.f;
};

// Brief "Milled N cards" text that fades near the deck area
class MillAnimation : public Animation {
public:
    MillAnimation(int count, sf::Vector2f position, const sf::Font& font);

    bool update(float dt) override;
    void render(sf::RenderWindow& window, const sf::View& gameView) override;

private:
    int count;
    sf::Vector2f position;
    const sf::Font& font;
    float elapsed = 0.f;

    static constexpr float DURATION = 1.0f;
};
