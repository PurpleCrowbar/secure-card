#pragma once

#include <memory>
#include <vector>
#include "GameState.h"
#include "../Network.h"
#include "../PlayerID.h"

class Card;

class Game {
public:
    Game(Network& network, PlayerID localPlayer, const std::vector<CardID>& localDeck);

    // Runs game lifecycle (initial shuffle, draw opening hands, main game loop)
    void run();

    // Mutative methods used by cards' resolve methods
    void dealDamage(PlayerID target, int amount);
    // Exchanges keys and draws cards. Both players call with the same arguments.
    // The drawing player receives keys; the other player sends keys.
    void drawCards(PlayerID player, uint8_t count);
    void gainLife(PlayerID target, int amount);
    // Runs the mental poker shuffle protocol for one player's deck, both
    // players call this with the same deckOwner argument
    std::vector<std::pair<Point, Scalar>> performShuffle(PlayerID deckOwner);

    // Getters
    [[nodiscard]] PlayerID getLocalPlayer() const { return localPlayer; }
    [[nodiscard]] int getHandSize(PlayerID player) const;
    [[nodiscard]] int getMana(PlayerID player) const;

private:

    // Called at the start of each turn; resets mana, draws a card
    void startTurn();
    // switches state.activePlayer to the other player
    void advanceTurn();

    void runMyTurn();
    void runOpponentTurn();

    void playCardLocal(int handIndex);
    void handleOpponentPlayCard();
    void printGameState() const;

    // need to defer initialisation of state because it requires decks (only acquired after shuffling)
    std::unique_ptr<GameState> state;
    Network& network;
    PlayerID localPlayer;
    // TODO: delete this. use Deck in GameState
    std::vector<CardID> deckList;  // our original plaintext deck
};
