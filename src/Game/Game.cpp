#include "Game.h"
#include <iostream>
#include <algorithm>
#include "../Cards/CardFactory.h"
#include "../DeckCommitment.h"

Game::Game(Network& network, PlayerID localPlayer, const std::map<CardID, uint8_t>& localDeckContents)
    : state(localDeckContents, localPlayer), network(network),
    localPlayer(localPlayer), verifier(localDeckContents, localPlayer),
    localDeckContents(localDeckContents) {}

void Game::run() {
    exchangeDeckCommitments();

    std::cout << "\n=== Shuffling decks ===\n";
    state.playerData[0].deck.setEncryptedContents(performShuffle(PlayerID::ONE));
    state.playerData[1].deck.setEncryptedContents(performShuffle(PlayerID::TWO));

    std::cout << "\n=== Drawing opening hands ===\n";
    drawCards(PlayerID::ONE, 4);
    drawCards(PlayerID::TWO, 4);

    // TODO: coin flip here determines who goes first
    state.activePlayer = PlayerID::ONE;
    verifier.setPlayerGoingFirst(state.activePlayer.value());
    std::cout << "\n=== Game starting! ===\n";

    // First turn doesn't draw (like MtG: player going first skips first draw)
    bool firstTurn = true;

    // main gameplay loop
    while (!state.isGameOver().first) {
        if (!firstTurn) {
            startTurn();
        }
        firstTurn = false;

        if (state.activePlayer.value() == localPlayer) {
            runMyTurn();
        } else {
            runOpponentTurn();
        }

        if (state.isGameOver().first) break;
    }

    auto [gameOver, winner] = state.isGameOver();
    std::cout << "\n=== GAME OVER ===\n";
    if (!winner.has_value()) {
        std::cout << "It's a tie!\n";
    } else if (winner.value() == localPlayer) {
        std::cout << "You win!\n";
    } else {
        std::cout << "You lose!\n";
    }

    postGameExchangeAndVerify();
}

/**
 * Both players generate a random key, hash their deck contents with it, and exchange those hashes.
 * The key is then sent at the end of the game alongside the deck's plaintext contents for verification
 */
void Game::exchangeDeckCommitments() {
    std::cout << "\n=== Exchanging deck commitments ===\n";
    auto key = generateDeckHashKey();
    auto hash = computeDeckHash(key, localDeckContents);
    verifier.setLocalDeckCommitmentKey(key);

    if (localPlayer == PlayerID::ONE) {
        network.sendDeckHash(hash);
        DeckHash remoteHash = network.receiveDeckHash();
        verifier.setRemoteDeckCommitment(remoteHash);
    } else {
        DeckHash remoteHash = network.receiveDeckHash();
        verifier.setRemoteDeckCommitment(remoteHash);
        network.sendDeckHash(hash);
    }
    std::cout << "  [Commit] Deck commitments exchanged.\n";
}

/**
 * Post-game key dump and verification. Both players reveal all secret data from during the game:
 *   1. Deck commitment key + plaintext deck contents
 *   2. Shuffle seeds (for both decks)
 *   3. Mid-game commitment keys
 * Data is exchanged turn-about to prevent one player from receiving all of the opponent's data and disconnecting
 */
void Game::postGameExchangeAndVerify() {
    std::cout << "\n=== Post-game verification exchange ===\n";

    // Data we'll send
    auto seedsForOurDeck = verifier.getLocalShuffleSeedsForOurDeck();
    const DeckHashKey& deckKey = verifier.getLocalDeckCommitmentKey();
    auto seedsForOpponentDeck = verifier.getLocalShuffleSeedsForOpponentsDeck();
    const auto& commitmentKeys = verifier.getLocalCommitmentKeys();

    // Containers for data we receive
    std::vector<ShuffleSeed> remoteSeedsForTheirDeck;
    DeckHashKey remoteDeckKey;
    std::map<CardID, uint8_t> remoteDeckContents;
    std::vector<ShuffleSeed> remoteSeedsForOurDeck;
    std::vector<std::vector<Scalar>> remoteCommitmentKeys;

    if (localPlayer == PlayerID::ONE) {
        // Shuffle seeds for each player's own deck
        network.sendShuffleSeeds(seedsForOurDeck);
        // Opponent's seeds for their deck
        remoteSeedsForTheirDeck = network.receiveShuffleSeeds();

        // Deck hash key and plaintext deck contents
        // Key for hashed deck we sent at the beginning of game as well as the plaintext contents
        network.sendDeckHashKey(deckKey);
        network.sendDeckContents(localDeckContents);
        remoteDeckKey = network.receiveDeckHashKey();
        remoteDeckContents = network.receiveDeckContents();

        // Shuffle seeds for each player's opponent's deck
        network.sendShuffleSeeds(seedsForOpponentDeck);
        remoteSeedsForOurDeck = network.receiveShuffleSeeds();

        // Commitment keys for each player
        network.sendCommitmentKeys(commitmentKeys);
        remoteCommitmentKeys = network.receiveCommitmentKeys();
    }
    else {
        remoteSeedsForTheirDeck = network.receiveShuffleSeeds();
        network.sendShuffleSeeds(seedsForOurDeck);

        remoteDeckKey = network.receiveDeckHashKey();
        remoteDeckContents = network.receiveDeckContents();
        network.sendDeckHashKey(deckKey);
        network.sendDeckContents(localDeckContents);

        remoteSeedsForOurDeck = network.receiveShuffleSeeds();
        network.sendShuffleSeeds(seedsForOpponentDeck);

        remoteCommitmentKeys = network.receiveCommitmentKeys();
        network.sendCommitmentKeys(commitmentKeys);
    }

    std::cout << "  [Verify] Data exchanged. Running verification...\n";

    // verify opponent hasn't changed their deck's contents midgame by comparing the hash they sent us at the start
    // of the game to the hash generated by their plaintext deck contents + hashing salt ("key")
    if (!verifier.verifyRemoteDeckContents(remoteDeckKey, remoteDeckContents)) {
        std::cerr << "  [Verify] FAILED: Opponent's deck commitment does not match!\n";
        return;
    }
    std::cout << "  [Verify] Deck commitment OK.\n";

    // update Verifier to use the opponent's newly-revealed deck
    verifier.initialiseOpponentDeck(remoteDeckContents);

    // supply all remote seeds to verifier
    verifier.addAllRemoteShuffleSeeds(localPlayer, remoteSeedsForOurDeck);
    PlayerID opponent = PlayerIDUtils::getOpponent(localPlayer);
    verifier.addAllRemoteShuffleSeeds(opponent, remoteSeedsForTheirDeck);

    // supply enemy commitment keys to verifier
    if (!verifier.decryptEnemyCommitments(remoteCommitmentKeys)) {
        std::cerr << "  [Verify] FAILED: Opponent sent wrong number of commitment keys!\n";
        return;
    }
    std::cout << "  [Verify] Commitment keys OK.\n";

    // run full gameplay verification
    if (verifier.run()) {
        std::cout << "  [Verify] Game verified successfully. No cheating detected.\n";
    } else {
        std::cerr << "  [Verify] FAILED: Game replay verification detected inconsistencies!\n";
    }
}

// For a deck owned by Alice, with opponent Bob:
//
//   1. Alice converts her deck to curve points, encrypts ALL cards with a
//      single bulk key K, shuffles, and sends the encrypted deck to Bob.
//      Each card is now: E(K, plaintext). Alice knows everything; Bob knows nothing.
//
//   2. Bob receives the deck, encrypts EACH card with a unique per-card key
//      k_i, shuffles, and sends it back to Alice.
//      Each card is now: E(k_i, E(K, plaintext)). Neither player knows the
//      card-to-position mapping (Bob shuffled blind, Alice can't see past k_i).
//
//   3. Alice strips the bulk key K from each card (decrypts with K_inv), then
//      encrypts each card with a unique per-card key k_j. Alice sends the
//      final ciphertexts to Bob so both sides track the same deck.
//      Each card is now: E(k_j, E(k_i, plaintext)). Neither player can
//      read any card alone - drawing/peeking a card p requires both
//      k_j_inv (from Alice) and k_i_inv (from Bob).
//
// IMPORTANT: Alice does NOT shuffle in step 3. The order was fixed by Bob in
// step 2. Alice only re-encrypts in place, preserving positions. This means
// Bob's key k_i for position p is still valid after step 3.

std::vector<std::pair<Point, Scalar>> Game::performShuffle(PlayerID deckOwner) {
    const bool localPlayerIsShuffling = (deckOwner == localPlayer);

    // if the local player owns the deck being shuffled
    if (localPlayerIsShuffling) {
        auto deckPoints = convertCardsToPoints(state.getImmutablePlayerData(deckOwner).deck.getContents());
        // initial bulk encryption with single key
        auto bulkKey = generateKeyPair();
        encryptDeckWithKey(deckPoints, bulkKey.k);
        // shuffle the deck before sending to opponent
        ShuffleSeed seed = randombytes_uniform(UINT32_MAX);
        shuffleDeck(deckPoints, seed);
        verifier.logLocalShuffleSeed(deckOwner, seed);

        // send 1-layer encryption deck to opponent
        std::cout << "  [Shuffle] Sending bulk-encrypted deck to opponent... (" << deckPoints.size() << " cards)\n";
        network.sendPoints(deckPoints);
        // opponent sends us back our deck shuffled and with each card uniquely encrypted
        auto received = network.receivePoints();
        std::cout << "  [Shuffle] Received re-encrypted deck back (" << received.size() << " cards)\n";

        // strip initial bulk encryption layer off of the cards and apply our own unique keys to every
        // card in the deck.
        auto reEncrypted = std::vector<std::pair<Point, PHKeyPair>>();
        reEncrypted.reserve(received.size());
        for (const auto& ct : received) {
            // strip the bulk encryption layer
            Point stripped = decrypt(ct, bulkKey.k_inv);
            // encrypt with unique per-card keys
            auto perCardKey = generateKeyPair();
            Point finalCt = encrypt(stripped, perCardKey.k);
            reEncrypted.emplace_back(finalCt, perCardKey);
        }

        // Send the final ciphertexts to the opponent so both players
        // have synced copies of each other's encrypted decks
        std::vector<Point> finalCiphertexts;
        finalCiphertexts.reserve(reEncrypted.size());
        for (const auto& [ct, key] : reEncrypted) {
            finalCiphertexts.push_back(ct);
        }
        std::cout << "  [Shuffle] Sending final re-encrypted deck to opponent...\n";
        network.sendPoints(finalCiphertexts);

        // build out the deck data and return it
        std::vector<std::pair<Point, Scalar>> result;
        result.reserve(reEncrypted.size());
        for (const auto& [ct, key] : reEncrypted) {
            result.emplace_back(ct, key.k_inv);
        }

        verifier.logAction(Action::Shuffle(deckOwner));
        return result;
    // if it's the opponent's deck being shuffled
    } else {
        // receive the owner's bulk-encrypted, shuffled deck
        auto received = network.receivePoints();
        std::cout << "  [Shuffle] Received opponent's encrypted deck (" << received.size() << " cards)\n";
        // shuffle the deck
        ShuffleSeed seed = randombytes_uniform(UINT32_MAX);
        shuffleDeck(received, seed);
        verifier.logLocalShuffleSeed(deckOwner, seed);
        // encrypt each card with a unique per-card key,
        auto withKeys = encryptDeckWithIndividualKeys(received);

        // now that we've uniquely encrypted each card and shuffled the deck, send it back to its owner
        std::vector<Point> ciphertextsToSend;
        ciphertextsToSend.reserve(withKeys.size());
        for (const auto& [ct, key] : withKeys) {
            ciphertextsToSend.push_back(ct);
        }
        std::cout << "  [Shuffle] Sending re-encrypted deck back to owner...\n";
        network.sendPoints(ciphertextsToSend);

        // receive final ciphertexts from the owner. the deck is now shuffled and neither player knows
        // the order of the cards in the deck
        auto finalDeck = network.receivePoints();
        std::cout << "  [Shuffle] Received final deck from owner (" << finalDeck.size() << " cards)\n";

        // build our deck data with the final ciphertexts and our local unique keys
        std::vector<std::pair<Point, Scalar>> result;
        result.reserve(finalDeck.size());
        for (std::size_t i = 0; i < finalDeck.size(); i++) {
            result.emplace_back(finalDeck[i], withKeys[i].second.k_inv);
        }

        verifier.logAction(Action::Shuffle(deckOwner));
        return result;
    }
}

/**
 * Both players call this at the same time (with the same arguments). The drawing player receives
 * some number of keys (count) for the top cards of their  deck, decrypts them, and adds them to their hand.
 * The opponent sends the keys and tracks the draws
 * @param player Drawing player
 * @param count Number of cards being drawn
 */
void Game::drawCards(PlayerID player, uint8_t count) {
    bool localPlayerIsDrawing = (player == localPlayer);
    auto& drawingPlayerData = state.getPlayerData(player);

    // check that there's enough cards to draw
    uint8_t availableCardsToDraw = std::min(count, drawingPlayerData.deck.getSize());

    if (localPlayerIsDrawing) {
        // receive per-card keys from opponent for the top card(s)
        std::vector<Scalar> receivedKeys = network.receiveScalars();
        uint8_t requiredKeyCount = 0;
        for (int i = 0; i < availableCardsToDraw; i++) {
            // we only need a remote key if we don't already know the card value
            if (!drawingPlayerData.deck.getCardIDAtIndex(i).has_value()) requiredKeyCount++;
        }
        if (receivedKeys.size() != requiredKeyCount)
            throw std::runtime_error("Received unexpected number of keys during draw");

        // Decrypt each relevant card
        for (int i = 0; i < availableCardsToDraw; i++) {
            // If we already know the value of this card, move onto the next one
            if (drawingPlayerData.deck.getCardIDAtIndex(i).has_value()) continue;
            if (!drawingPlayerData.deck.addOpponentKey(i, receivedKeys.front())) {
                throw std::runtime_error("addOpponentKey failed for index " + std::to_string(i));
            }
            receivedKeys.erase(receivedKeys.begin());
        }

        // draw the now-decrypted cards from the top of the deck into our hand
        for (int i = 0; i < count; i++) {
            // Deck empty, take fatigue damage
            if (drawingPlayerData.deck.getSize() == 0) {
                std::cout << "  [Draw] Your deck is empty.\n";
                drawingPlayerData.currentHealth -= ++drawingPlayerData.fatigueCount;
                std::cout << "  [Fatigue] You took " << std::to_string(drawingPlayerData.fatigueCount) << " fatigue damage.\n";
                continue;
            }

            // Hand full, we need to mill card to graveyard
            if (std::get<ClearHand>(drawingPlayerData.hand).isFull()) {
                std::cout << "  [Draw] Your hand is full, so you failed to draw the card!\n";
                // if opponent doesn't know the value of this card, send them the key
                if (!drawingPlayerData.deck.isKnownToOpponent(0)) {
                    network.sendScalar(drawingPlayerData.deck.getLocalKey(0).value()); // should be known (unless bug)
                }
                // then we both mill it
                auto card = drawingPlayerData.deck.draw();
                if (!card.has_value())
                    throw std::runtime_error("[Draw] Drawn card ID not found in lookup table, meaning dud key received"); // (or bug)
                drawingPlayerData.graveyard.push_back(card.value());
                std::cout << "  [Overdraw] You milled " << CardFactory::create(card.value())->getName() << ".\n";

                continue;
            }

            // hand not full & deck not empty: draw card
            auto card = drawingPlayerData.deck.draw();
            if (!card.has_value())
                throw std::runtime_error("[Draw] Drawn card ID not found in lookup table, meaning dud key received"); // (or bug)
            std::get<ClearHand>(drawingPlayerData.hand).addCard(card.value());
            std::cout << "  [Draw] Drew: " << CardFactory::create(card.value())->getName() << "\n";
        }
    }
    // opponent drawing card(s)
    else {
        std::vector<Scalar> keysToSend; // our local keys that opponent needs for drawing (unless they already know the card value)
        keysToSend.reserve(availableCardsToDraw);

        for (int i = 0; i < availableCardsToDraw; i++) {
            // if opponent already knows the value of the card, continue to next card
            if (drawingPlayerData.deck.isKnownToOpponent(i)) continue;
            std::optional<Scalar> localKey = drawingPlayerData.deck.getLocalKey(i);
            if (!localKey.has_value()) {
                // they don't know the card value... but we don't have the key? something has gone wrong here. exception
                throw std::runtime_error("No local key available for card in opponent's deck at index " + std::to_string(i));
            }
            keysToSend.push_back(localKey.value());
        }
        network.sendScalars(keysToSend);

        // update our opponent's deck and hands (remove from deck, add to hand)
        for (int i = 0; i < count; i++) {
            // Deck empty, take fatigue damage
            if (drawingPlayerData.deck.getSize() == 0) {
                std::cout << "  [Draw] Opponent's deck is empty.\n";
                drawingPlayerData.currentHealth -= ++drawingPlayerData.fatigueCount;
                std::cout << "  [Fatigue] Opponent took " << std::to_string(drawingPlayerData.fatigueCount) << " fatigue damage.\n";
                continue;
            }

            // Hand full, we need to mill card to graveyard
            if (std::get<UnknownHand>(drawingPlayerData.hand).isFull()) {
                std::cout << "  [Draw] Opponent's hand is full, so they failed to draw a card!\n";
                // if we don't know the value of this card, get the remote key
                if (!drawingPlayerData.deck.getCardIDAtIndex(0).has_value()) {
                    drawingPlayerData.deck.addOpponentKey(0, network.receiveScalar());
                }
                // then we both mill it
                auto card = drawingPlayerData.deck.draw();
                if (!card.has_value())
                    throw std::runtime_error("[Draw] Drawn card ID not found in lookup table, meaning dud key received"); // (or bug)
                drawingPlayerData.graveyard.push_back(card.value());
                std::cout << "  [Overdraw] Opponent milled " << CardFactory::create(card.value())->getName() << ".\n";

                continue;
            }

            // hand not full & deck not empty: draw card
            auto card = drawingPlayerData.deck.draw();
            std::get<UnknownHand>(drawingPlayerData.hand).addCard();
            std::cout << "  [Draw] Opponent drew " << (card.has_value() ? CardFactory::create(card.value())->getName() : "a card") << ".\n";
        }
    }

    verifier.logAction(Action::DrawCards(player, count));
}

void Game::startTurn() {
    PlayerID active = state.activePlayer.value();
    // reset mana
    state.getPlayerData(active).currentMana = state.getPlayerData(active).maxMana;
    // draw a card
    drawCards(active, 1);
}

void Game::advanceTurn() {
    verifier.logAction(Action::EndTurn(state.activePlayer.value()));
    state.activePlayer = PlayerIDUtils::getOpponent(state.activePlayer.value());
}

/**
 * Display game state, prompt for actions, send packets to opponent.
 */
void Game::runMyTurn() {
    auto& myData = state.getPlayerData(localPlayer);

    // Reset mana at the start of our action phase
    // (already done in startTurn, but if this is turn 1 we reset here)
    // Actually: let's just ensure mana is correct
    // myData.currentMana is set by startTurn() already

    while (true) {
        printGameState();
        auto& hand = std::get<ClearHand>(myData.hand);

        // check if we can actually play anything
        // TODO: actually, why do we even bother doing this? maybe instead just generate a set of indices of cards we can play
        bool canPlay = false;
        for (const auto& entry : hand.ui_getHandContents()) {
            auto card = CardFactory::create(entry);
            if (card->getManaCost() <= myData.currentMana) {
                canPlay = true;
                break;
            }
        }

        if (hand.getSize() == 0) {
            std::cout << "No cards in hand. ";
        }

        // user prompt
        // TODO: upgrade this to UI events
        std::cout << "\nEnter card number to play (1-" << std::to_string(hand.getSize()) << "), or 0 to end turn: ";
        int choice;
        std::cin >> choice;

        if (choice == 0) {
            network.sendPacketType(PacketType::END_TURN);
            advanceTurn();
            std::cout << "  -> Turn ended.\n";
            return;
        }
        if (choice < 1 || choice > static_cast<int>(hand.getSize())) {
            std::cout << "  Invalid choice.\n";
            continue;
        }
        int handIndex = choice - 1;
        auto card = CardFactory::create(hand.getCardAtIndex(handIndex));
        if (card->getManaCost() > myData.currentMana) {
            std::cout << "  Not enough mana! (need " << card->getManaCost() << ", have " << std::to_string(myData.currentMana) << ")\n";
            continue;
        }
        // play the card
        playCardLocal(handIndex);

        if (state.isGameOver().first) return;
    }
}

/**
 * We (the local player) want to play a card. We send the play card request to our opponent and, if they
 * permit it, we resolve the effect.
 * @param handIndex Index of card in our hand that we want to play
 */
void Game::playCardLocal(int handIndex) {
    auto& myData = state.getPlayerData(localPlayer);
    auto& hand = std::get<ClearHand>(myData.hand);

    const CardID cardId = hand.getCardAtIndex(handIndex);
    auto card = CardFactory::create(cardId);

    // send play request to opponent
    network.sendPacketType(PacketType::PLAY_CARD);
    network.sendUint16(static_cast<uint16_t>(cardId));
    PacketType response = network.receivePacketType();
    if (response == PacketType::DENIED) {
        std::cout << "  Card play DENIED by opponent.\n"; // happens if you can't afford the card
        return;
    }
    if (response != PacketType::PERMITTED) {
        std::cerr << "  Unexpected packet type: " << static_cast<int>(response) << "\n";
        return;
    }

    std::cout << "  -> Playing " << card->getName() << "!\n";
    verifier.logAction(Action::PlayCard(localPlayer, cardId));

    // pay mana
    myData.currentMana -= card->getManaCost();
    // delete from hand
    hand.removeCard(cardId);
    // resolve the card's effect
    card->resolve(*this, localPlayer);
    // add to graveyard / discard pile
    myData.graveyard.push_back(cardId);
}

/**
 * Wait for packets and respond to them when they arrive. Players have no autonomy on their opponent's turns
 * except when a card is played which requires the inactive player's input (e.g., Fortune's Favour)
 */
void Game::runOpponentTurn() {
    std::cout << "\n--- Opponent's turn ---\n";
    // main loop while on opponent's turn
    while (true) {
        switch (PacketType packetType = network.receivePacketType()) {
            using enum PacketType;
            case END_TURN: {
                advanceTurn();
                std::cout << "  Opponent ended their turn.\n";
                return;
            }
            case PLAY_CARD: {
                handleOpponentPlayCard();
                if (state.isGameOver().first) return;
                break;
            }
            case CONCEDE: {
                std::cout << "  Opponent conceded!\n";
                // Set opponent's health to 0 to trigger game over
                auto& oppData = state.getOpponentPlayerData(localPlayer);
                oppData.currentHealth = 0;
                return;
            }
            default:
                std::cerr << "  Unexpected packet type: " << static_cast<int>(packetType) << "\n";
                break;
        }
    }
}

void Game::handleOpponentPlayCard() {
    uint16_t rawId = network.receiveUint16();
    CardID cardId = static_cast<CardID>(rawId);
    auto card = CardFactory::create(cardId);

    PlayerID opponent = PlayerIDUtils::getOpponent(localPlayer);
    auto& oppData = state.getPlayerData(opponent);
    auto& hand = std::get<UnknownHand>(oppData.hand);

    // check mana and hand size (can they afford it? do they have a card in hand?). when we add hand-tracking with
    // revealed cards we can make this even more sophisticated (post-game verification also plays a part later)
    if (card->getManaCost() > oppData.currentMana) {
        std::cout << "  Opponent tried to play " << card->getName()
                  << " but doesn't have enough mana. DENIED.\n";
        network.sendPacketType(PacketType::DENIED);
        return;
    }
    if (hand.getSize() == 0) {
        std::cout << "  Opponent tried to play a card with an empty hand. DENIED.\n";
        network.sendPacketType(PacketType::DENIED);
        return;
    }

    network.sendPacketType(PacketType::PERMITTED);
    std::cout << "  Opponent plays " << card->getName() << "!\n";
    verifier.logAction(Action::PlayCard(PlayerIDUtils::getOpponent(localPlayer), cardId));

    // opponent spent mana
    oppData.currentMana -= card->getManaCost();
    // Remove one card from opponent's hand
    hand.removeCard();

    // Add to graveyard
    oppData.graveyard.push_back(cardId);

    // Resolve the card's effect. Both players must call resolve() with the same controller argument so that
    // the game state stays in sync.
    card->resolve(*this, opponent);
}

void Game::dealDamage(PlayerID target, int amount) {
    auto& data = state.getPlayerData(target);
    data.currentHealth = std::max(0, static_cast<int>(data.currentHealth) - amount);
    if (target == localPlayer) {
        std::cout << "  -> You take " << amount << " damage! ("
                  << data.currentHealth << " HP remaining)\n";
    } else {
        std::cout << "  -> Opponent takes " << amount << " damage! ("
                  << data.currentHealth << " HP remaining)\n";
    }
    verifier.logAction(Action::DealDamage(target, amount));
}

void Game::gainLife(PlayerID target, int amount) {
    state.getPlayerData(target).currentHealth += amount;
    verifier.logAction(Action::GainLife(target, amount));
}

int Game::getMana(PlayerID player) const {
    return state.getImmutablePlayerData(player).currentMana;
}

/**
 * Temporary function for displaying the game whilst graphics are unimplemented. Ugly but does the job
 */
void Game::printGameState() const {
    PlayerID opponent = PlayerIDUtils::getOpponent(localPlayer);
    const auto& myData = state.getImmutablePlayerData(localPlayer);
    const auto& oppData = state.getImmutablePlayerData(opponent);
    const auto& ourHand = std::get<ClearHand>(myData.hand);
    const auto& oppHand = std::get<UnknownHand>(oppData.hand);

    std::cout << "\n\n OPPONENT: " << oppData.currentHealth << " HP | "
              << static_cast<int>(oppData.currentMana) << " mana | "
              << static_cast<int>(oppData.deck.getSize()) << " cards in deck | "
              << std::to_string(oppHand.getSize()) << " in hand\n";
    std::cout << "---------------------------------------\n";
    std::cout << "  YOU:      " << myData.currentHealth << " HP | "
              << static_cast<int>(myData.currentMana) << " mana | "
              << static_cast<int>(myData.deck.getSize()) << " cards in deck\n";
    std::cout << "  Hand:\n";

    int count = 0;
    for (auto cardId : ourHand.ui_getHandContents()) {
        auto card = CardFactory::create(cardId);
        std::cout << "    " << (++count) << ". " << card->getName()
                  << " (cost: " << card->getManaCost() << ")\n";
    }
    std::cout << "---------------------------------------\n";
}
