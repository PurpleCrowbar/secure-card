#include "SpectralWail.h"
#include <iostream>
#include <ranges>

#include "../CardFactory.h"
#include "../../Commitments/CardsInHandCommitment.h"

void SpectralWail::resolve(Game& game, PlayerID controller) {
    // If local player is playing this card
    if (game.getLocalPlayer() == controller) {
        PlayerID opponent = PlayerIDUtils::getOpponent(controller);

        // Receives cryptographic commitment of opponent's hand
        std::vector<Point> ciphertexts = game.network.receivePoints();
        if (ciphertexts.size() != game.state.getOpponentHandSize(controller)) {
            throw std::runtime_error(std::format(
                "[SpectralWail::resolve] Error: received {} ciphertexts, expected {} (opponent's hand size)\n",
                ciphertexts.size(), game.state.getOpponentHandSize(controller)
            ));
        }
        game.verifier.logEnemyCommitment(std::make_unique<CardsInHandCommitment>(opponent, ciphertexts));

        // Send back the index of one of the ciphertexts, chosen at random
        uint8_t index = static_cast<uint8_t>(randombytes_uniform(static_cast<uint32_t>(ciphertexts.size())));
        game.network.sendUint8(index);

        // Receive the key for the randomly selected ciphertext
        // TODO: Possible optimisation can be made here: update the commitment with any keys received during the effect's resolution
        auto card = LookupTable::instance().getCardID(decrypt(ciphertexts.at(index), game.network.receiveScalar()));
        if (!card.has_value()) throw std::runtime_error("[SpectralWail::resolve] Error: received decryption key failed to decrypt ciphertext\n");

        // Remove that card from the opponent's hand and add it to their graveyard
        std::get<UnknownHand>(game.state.getPlayerData(opponent).hand).removeCard();
        game.state.getPlayerData(opponent).graveyard.push_back(card.value());

        std::cout << std::format("  [Spectral Wail] Opponent discarded {} at random!\n", CardFactory::create(card.value())->getName());
        game.verifier.logAction(Action::Discard(opponent, card.value()));
        return;
    }
    // If opponent is playing this card
    else {
        // encrypt hand contents with keys - the inverse of which are stored and then sent at end of game
        auto& hand = std::get<ClearHand>(game.state.getPlayerData(game.getLocalPlayer()).hand);
        std::vector<CardID> handContents = hand.ui_getHandContents();
        shuffleCards(handContents);
        auto encryptedHandContents = encryptCardsWithIndividualKeys(convertCardsToPoints_vec(handContents));

        // Send opponent the ciphertexts for all cards in our hand.
        // They will store these ciphertexts, then tell us which they choose to discard
        game.network.sendPoints(encryptedHandContents | std::views::transform(
            [](const auto& entry) -> const Point& { return entry.first; }
        ));
        game.verifier.logLocalCommitmentKeys(encryptedHandContents | std::views::transform(
            [](const auto& entry) -> const Scalar& { return entry.second.k_inv; }
        ));

        // Receive the card the opponent wishes us to discard
        auto indexToDiscard = game.network.receiveUint8();
        if (indexToDiscard >= handContents.size()) throw std::runtime_error(
            "[SpectralWail::resolve] Error: received invalid index for card in hand\n"
        );

        // Send the key for the card at the index
        game.network.sendScalar(encryptedHandContents[indexToDiscard].second.k_inv);

        // Remove that card from our hand and add it to our graveyard
        hand.removeCard(handContents[indexToDiscard]);
        game.state.getPlayerData(game.getLocalPlayer()).graveyard.push_back(handContents[indexToDiscard]);

        std::cout << std::format("  [Spectral Wail] You discarded {} at random!\n", CardFactory::create(handContents[indexToDiscard])->getName());
        game.verifier.logAction(Action::Discard(game.getLocalPlayer(), handContents[indexToDiscard]));
        return;
    }
}
