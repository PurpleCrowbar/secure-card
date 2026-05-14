#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <map>
#include <deque>
#include <memory>
#include "GameBridge.h"
#include "Animation.h"
#include "AnimationFactory.h"
#include "../Cards/CardID.h"

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

    // Static versions for use by AnimationFactory (no instance needed)
    static sf::FloatRect getOpponentCardBoundsStatic(int index, int handSize, float logicalWidth, float cardWidth, float cardHeight);
    static sf::FloatRect getPlayerCardBoundsStatic(int index, int handSize, float logicalWidth, float cardWidth, float cardHeight);

    GameBridge& bridge;
    sf::RenderWindow window;
    sf::Font font;
    sf::View gameView;

    std::map<CardID, sf::Texture> cardTextures;
    sf::Texture cardbackTexture;

    PlayerID localPlayer;
    GameSnapshot displaySnapshot; // the visual state rendered on screen (lags behind true state)
    sf::Vector2f mousePos;

    // Pending snapshot updates, each with events to animate before advancing to that snapshot
    std::deque<SnapshotUpdate> pendingUpdates;

    // Each step pairs an animation with the displaySnapshot to switch to when it begins playing.
    // This lets per-event snapshot fields (e.g. HP for damage, hand contents for discards) advance
    // in lockstep with their own animation rather than all at once at update start
    struct AnimationStep {
        GameSnapshot snapshotAtStart;
        std::unique_ptr<Animation> animation;
    };
    std::deque<AnimationStep> animationQueue;
    // Applied once the queue drains, to catch any fields not covered by per-event deltas
    // (turn flag, gameOver, statusMessage, winnerMessage, etc)
    GameSnapshot pendingFinalSnapshot;
    sf::Clock frameClock;

    void startNextUpdate();
    AnimationContext buildAnimationContext(const GameSnapshot& snapshot) const;

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
