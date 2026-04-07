#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <map>
#include <optional>
#include "GameBridge.h"
#include "../Cards/CardID.h"

struct CardAnimation {
    CardID card;
    bool isDiscard;
    sf::Vector2f startPos;
    sf::Vector2f endPos;
    float elapsed = 0.f;

    static constexpr float SLIDE_DURATION = 0.5f;
    static constexpr float HOLD_DURATION = 0.5f;
    static constexpr float FADE_DURATION = 0.4f;

    [[nodiscard]] float totalDuration() const { return SLIDE_DURATION + HOLD_DURATION + FADE_DURATION; }
    [[nodiscard]] bool isComplete() const { return elapsed >= totalDuration(); }
};

enum class PlayerID : std::uint8_t;

class GameRenderer {
public:
    explicit GameRenderer(GameBridge& bridge, PlayerID player);

    void run();

private:
    void loadTextures();
    void handleEvent(const sf::Event& event);
    void render(const GameSnapshot& snapshot);

    [[nodiscard]] int getClickedCardIndex(sf::Vector2f mousePos, const GameSnapshot& snapshot) const;
    [[nodiscard]] bool isEndTurnButtonClicked(sf::Vector2f mousePos) const;

    // Compute the bounding rect for a card at the given hand index (player's hand)
    [[nodiscard]] sf::FloatRect getPlayerCardBounds(int index, int handSize) const;
    [[nodiscard]] sf::FloatRect getOpponentCardBounds(int index, int handSize) const;
    [[nodiscard]] sf::FloatRect getEndTurnButtonBounds() const;

    GameBridge& bridge;
    sf::RenderWindow window;
    sf::Font font;
    sf::View gameView;

    std::map<CardID, sf::Texture> cardTextures;
    sf::Texture cardbackTexture;

    GameSnapshot latestSnapshot;

    std::optional<CardAnimation> activeAnimation;
    sf::Clock frameClock;
    void startAnimation(const OpponentCardEvent& event, const GameSnapshot& snapshot);

    void updateViewport(sf::Vector2u windowSize);

    // these values (width and height) maintain the aspect ratio of the real card images (785x1100 = 157:220).
    // they don't nececssarily have to be hard-coded though; could be calculated at game time in case I
    // ever change the images? low priority at any rate
    static constexpr float CARD_WIDTH = 157.f;
    static constexpr float CARD_HEIGHT = 220.f;
    static constexpr float CARD_SPACING = 10.f;
    // i.e., width / height of window within letterboxed view
    static constexpr float LOGICAL_WIDTH = 1280.f;
    static constexpr float LOGICAL_HEIGHT = 720.f;
};
