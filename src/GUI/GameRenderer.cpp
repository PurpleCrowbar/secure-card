#include "GameRenderer.h"
#include "../Cards/CardFactory.h"
#include <iostream>
#include <ranges>
#include <algorithm>
#include <sodium/randombytes.h>

GameRenderer::GameRenderer(GameBridge& bridge, PlayerID player)
    : bridge(bridge),
      window(sf::VideoMode({1280u, 720u}),
          std::string("Secure Card - Player ") + (player == PlayerID::ONE ? "One (Host)" : "Two (Client)")),
      font("resources/font.ttf"),
      gameView(sf::FloatRect({0.f, 0.f}, {LOGICAL_WIDTH, LOGICAL_HEIGHT}))
{
    window.setFramerateLimit(60);
    window.setView(gameView);
    loadTextures();
}

[[nodiscard]] std::string getCardImagePath(CardID id) {
    assert(id != CardID::End && "getCardImagePath called with CardID::End");

    // TODO: Heap-instantiating cards every time we need to access their name is not efficient, but I don't know
    // that there's a better way to do it. Constexpr card factory thing? Weird, and not pressing
    std::string cardName = CardFactory::create(id)->getName();
    std::erase(cardName, ' '); // remove the spaces from the card's name
    return "resources/" + cardName + ".png";
}

void GameRenderer::loadTextures() {
    for (int i = 0; i < static_cast<int>(CardID::End); i++) {
        CardID id = static_cast<CardID>(i);
        cardTextures[id] = sf::Texture(getCardImagePath(id));
        cardTextures[id].setSmooth(true);
    }
    cardbackTexture = sf::Texture("resources/cardback.png");
    cardbackTexture.setSmooth(true);
}


constexpr float textOversample = 3.f;

sf::Text makeText(const sf::Font& font, const std::string& str, unsigned int logicalSize) {
    sf::Text text(font, str, static_cast<unsigned int>(logicalSize * textOversample));
    text.setScale({1.f / textOversample, 1.f / textOversample});
    return text;
}

sf::Vector2f snapToPixel(const sf::RenderWindow& window, const sf::View& view, sf::Vector2f logicalPos) {
    auto pixel = window.mapCoordsToPixel(logicalPos, view);
    return window.mapPixelToCoords(pixel, view);
}

/**
 * <b>Runs on main thread.</b> Returns either when the window is closed by the user or the game ends.
 */
void GameRenderer::run() {
    while (window.isOpen()) {
        float dt = frameClock.restart().asSeconds();

        while (auto event = window.pollEvent()) {
            handleEvent(*event);
        }

        mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window), gameView);
        latestSnapshot = bridge.getSnapshot();

        // Start animation if opponent did something and we're not already animating
        if (latestSnapshot.oppCardEvent.has_value() && !activeAnimation.has_value()) {
            startAnimation(latestSnapshot.oppCardEvent.value(), latestSnapshot);
        }

        // Advance animation
        if (activeAnimation.has_value()) {
            activeAnimation->elapsed += dt;
            if (activeAnimation->isComplete()) activeAnimation.reset();
        }

        render(latestSnapshot);
    }

    bridge.requestQuit();
}

void GameRenderer::handleEvent(const sf::Event& event) {
    if (event.is<sf::Event::Closed>()) {
        window.close();
        return;
    }

    if (const auto resized = event.getIf<sf::Event::Resized>()) {
        updateViewport(resized->size);
        return;
    }

    if (const auto mouseEvent = event.getIf<sf::Event::MouseButtonPressed>()) {
        if (mouseEvent->button != sf::Mouse::Button::Left) return;

        auto mousePos = window.mapPixelToCoords(mouseEvent->position);
        if (!latestSnapshot.isMyTurn) return; // maybe more here at some point, but for now just end turn / click card

        // check End Turn button
        if (isEndTurnButtonClicked(mousePos)) {
            bridge.submitInput(0);
            return;
        }

        // check card clicks
        int cardIndex = getClickedCardIndex(mousePos, latestSnapshot);
        if (cardIndex > 0) {
            bridge.submitInput(cardIndex);
        }
    }
}

/**
 * Renders the GUI. Currently uses a lot of magic numbers which must be fixed.
 * @param snapshot The snapshot, based on which the GUI will be rendered
 */
void GameRenderer::render(const GameSnapshot& snapshot) {
    window.clear(sf::Color::Black);
    window.setView(gameView);

    // draw game background within the logical viewport
    sf::RectangleShape bg({LOGICAL_WIDTH, LOGICAL_HEIGHT});
    bg.setFillColor(sf::Color(30, 30, 40));
    window.draw(bg);

    // opponent stats (top-left)
    {
        auto text = makeText(font, "Opponent", 18);
        text.setFillColor(sf::Color(200, 80, 80));
        text.setPosition({20.f, 15.f});
        window.draw(text);

        auto stats = makeText(font,
            "HP: " + std::to_string(snapshot.oppHealth) +
            "  |  Mana: " + std::to_string(snapshot.oppMana) +
            "  |  Deck: " + std::to_string(snapshot.oppDeckSize), 16);
        stats.setFillColor(sf::Color::White);
        stats.setPosition({20.f, 40.f});
        window.draw(stats);
    }

    // opponent's hand (top, face-down)
    for (int i = 0; i < snapshot.oppHandSize; i++) {
        auto bounds = getOpponentCardBounds(i, snapshot.oppHandSize);
        sf::Sprite sprite(cardbackTexture);
        sprite.setPosition(snapToPixel(window, gameView, bounds.position));
        sprite.setScale({
            bounds.size.x / static_cast<float>(cardbackTexture.getSize().x),
            bounds.size.y / static_cast<float>(cardbackTexture.getSize().y)
        });
        window.draw(sprite);
    }

    // local player's stats (bottom-left, above hand)
    {
        auto text = makeText(font, "You", 18);
        text.setFillColor(sf::Color(80, 200, 80));
        text.setPosition({20.f, LOGICAL_HEIGHT - 250.f});
        window.draw(text);

        auto stats = makeText(font,
            "HP: " + std::to_string(snapshot.myHealth) +
            "  |  Mana: " + std::to_string(snapshot.myMana) +
            "  |  Deck: " + std::to_string(snapshot.myDeckSize), 16);
        stats.setFillColor(sf::Color::White);
        stats.setPosition({20.f, LOGICAL_HEIGHT - 225.f});
        window.draw(stats);
    }

    // local player's hand (bottom, face-up)
    int handSize = static_cast<int>(snapshot.myHand.size());
    for (int i = 0; i < handSize; i++) {
        CardID cardId = snapshot.myHand[i];
        auto bounds = getPlayerCardBounds(i, handSize);

        auto it = cardTextures.find(cardId);
        if (it == cardTextures.end()) continue; // TODO: should maybe throw or use some placeholder for missing textures

        sf::Sprite sprite(it->second);
        sprite.setPosition(snapToPixel(window, gameView, bounds.position));
        sprite.setScale({
            bounds.size.x / static_cast<float>(it->second.getSize().x),
            bounds.size.y / static_cast<float>(it->second.getSize().y)
        });

        auto card = CardFactory::create(cardId);
        bool affordable = snapshot.isMyTurn && card->getManaCost() <= snapshot.myMana;

        // grey out any cards we can't afford
        if (!affordable) {
            sprite.setColor(sf::Color(128, 128, 128));
        }

        window.draw(sprite);

        // because the sprite's colour is 255,255,255 by default, it's impossible to make it lighter. the workaround
        // I'm using here is just to render a transparent white rectangle over the card if it's being moused over
        if (affordable && bounds.contains(mousePos)) {
            sf::RectangleShape highlight(bounds.size);
            highlight.setPosition(sprite.getPosition());
            highlight.setFillColor(sf::Color(255, 255, 255, 60));
            window.draw(highlight);
        }

        // draw mana cost label below card (though the card image texture currently also shows this)
        auto costText = makeText(font, std::to_string(card->getManaCost()), 14);
        costText.setFillColor(sf::Color(100, 150, 255));
        costText.setPosition({bounds.position.x + bounds.size.x / 2.f - 4.f, bounds.position.y + bounds.size.y + 2.f});
        window.draw(costText);
    }

    // End Turn button
    if (snapshot.isMyTurn && !snapshot.gameOver) {
        auto btnBounds = getEndTurnButtonBounds();
        bool btnHovered = btnBounds.contains(mousePos);

        sf::RectangleShape btn({btnBounds.size.x, btnBounds.size.y});
        btn.setPosition({btnBounds.position.x, btnBounds.position.y});
        btn.setFillColor(btnHovered ? sf::Color(80, 160, 80) : sf::Color(60, 120, 60));
        btn.setOutlineColor(sf::Color::White);
        btn.setOutlineThickness(2.f);
        window.draw(btn);

        auto btnText = makeText(font, "End Turn", 18);
        btnText.setFillColor(sf::Color::White);
        auto textBounds = btnText.getLocalBounds();
        btnText.setPosition({
            btnBounds.position.x + (btnBounds.size.x - textBounds.size.x / textOversample) / 2.f,
            btnBounds.position.y + (btnBounds.size.y - textBounds.size.y / textOversample) / 2.f - textBounds.position.y / textOversample
        });
        window.draw(btnText);
    }

    // turn indicator
    {
        std::string turnText = snapshot.isMyTurn ? "Your Turn" : "Opponent's Turn";
        auto text = makeText(font, turnText, 22);
        text.setFillColor(snapshot.isMyTurn ? sf::Color(80, 255, 80) : sf::Color(255, 80, 80));
        text.setPosition({LOGICAL_WIDTH / 2.f - 60.f, LOGICAL_HEIGHT / 2.f - 50.f});
        window.draw(text);
    }

    // status message
    if (!snapshot.statusMessage.empty()) {
        auto text = makeText(font, snapshot.statusMessage, 16);
        text.setFillColor(sf::Color(220, 220, 180));
        text.setPosition({LOGICAL_WIDTH / 2.f - 120.f, LOGICAL_HEIGHT / 2.f - 15.f});
        window.draw(text);
    }

    // opponent card animation
    if (activeAnimation.has_value()) {
        const auto& anim = activeAnimation.value();
        float t = anim.elapsed;

        // compute position (how far from hand) and alpha based on animation phase
        sf::Vector2f pos;
        uint8_t alpha = 255;
        if (t < CardAnimation::SLIDE_DURATION) {
            // slide phase: so lerp from start to end
            float progress = t / CardAnimation::SLIDE_DURATION;
            pos.x = anim.startPos.x + (anim.endPos.x - anim.startPos.x) * progress;
            pos.y = anim.startPos.y + (anim.endPos.y - anim.startPos.y) * progress;
        } else if (t < CardAnimation::SLIDE_DURATION + CardAnimation::HOLD_DURATION) {
            // hold phase: stay at end position
            pos = anim.endPos;
        } else {
            // fade phase: stay at end, reduce alpha
            pos = anim.endPos;
            float fadeProgress = (t - CardAnimation::SLIDE_DURATION - CardAnimation::HOLD_DURATION) / CardAnimation::FADE_DURATION;
            alpha = static_cast<uint8_t>(255.f * (1.f - std::min(fadeProgress, 1.f)));
        }

        auto it = cardTextures.find(anim.card);
        if (it != cardTextures.end()) {
            sf::Sprite sprite(it->second);
            sprite.setPosition(snapToPixel(window, gameView, pos));
            sprite.setScale({
                CARD_WIDTH / static_cast<float>(it->second.getSize().x),
                CARD_HEIGHT / static_cast<float>(it->second.getSize().y)
            });
            if (anim.isDiscard) sprite.setColor(sf::Color(255, 80, 80, alpha));
            else sprite.setColor(sf::Color(255, 255, 255, alpha));
            window.draw(sprite);
        }
    }

    // game over
    if (snapshot.gameOver && snapshot.winnerMessage.has_value()) {
        // dim the entire physical window (including letterbox bars) using the default view
        window.setView(window.getDefaultView());
        auto winSize = window.getSize();
        sf::RectangleShape overlay({static_cast<float>(winSize.x), static_cast<float>(winSize.y)});
        overlay.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(overlay);

        // switch back to game view for text positioning
        window.setView(gameView);
        auto text = makeText(font, snapshot.winnerMessage.value(), 36);
        text.setFillColor(sf::Color::White);
        text.setPosition({LOGICAL_WIDTH / 2.f - 100.f, LOGICAL_HEIGHT / 2.f - 20.f});
        window.draw(text);
    }

    window.display();
}

void GameRenderer::startAnimation(const OpponentCardEvent& event, const GameSnapshot& snapshot) {
    // NOTE: have to use oppHandSize + 1 because snapshot already reflects the decremented hand count
    int preActionHandSize = std::max(1, snapshot.oppHandSize + 1);
    int slotIndex = static_cast<int>(randombytes_uniform(static_cast<uint32_t>(preActionHandSize)));
    auto startBounds = getOpponentCardBounds(slotIndex, preActionHandSize);

    activeAnimation = CardAnimation {
        .card = event.card,
        .isDiscard = (event.type == OpponentCardEventType::DISCARD),
        .startPos = startBounds.position,
        .endPos = {LOGICAL_WIDTH / 2.f - CARD_WIDTH / 2.f, LOGICAL_HEIGHT / 2.f - CARD_HEIGHT / 2.f},
    };
}

void GameRenderer::updateViewport(sf::Vector2u windowSize) {
    float windowRatio = static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);
    float viewRatio = LOGICAL_WIDTH / LOGICAL_HEIGHT;

    float vpWidth = 1.f, vpHeight = 1.f;
    float vpX = 0.f, vpY = 0.f;

    if (windowRatio > viewRatio) {
        // window wider than 16:9, need to use pillarbox
        vpWidth = viewRatio / windowRatio;
        vpX = (1.f - vpWidth) / 2.f;
    } else {
        // window taller than 16:9, so need to use letterbox
        vpHeight = windowRatio / viewRatio;
        vpY = (1.f - vpHeight) / 2.f;
    }

    gameView.setViewport(sf::FloatRect({vpX, vpY}, {vpWidth, vpHeight}));
    window.setView(gameView);
}

/**
 * Gets the card bounds of a card in the local player's hand. Used to render the card and check if it was clicked.
 * @param index Index of card
 * @param handSize Number of cards in hand
 * @return FloatRect bounds for the card
 */
sf::FloatRect GameRenderer::getPlayerCardBounds(int index, int handSize) const {
    float totalWidth = handSize * CARD_WIDTH + (handSize - 1) * CARD_SPACING;
    float startX = (LOGICAL_WIDTH - totalWidth) / 2.f;
    float x = startX + index * (CARD_WIDTH + CARD_SPACING);
    float y = LOGICAL_HEIGHT - CARD_HEIGHT - 30.f;
    return sf::FloatRect({x, y}, {CARD_WIDTH, CARD_HEIGHT});
}

sf::FloatRect GameRenderer::getOpponentCardBounds(int index, int handSize) const {
    float totalWidth = handSize * CARD_WIDTH + (handSize - 1) * CARD_SPACING;
    float startX = (LOGICAL_WIDTH - totalWidth) / 2.f;
    float x = startX + index * (CARD_WIDTH + CARD_SPACING);
    float y = 70.f;
    return sf::FloatRect({x, y}, {CARD_WIDTH, CARD_HEIGHT});
}

sf::FloatRect GameRenderer::getEndTurnButtonBounds() const {
    return sf::FloatRect({LOGICAL_WIDTH - 160.f, LOGICAL_HEIGHT / 2.f - 25.f}, {130.f, 45.f});
}

/**
 * @return 1-based hand index of clicked card. 0 = none
 */
int GameRenderer::getClickedCardIndex(sf::Vector2f mousePos, const GameSnapshot& snapshot) const {
    int handSize = static_cast<int>(snapshot.myHand.size());
    // check in reverse order so topmost (rightmost overlapping) card wins
    for (int i = handSize - 1; i >= 0; i--) {
        auto bounds = getPlayerCardBounds(i, handSize);
        if (bounds.contains(mousePos)) {
            return i + 1; // 1-based
        }
    }
    return 0;
}

bool GameRenderer::isEndTurnButtonClicked(sf::Vector2f mousePos) const {
    return getEndTurnButtonBounds().contains(mousePos);
}
