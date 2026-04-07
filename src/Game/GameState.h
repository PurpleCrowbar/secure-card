#pragma once
#include <array>
#include <optional>
#include <vector>
#include "../Cards/CardID.h"
#include "../Cards/Deck.h"
#include "../PlayerID.h"
#include "../Hand/ClearHand.h"
#include "../Hand/UnknownHand.h"

// TODO: Add source file to this class; it's outgrown this header
/**
 * Container of data pertaining to the game state. Used in both the Game class and the (unimplemented) Verifier class
 */
class GameState {
public:
    GameState(const std::map<CardID, uint8_t>& localPlaintextDeckContents, PlayerID localPlayer)
        : playerData {
            localPlayer == PlayerID::ONE
            ? PlayerData(localPlaintextDeckContents, true) : PlayerData(false),
            localPlayer == PlayerID::TWO
            ? PlayerData(localPlaintextDeckContents, true) : PlayerData(false),
        } {}

    // Individual player data. The GameState possesses a PlayerData for each of the two players
    struct PlayerData {
        PlayerData(bool isLocalPlayer) : hand(isLocalPlayer
                ? std::variant<ClearHand, UnknownHand>(ClearHand{})
                : std::variant<ClearHand, UnknownHand>(UnknownHand{})
            ) {}

        PlayerData(const std::map<CardID, uint8_t>& plaintextDeckContents, bool isLocalPlayer)
            : deck(plaintextDeckContents), hand(isLocalPlayer
                ? std::variant<ClearHand, UnknownHand>(ClearHand{})
                : std::variant<ClearHand, UnknownHand>(UnknownHand{})
            ) {}

        Deck deck;
        std::variant<ClearHand, UnknownHand> hand;
        int16_t maxHealth = 10, currentHealth = maxHealth;
        uint8_t maxMana = 3, currentMana = maxMana;
        uint8_t fatigueCount = 0;
        std::vector<CardID> graveyard = {};
    };

    /**
     * @return Whether the game is over and, if it is, who the winner was. Nullopt if it was a tie
     */
    [[nodiscard]] std::pair<bool, std::optional<PlayerID>> isGameOver() const {
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

    [[nodiscard]] PlayerData& getPlayerData(PlayerID player) {
        return playerData[static_cast<std::underlying_type_t<PlayerID>>(player)];
    }

    [[nodiscard]] const PlayerData& getImmutablePlayerData(PlayerID player) const {
        return playerData[static_cast<std::underlying_type_t<PlayerID>>(player)];
    }

    [[nodiscard]] PlayerData& getOpponentPlayerData(PlayerID localPlayer) {
        return playerData[static_cast<std::underlying_type_t<PlayerID>>(PlayerIDUtils::getOpponent(localPlayer))];
    }

    [[nodiscard]] bool isMyTurn(PlayerID localPlayer) const {
        return activePlayer.value() == localPlayer;
    }

    [[nodiscard]] size_t getHandSize(PlayerID player) const {
        auto data = playerData[static_cast<std::underlying_type_t<PlayerID>>(player)];
        if (const auto hand = std::get_if<ClearHand>(&data.hand)) return hand->getSize();
        if (const auto hand = std::get_if<UnknownHand>(&data.hand)) return hand->getSize();

        // This never returns; data.hand is always either a ClearHand or an UnknownHand.
        return 0;
    }

    [[nodiscard]] size_t getOpponentHandSize(PlayerID player) const {
        return getHandSize(PlayerIDUtils::getOpponent(player));
    }

    // index 0 = Player One, index 1 = Player Two
    std::array<PlayerData, 2> playerData;
    // the player whose turn it is. **must be set after coin toss deciding who goes first**.
    // TODO: move to constructor? game state needs an active player so this should ideally not be optional.
    // TODO: could coin toss, *then* make gamestate w/ this initialised via GameState constructor
    std::optional<PlayerID> activePlayer = std::nullopt;
};
