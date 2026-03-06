#pragma once

#include <memory>
#include <vector>
#include "GameState.h"
#include "../Network.h"
#include "../PlayerID.h"

class Card;

class Game {
public:
    Game(Network& network, PlayerID localPlayer, LookupTable& lookupTable, const std::vector<CardID>& localDeck);

    // Runs game lifecycle (initial shuffle, draw opening hands, main game loop)
    void run();

    // methods used by cards' resolve method
    void dealDamage(PlayerID target, int amount);
    void gainLife(PlayerID target, int amount);
    [[nodiscard]] PlayerID getLocalPlayer() const { return localPlayer; }
    [[nodiscard]] int getHandSize(PlayerID player) const;
    [[nodiscard]] int getMana(PlayerID player) const;

private:
    // Runs the mental poker shuffle protocol for one player's deck, both
    // players call this with the same deckOwner argument
    std::vector<std::pair<Point, Scalar>> performShuffle(PlayerID deckOwner);

    // Exchanges keys and draws cards. Both players call with the same arguments.
    // The drawing player receives keys; the other player sends keys.
    void drawCards(PlayerID player, int count);

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
    LookupTable& lookupTable;
    std::vector<CardID> deckList;  // our original plaintext deck
};
