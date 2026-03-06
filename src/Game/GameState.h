#pragma once
#include "../Cards/CardID.h"
#include "../Cards/Deck.h"
#include "../Cryptosystem.h"
#include "../PlayerID.h"
#include <array>
#include <optional>
#include <vector>

/**
 * Container of data pertaining to the game state. Used in both the Game class and the (unimplemented) Verifier class
 */
class GameState {
public:
    // Construct GameState with both players' encrypted decks
    // TODO: fix these parameter names -- they're not "local deck" and "opponent deck", they're "Player One" and "Player Two"
    GameState(const std::vector<std::pair<Point, Scalar>>& localDeck, const std::vector<std::pair<Point, Scalar>>& opponentDeck, LookupTable& lookupTable)
        : playerData { PlayerData(localDeck, lookupTable), PlayerData(opponentDeck, lookupTable) } {}
    // Construct GameState with only local deck, cleartext
    GameState(const std::vector<CardID>& localCleartextDeck, LookupTable& lookupTable)
        : playerData { PlayerData(localCleartextDeck, lookupTable), lookupTable } {}

    // Individual player data. The GameState two copies of these - one for each player
    struct PlayerData {
        PlayerData(LookupTable& lookupTable)
            : deck(lookupTable) {}
        PlayerData(const std::vector<std::pair<Point, Scalar>>& inpDeck, LookupTable& lookupTable)
            : deck(inpDeck, lookupTable) {}
        PlayerData(const std::vector<CardID>& inpDeck, LookupTable& lookupTable)
            : deck(inpDeck, lookupTable) {}

        Deck deck;
        // TODO: Could make a Hand class in the future for storing hand contents + visibility states
        // TODO: should also probably just be real card objects, not card IDs
        // unknown card in opponent's hand = nullopt. bool = opponent knows (only applies to cards in local player's hand)
        std::vector<std::optional<std::pair<CardID, bool>>> handContents = {};
        uint16_t maxHealth = 20, currentHealth = maxHealth;
        uint8_t maxMana = 5, currentMana = maxMana;
        uint8_t fatigueCount = 0;
        std::vector<CardID> graveyard = {};
    };

    /**
     * @return Whether the game is over and, if it is, who the winner was. Nullopt if it was a tie
     */
    std::pair<bool, std::optional<PlayerID>> isGameOver() const {
        bool gameOver = false;
        std::optional<PlayerID> winner;
        for (int i = 0; i < playerData.size(); ++i) {
            if (playerData[i].currentHealth > 0) continue;
            // This player has lost. If the other player also lost, it's a tie
            if (gameOver) winner.reset();
            else winner = static_cast<PlayerID>(i ^ 1);
            gameOver = true;
        }
        return {gameOver, winner};
    }

    PlayerData& getPlayerData(PlayerID player) {
        return playerData[static_cast<std::underlying_type_t<PlayerID>>(player)];
    }

    const PlayerData& getImmutablePlayerData(PlayerID player) const {
        return playerData[static_cast<std::underlying_type_t<PlayerID>>(player)];
    }

    PlayerData& getOpponentPlayerData(PlayerID localPlayer) {
        return playerData[static_cast<std::underlying_type_t<PlayerID>>(PlayerIDUtils::getOpponent(localPlayer))];
    }

    bool isMyTurn(PlayerID localPlayer) const {
        return activePlayer.value() == localPlayer;
    }

    // index 0 = Player One, index 1 = Player Two
    std::array<PlayerData, 2> playerData;
    // the player whose turn it is. **must be set after coin toss deciding who goes first**.
    std::optional<PlayerID> activePlayer = std::nullopt;
};
