#pragma once

#include <memory>
#include <vector>
#include "GameState.h"
#include "../Network.h"
#include "../PlayerID.h"
#include "../Verification/GameVerifier.h"

class Card;

class Game {
public:
    Game(Network& network, PlayerID localPlayer, const std::map<CardID, uint8_t>& localDeckContents);

    // Runs game lifecycle (initial shuffle, draw opening hands, main game loop)
    void run();

    // Game actions. Mutative methods used by cards' resolve methods
    void dealDamage(PlayerID target, int amount);
    void gainLife(PlayerID target, int amount);
    // Exchanges keys and draws cards. Both players call with the same arguments.
    // The drawing player receives keys; the other player sends keys.
    void drawCards(PlayerID player, uint8_t count);
    // Runs the mental poker shuffle protocol for one player's deck, both
    // players call this with the same deckOwner argument
    std::vector<std::pair<Point, Scalar>> performShuffle(PlayerID deckOwner);

    // Getters
    [[nodiscard]] PlayerID getLocalPlayer() const { return localPlayer; }
    [[nodiscard]] int getMana(PlayerID player) const;

private:
    // Pre-game: both players commit to their deck contents via keyed hash
    void exchangeDeckCommitments();
    // Called at the start of each turn; resets mana, draws a card
    void startTurn();
    // switches state.activePlayer to the other player
    void advanceTurn();

    void runMyTurn();
    void runOpponentTurn();

    void playCardLocal(int handIndex);
    void handleOpponentPlayCard();
    void printGameState() const;

    // post-game: exchange all secret data and run the verifier
    void postGameExchangeAndVerify();

    GameState state;
    Network& network;
    PlayerID localPlayer;
    GameVerifier verifier;
    // Local player's deck's contents from the start of the game. Sent post-game for verification
    const std::map<CardID, uint8_t> localDeckContents;
};
