#pragma once
#include <array>
#include <optional>
#include <vector>
#include "../Cards/CardID.h"
#include "../Cards/Deck.h"
#include "../Cryptosystem.h"
#include "../PlayerID.h"

// TODO: Add source file to this class; it's outgrown this header
/**
 * Container of data pertaining to the game state. Used in both the Game class and the (unimplemented) Verifier class
 */
class GameState {
public:
    // Construct GameState with both players' encrypted decks
    GameState(const std::vector<std::pair<Point, Scalar>>& p1Deck, const std::vector<std::pair<Point, Scalar>>& p2Deck)
        : playerData { PlayerData(p1Deck), PlayerData(p2Deck) } {}
    // Construct GameState with cleartext local deck and empty remote deck
    GameState(const std::vector<CardID>& localCleartextDeck, PlayerID localPlayer)
        : playerData {
            localPlayer == PlayerID::ONE ? PlayerData(localCleartextDeck) : PlayerData(),
            localPlayer == PlayerID::TWO ? PlayerData(localCleartextDeck) : PlayerData(),
        } {}

    // Individual player data. The GameState possesses a PlayerData for each of the two players
    struct PlayerData {
        PlayerData() = default;
        // NOTE: This specific constructor currently unused
        PlayerData(const std::vector<std::pair<Point, Scalar>>& encryptedDeck)
            : deck(encryptedDeck) {}
        PlayerData(const std::vector<CardID>& plaintextDeck)
            : deck(plaintextDeck) {}

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
    // TODO: move to constructor? game state needs an active player so this should ideally not be optional.
    // TODO: could coin toss, *then* make gamestate w/ this initialised via GameState constructor
    std::optional<PlayerID> activePlayer = std::nullopt;
};
