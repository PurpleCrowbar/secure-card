#include "Animations.h"
#include <algorithm>
#include <cmath>

// helper shared with GameRenderer. TODO: move to Constants.h?
static constexpr float textOversample = 3.f;

static sf::Text makeAnimText(const sf::Font& font, const std::string& str, unsigned int logicalSize) {
    sf::Text text(font, str, static_cast<unsigned int>(logicalSize * textOversample));
    text.setScale({1.f / textOversample, 1.f / textOversample});
    return text;
}

static sf::Vector2f snapToPixel(const sf::RenderWindow& window, const sf::View& view, sf::Vector2f logicalPos) {
    auto pixel = window.mapCoordsToPixel(logicalPos, view);
    return window.mapPixelToCoords(pixel, view);
}

// --- CardSlideAnimation ---

CardSlideAnimation::CardSlideAnimation(CardID card, bool isDiscard, sf::Vector2f startPos, sf::Vector2f endPos,
                                       const sf::Texture& texture, float cardWidth, float cardHeight)
    : card(card), isDiscard(isDiscard), startPos(startPos), endPos(endPos),
      texture(texture), cardWidth(cardWidth), cardHeight(cardHeight) {}

bool CardSlideAnimation::update(float dt) {
    elapsed += dt;
    return elapsed >= totalDuration();
}

void CardSlideAnimation::render(sf::RenderWindow& window, const sf::View& gameView) {
    float t = elapsed;

    sf::Vector2f pos;
    uint8_t alpha = 255;
    if (t < SLIDE_DURATION) {
        float progress = t / SLIDE_DURATION;
        pos.x = startPos.x + (endPos.x - startPos.x) * progress;
        pos.y = startPos.y + (endPos.y - startPos.y) * progress;
    } else if (t < SLIDE_DURATION + HOLD_DURATION) {
        pos = endPos;
    } else {
        pos = endPos;
        float fadeProgress = (t - SLIDE_DURATION - HOLD_DURATION) / FADE_DURATION;
        alpha = static_cast<uint8_t>(255.f * (1.f - std::min(fadeProgress, 1.f)));
    }

    sf::Sprite sprite(texture);
    sprite.setPosition(snapToPixel(window, gameView, pos));
    sprite.setScale({
        cardWidth / static_cast<float>(texture.getSize().x),
        cardHeight / static_cast<float>(texture.getSize().y)
    });
    if (isDiscard) sprite.setColor(sf::Color(255, 80, 80, alpha));
    else sprite.setColor(sf::Color(255, 255, 255, alpha));
    window.draw(sprite);
}

// --- DamageNumberAnimation ---

DamageNumberAnimation::DamageNumberAnimation(int amount, sf::Vector2f position, const sf::Font& font)
    : amount(amount), position(position), font(font) {}

bool DamageNumberAnimation::update(float dt) {
    elapsed += dt;
    return elapsed >= DURATION;
}

void DamageNumberAnimation::render(sf::RenderWindow& window, const sf::View& gameView) {
    float progress = std::min(elapsed / DURATION, 1.f);
    uint8_t alpha = static_cast<uint8_t>(255.f * (1.f - progress));
    float yOffset = -DRIFT_DISTANCE * progress;

    auto text = makeAnimText(font, "-" + std::to_string(amount), 24);
    text.setFillColor(sf::Color(255, 60, 60, alpha));
    text.setPosition(snapToPixel(window, gameView, {position.x, position.y + yOffset}));
    window.draw(text);
}

// --- LifeGainAnimation ---

LifeGainAnimation::LifeGainAnimation(int amount, sf::Vector2f position, const sf::Font& font)
    : amount(amount), position(position), font(font) {}

bool LifeGainAnimation::update(float dt) {
    elapsed += dt;
    return elapsed >= DURATION;
}

void LifeGainAnimation::render(sf::RenderWindow& window, const sf::View& gameView) {
    float progress = std::min(elapsed / DURATION, 1.f);
    uint8_t alpha = static_cast<uint8_t>(255.f * (1.f - progress));
    float yOffset = -DRIFT_DISTANCE * progress;

    auto text = makeAnimText(font, "+" + std::to_string(amount), 24);
    text.setFillColor(sf::Color(60, 255, 60, alpha));
    text.setPosition(snapToPixel(window, gameView, {position.x, position.y + yOffset}));
    window.draw(text);
}

// --- MillAnimation ---

MillAnimation::MillAnimation(int count, sf::Vector2f position, const sf::Font& font)
    : count(count), position(position), font(font) {}

bool MillAnimation::update(float dt) {
    elapsed += dt;
    return elapsed >= DURATION;
}

void MillAnimation::render(sf::RenderWindow& window, const sf::View& gameView) {
    float progress = std::min(elapsed / DURATION, 1.f);
    uint8_t alpha = static_cast<uint8_t>(255.f * (1.f - progress));

    auto text = makeAnimText(font, "Milled " + std::to_string(count) + " cards", 18);
    text.setFillColor(sf::Color(180, 140, 255, alpha));
    text.setPosition(snapToPixel(window, gameView, position));
    window.draw(text);
}
