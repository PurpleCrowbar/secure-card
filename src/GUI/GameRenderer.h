#pragma once

#include <SFML/Graphics.hpp>
#include <map>
#include "GameBridge.h"
#include "../Cards/CardID.h"

class GameRenderer {
public:
    explicit GameRenderer(GameBridge& bridge);

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

    std::map<CardID, sf::Texture> cardTextures;
    sf::Texture cardbackTexture;

    GameSnapshot latestSnapshot;

    // TODO: must not use hard-coded values; adapt to screen size
    static constexpr float CARD_WIDTH = 120.f;
    static constexpr float CARD_HEIGHT = 168.f;
    static constexpr float CARD_SPACING = 10.f;
    static constexpr unsigned int WINDOW_WIDTH = 1280;
    static constexpr unsigned int WINDOW_HEIGHT = 720;
};
