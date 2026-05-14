#pragma once
#include <SFML/Graphics.hpp>

// interfce from which all animation classes derive
class Animation {
public:
    virtual ~Animation() = default;

    // Advance time by dt seconds, returns true when the animation is complete
    virtual bool update(float dt) = 0;

    // Render the animation's visuals onto the window
    virtual void render(sf::RenderWindow& window, const sf::View& gameView) = 0;
};
