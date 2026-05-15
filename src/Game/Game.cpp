#include "Game.h"
#include <iostream>
#include <algorithm>
#include "../Cards/CardFactory.h"
#include "../DeckCommitment.h"
#include "../GUI/GameBridge.h"

Game::Game(Network& network, PlayerID localPlayer, const std::map<CardID, uint8_t>& localDeckContents)
    : network(network), verifier(localDeckContents, localPlayer),
    state(localDeckContents, localPlayer), localPlayer(localPlayer),
    localDeckContents(localDeckContents)
{
    // disable tracking of opponent's deck
    state.getOpponentPlayerData(localPlayer).deck.disableContentsTracking();
}

/**
 * Generates a GameSnapshot (basically a container of important game info) from the data in the gamestate. Used for GUI.
 * @param statusMessage Status message to include with the snapshot
 * @return A GameSnapshot object to be used in the GUI renderer
 */
GameSnapshot Game::buildSnapshot(const std::string& statusMessage) {
    PlayerID opponent = PlayerIDUtils::getOpponent(localPlayer);
    const auto& myData = state.getImmutablePlayerData(localPlayer);
    const auto& oppData = state.getImmutablePlayerData(opponent);

    GameSnapshot snap;
    snap.myHealth = myData.currentHealth;
    snap.myMana = myData.currentMana;
    snap.myDeckSize = myData.deck.getSize();

    if (const auto hand = std::get_if<ClearHand>(&myData.hand)) {
        snap.myHand = hand->ui_getHandContents();
    }

    snap.oppHealth = oppData.currentHealth;
    snap.oppMana = oppData.currentMana;
    snap.oppDeckSize = oppData.deck.getSize();
    snap.oppHandSize = static_cast<int>(state.getHandSize(opponent));

    snap.isMyTurn = state.activePlayer.has_value() && state.activePlayer.value() == localPlayer;
    snap.statusMessage = statusMessage;

    auto [gameOver, winner] = state.isGameOver();
    snap.gameOver = gameOver;
    if (gameOver) {
        if (!winner.has_value()) snap.winnerMessage = "It's a tie!";
        else if (winner.value() == localPlayer) snap.winnerMessage = "You win!";
        else snap.winnerMessage = "You lose!";
    }

    return snap;
}

/**
 * Updates the UI with a snapshot of the game data.
 * @param statusMessage Optional status message for this snapshot
 */
void Game::publishSnapshot(const std::string& statusMessage) {
    if (bridge) bridge->publishState(buildSnapshot(statusMessage));
}

void Game::run() {
    publishSnapshot("Exchanging deck commitments...");
    exchangeDeckCommitments();

    // Coin flip deciding who goes first. In its own mini scope to clean up temporary stack variable
    std::cout << "\n=== Turn order coin toss ===\n";
    {
        auto playerGoingFirst = static_cast<PlayerID>(flipCoin());
        state.activePlayer = playerGoingFirst;
        verifier.setPlayerGoingFirst(playerGoingFirst);
        std::cout << "  [Coin Toss] " << (playerGoingFirst == localPlayer ? "You go" : "Opponent goes") << " first!\n";
    }

    std::cout << "\n=== Shuffling decks ===\n";
    publishSnapshot("Shuffling decks...");
    performShuffle(PlayerID::ONE);
    performShuffle(PlayerID::TWO);

    std::cout << "\n=== Drawing opening hands ===\n";
    publishSnapshot("Drawing opening hands...");
    drawCards(PlayerID::ONE, 4);
    drawCards(PlayerID::TWO, 4);

    std::cout << "\n=== Game starting! ===\n";
    publishSnapshot();

    // First turn doesn't draw (like MtG: player going first skips first draw)
    bool firstTurn = true;

    // main gameplay loop
    while (!state.isGameOver().first) {
        if (!firstTurn) {
            startTurn();
            if (state.isGameOver().first) break;
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

    publishSnapshot("Game over! Running verification...");
    postGameExchangeAndVerify();
    publishSnapshot();
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
    auto localSeedsForOurDeck = verifier.getLocalShuffleSeedsForOurDeck();
    const DeckHashKey& deckKey = verifier.getLocalDeckCommitmentKey();
    auto localSeedsForOpponentDeck = verifier.getLocalShuffleSeedsForOpponentsDeck();
    const auto& commitmentKeys = verifier.getLocalCommitmentKeys();

    // Containers for data we receive
    std::vector<ShuffleSeed> remoteSeedsForOpponentDeck;
    DeckHashKey remoteDeckKey;
    std::map<CardID, uint8_t> remoteDeckContents;
    std::vector<ShuffleSeed> remoteSeedsForOurDeck;
    std::vector<std::vector<Scalar>> remoteCommitmentKeys;

    if (localPlayer == PlayerID::ONE) {
        // Shuffle seeds for each player's own deck
        network.sendShuffleSeeds(localSeedsForOurDeck);
        // Opponent's seeds for their deck
        remoteSeedsForOpponentDeck = network.receiveShuffleSeeds();

        // Deck hash key and plaintext deck contents
        // Key for hashed deck we sent at the beginning of game as well as the plaintext contents
        network.sendDeckHashKey(deckKey);
        network.sendDeckContents(localDeckContents);
        remoteDeckKey = network.receiveDeckHashKey();
        remoteDeckContents = network.receiveDeckContents();

        // Shuffle seeds for each player's opponent's deck
        network.sendShuffleSeeds(localSeedsForOpponentDeck);
        remoteSeedsForOurDeck = network.receiveShuffleSeeds();

        // Commitment keys for each player
        network.sendCommitmentKeys(commitmentKeys);
        remoteCommitmentKeys = network.receiveCommitmentKeys();
    }
    else {
        remoteSeedsForOpponentDeck = network.receiveShuffleSeeds();
        network.sendShuffleSeeds(localSeedsForOurDeck);

        remoteDeckKey = network.receiveDeckHashKey();
        remoteDeckContents = network.receiveDeckContents();
        network.sendDeckHashKey(deckKey);
        network.sendDeckContents(localDeckContents);

        remoteSeedsForOurDeck = network.receiveShuffleSeeds();
        network.sendShuffleSeeds(localSeedsForOpponentDeck);

        remoteCommitmentKeys = network.receiveCommitmentKeys();
        network.sendCommitmentKeys(commitmentKeys);
    }

    std::cout << "  [Verify] Data exchanged. Running verification...\n";

    if (!verifier.validateRemoteDeckContents(remoteDeckContents)) {
        std::cerr << "  [Verify] FAILED: Opponent's deck is illegally composed!\n";
        return;
    }
    std::cout << "  [Verify] Deck validation OK.\n";

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
    verifier.addAllRemoteShuffleSeeds(opponent, remoteSeedsForOpponentDeck);

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

/**
 * Networked method, callable in card effects. Conducts full mental poker shuffle protocol using commutative encryption.
 * Also logs the seed used to shuffle the deck so the shuffle may be recreated deterministically during verification.
 * @param deckOwner Owner of deck being shuffled
 */
void Game::performShuffle(PlayerID deckOwner) {
    const bool localPlayerIsShuffling = (deckOwner == localPlayer);
    std::vector<std::pair<Point, Scalar>> result;

    // if the local player owns the deck being shuffled
    if (localPlayerIsShuffling) {
        auto deckPoints = convertCardsToPoints(state.getImmutablePlayerData(deckOwner).deck.getContents());
        // initial bulk encryption with single key
        auto bulkKey = generateKeyPair();
        encryptCardsWithKey(deckPoints, bulkKey.k);
        // shuffle the deck before sending to opponent
        ShuffleSeed seed = randombytes_uniform(UINT32_MAX);
        shuffleCards(deckPoints, seed);
        verifier.logLocalShuffleSeed(deckOwner, seed);

        // send 1-layer encryption deck to opponent
        std::cout << "  [Shuffle] Sending bulk-encrypted deck to opponent... (" << deckPoints.size() << " cards)\n";
        network.sendPoints(deckPoints);
        // opponent sends us back our deck shuffled and with each card uniquely encrypted
        auto received = network.receivePoints();
        std::cout << "  [Shuffle] Received re-encrypted deck back (" << received.size() << " cards)\n";

        // strip initial bulk encryption layer off of the cards and apply our own unique keys to every card in the deck
        std::vector<std::pair<Point, PHKeyPair>> reEncrypted;
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
        std::vector<Point> finalDeck;
        finalDeck.reserve(reEncrypted.size());
        for (const auto& [ct, key] : reEncrypted) {
            finalDeck.push_back(ct);
        }
        std::cout << "  [Shuffle] Sending final re-encrypted deck to opponent...\n";
        network.sendPoints(finalDeck);

        // build out the deck data
        result.reserve(reEncrypted.size());
        for (const auto& [ct, key] : reEncrypted) {
            result.emplace_back(ct, key.k_inv);
        }
    // if it's the opponent's deck being shuffled
    } else {
        // receive the owner's bulk-encrypted, shuffled deck
        auto received = network.receivePoints();
        std::cout << "  [Shuffle] Received opponent's encrypted deck (" << received.size() << " cards)\n";
        // shuffle the deck
        ShuffleSeed seed = randombytes_uniform(UINT32_MAX);
        shuffleCards(received, seed);
        verifier.logLocalShuffleSeed(deckOwner, seed);
        // encrypt each card with a unique per-card key,
        auto withKeys = encryptCardsWithIndividualKeys(received);

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
        std::vector<Point> finalDeck = network.receivePoints();
        std::cout << "  [Shuffle] Received final deck from owner (" << finalDeck.size() << " cards)\n";

        // build our deck data with the final ciphertexts and our local unique keys
        result.reserve(finalDeck.size());
        for (std::size_t i = 0; i < finalDeck.size(); i++) {
            result.emplace_back(finalDeck[i], withKeys[i].second.k_inv);
        }
    }

    // Update deck data with new ordered, encrypted deck contents and log action to verifier
    state.getPlayerData(deckOwner).deck.setEncryptedContents(result);
    verifier.logAction(Action::Shuffle(deckOwner));
}

void Game::discard(PlayerID player, CardID card) {
    auto& playerData = state.getPlayerData(player);
    if (auto clearHand = std::get_if<ClearHand>(&playerData.hand)) {
        if (!clearHand->removeCard(card)) {
            throw std::runtime_error(std::format(
                "[Game::discard] Error: Card '{}' not found in local player's hand", CardFactory::create(card)->getName()
            ));
        }
    }
    else {
        auto& unknownHand = std::get<UnknownHand>(playerData.hand);
        // This returns false if hand is empty. Could throw exception if desired, but I think it's OK
        unknownHand.removeCard();
    }
    if (bridge) bridge->enqueueEvent(CardDiscardedEvent{card, player});
    playerData.graveyard.push_back(card);
    verifier.logAction(Action::Discard(player, card));
}

/**
 * Put cards from the top of a player's deck into their graveyard (a process known as "milling").
 * @param millingPlayer Player milling
 * @param count Number of cards to mill
 * @param logInVerifier Whether to log the action in the verifier. False when part of Game::draw resolution
 */
void Game::mill(PlayerID millingPlayer, uint8_t count, const bool logInVerifier) {
    auto& playerData = state.getPlayerData(millingPlayer);
    // count should never be 0, always incorrect behaviour. deck may be empty, however, in which case no mill occurs
    if (count == 0 || playerData.deck.getSize() == 0) return;
    uint8_t totalToMill = std::min(count, playerData.deck.getSize());

    std::set<uint8_t> indicesOfLocallyUnknown = playerData.deck.getIndicesOfLocallyUnknown(totalToMill - 1);
    std::set<uint8_t> indicesOfRemotelyUnknown = playerData.deck.getIndicesOfRemotelyUnknown(totalToMill - 1);

    // lambda to receive keys and update the player's deck with those new keys
    auto receiveKeys = [&] {
        if (indicesOfLocallyUnknown.empty()) return;
        std::vector<Scalar> receivedKeys = network.receiveScalars();
        if (receivedKeys.size() != indicesOfLocallyUnknown.size())
            throw std::runtime_error("[Game::mill] Received invalid number of keys.\n");
        auto keyIt = receivedKeys.begin();
        for (auto index : indicesOfLocallyUnknown) playerData.deck.addOpponentKey(index, *keyIt++);
    };

    // lambda to send keys for cards that the opponent doesn't know
    auto sendKeys = [&] {
        if (indicesOfRemotelyUnknown.empty()) return;
        std::vector<Scalar> keysToSend = playerData.deck.getLocalKeysAtIndices(indicesOfRemotelyUnknown);
        network.sendScalars(keysToSend);
        playerData.deck.setKnownToOpponentAtIndices(indicesOfRemotelyUnknown);
    };

    if (millingPlayer == localPlayer) {
        receiveKeys();
        sendKeys();
    } else {
        sendKeys();
        receiveKeys();
    }

    // mill from deck, add milled cards to yard, log mill action
    std::vector<CardID> milledCards = playerData.deck.mill(totalToMill);
    // TODO: C++23 has append_range which would compress this code: playerData.graveyard.append_range(playerData.deck.mill(totalToMill));
    playerData.graveyard.insert(
        playerData.graveyard.end(),
        milledCards.begin(),
        milledCards.end()
    );
    if (milledCards.size() > 1) {
        std::cout << "  [Mill] " << (millingPlayer == localPlayer ? "You" : "Opponent") << " milled " << std::to_string(totalToMill) << " cards:\n  ";
        std::string cardsMilledString;
        for (const auto card : milledCards) cardsMilledString += (CardFactory::create(card)->getName() + ", ");
        cardsMilledString.replace(cardsMilledString.size() - 2, 2, "\n");
        std::cout << cardsMilledString;
    }
    else {
        std::cout << "  [Mill] " << (millingPlayer == localPlayer ? "You" : "Opponent") << " milled "
            << CardFactory::create(milledCards[0])->getName() << "\n";
    }
    if (logInVerifier) verifier.logAction(Action::Mill(millingPlayer, totalToMill));
    if (bridge) bridge->enqueueEvent(CardsMilledEvent{millingPlayer, static_cast<int>(totalToMill)});
}

void Game::addUnencryptedCardToDeck(PlayerID deckOwner, CardID card, uint8_t index) {
    auto& playerData = state.getPlayerData(deckOwner);
    if (!playerData.deck.addUnencryptedCard(card, index)) {
        throw std::runtime_error("[Game::addUnencryptedCardToDeck] Invalid index supplied.");
    }
    verifier.logAction(Action::AddCardToDeck(deckOwner, card, index));
}

/**
 * Networked function that simulates a coin flip between the two players. Implementation is a standard Blum coin flip.
 * Alice sends Bob a commitment of the bit she chose. Bob then sends her his bit. She then reveals her initial bit.
 * The result of the coin toss is Alice's bit XOR Bob's bit.
 * @return True if heads, false if tails
 */
bool Game::flipCoin() const {
    // Not to be confused with verification phase commitments. This is just for the 32-byte hash of our random bit
    using Commitment = std::array<uint8_t, crypto_generichash_BYTES>;
    using CoinNonce = std::array<uint8_t, 32>;

    // Lambda used here to save on repeating the exact same code in two separate places. This function uses
    // libsodium's streaming hash capabilities, so building a hash from a stream of data (nonce & bit in our case)
    auto computeCommitment = [](const CoinNonce& nonce, uint8_t bit) {
        Commitment commBuffer;
        crypto_generichash_state hashState;
        // intialise state without a key - we use the nonce to hide the underlying val
        crypto_generichash_init(&hashState, nullptr, 0, commBuffer.size());
        // feed 32 byte nonce into hash
        crypto_generichash_update(&hashState, nonce.data(), nonce.size());
        // append the bit byte (7 bits unused) to the stream
        crypto_generichash_update(&hashState, &bit, sizeof(bit));
        // finally output 32 byte commitment
        crypto_generichash_final(&hashState, commBuffer.data(), commBuffer.size());
        return commBuffer;
    };

    // generate a random bit (2 = excluded upper bound, so val of either 0 or 1)
    const uint8_t localBit = static_cast<uint8_t>(randombytes_uniform(2));
    uint8_t remoteBit;

    if (localPlayer == PlayerID::ONE) {
        CoinNonce myNonce;
        randombytes_buf(myNonce.data(), myNonce.size()); // generate some random 32-byte nonce
        // generate a commitment (Blake2b hash) of the bit and nonce
        const Commitment myCommitment = computeCommitment(myNonce, localBit);

        // Player One commits their bit to Player Two, receives Player Two's bit, then reveals their own nonce & bit
        network.sendAll(myCommitment.data(), myCommitment.size()); // send commitment
        remoteBit = network.receiveUint8(); // receive Player Two's bit
        network.sendAll(myNonce.data(), myNonce.size()); // send our nonce
        network.sendUint8(localBit); // send our bit
    } else {
        // Receive commitment, then send our bit, then receive and verify reveal
        Commitment theirCommitment;
        network.receiveAll(theirCommitment.data(), theirCommitment.size()); // receive commitment
        network.sendUint8(localBit); // send our bit

        CoinNonce theirNonce;
        network.receiveAll(theirNonce.data(), theirNonce.size()); // receive their nonce
        remoteBit = network.receiveUint8(); // receive their bit
        if (remoteBit > 1) throw std::runtime_error("[Game::flipCoin] Opponent sent more than one bit of data");

        const Commitment expected = computeCommitment(theirNonce, remoteBit);
        if (sodium_memcmp(theirCommitment.data(), expected.data(), expected.size()) != 0) {
            throw std::runtime_error("[Game::flipCoin] Opponent's commitment does not match their reveal");
        }
    }

    if (remoteBit > 1) throw std::runtime_error("[Game::flipCoin] Opponent sent more than one bit of data");
    return (localBit ^ remoteBit);
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

    // get indices of cards unknown to drawer (i.e., indices for which they'll need keys)
    std::set<uint8_t> indicesDrawerDoesntKnow = localPlayerIsDrawing
        ? drawingPlayerData.deck.getIndicesOfLocallyUnknown(std::min(count, drawingPlayerData.deck.getSize()) - 1)
        : drawingPlayerData.deck.getIndicesOfRemotelyUnknown(std::min(count, drawingPlayerData.deck.getSize()) - 1);

    if (localPlayerIsDrawing) {
        // receive per-card keys from opponent for the top card(s)
        std::vector<Scalar> receivedKeys = network.receiveScalars();
        if (receivedKeys.size() != indicesDrawerDoesntKnow.size())
            throw std::runtime_error("[Game::drawCards] Received unexpected number of keys during draw");

        // Decrypt each relevant card
        for (const auto index : indicesDrawerDoesntKnow) {
            if (!drawingPlayerData.deck.addOpponentKey(index, receivedKeys.front()))
                throw std::runtime_error("[Game::drawCards] addOpponentKey failed for index " + std::to_string(index));
            receivedKeys.erase(receivedKeys.begin());
        }

        // draw the now-decrypted cards from the top of the deck into our hand
        for (int i = 0; i < count; i++) {
            // Deck empty, take fatigue damage for remaining card draws
            if (drawingPlayerData.deck.getSize() == 0) {
                std::cout << "  [Draw] Your deck is empty.\n";
                uint8_t remainingDraws = count - i;
                for (int drawIndex = 0; drawIndex < remainingDraws; drawIndex++) {
                    drawingPlayerData.currentHealth -= ++drawingPlayerData.fatigueCount;
                    std::cout << "  [Fatigue] You took " << std::to_string(drawingPlayerData.fatigueCount) << " fatigue damage.\n";
                }
                break;
            }

            // Hand full but deck still contains cards; we need to mill cards to graveyard
            if (std::get<ClearHand>(drawingPlayerData.hand).isFull()) {
                std::cout << "  [Draw] Your hand is full, so you failed to draw the card!\n";
                uint8_t totalCardsToMill = std::min(drawingPlayerData.deck.getSize(), static_cast<uint8_t>(count - i));
                mill(player, totalCardsToMill, false);
                i += totalCardsToMill - 1;
                // have to continue as we may need to take fatigue. e.g., we draw 5 cards with only 3 in the deck. we
                // need to mill the first 3 cards (what we do here), then take fatigue damage for the last 2 draws
                continue;
            }

            // Hand not full & deck not empty: draw card
            auto drawnCard = drawingPlayerData.deck.draw();
            if (!drawnCard.has_value())
                throw std::runtime_error("[Draw] Drawn card ID not found in lookup table, meaning dud key received"); // (or bug)
            std::get<ClearHand>(drawingPlayerData.hand).addCard(drawnCard.value());
            std::cout << "  [Draw] Drew: " << CardFactory::create(drawnCard.value())->getName() << "\n";
        }
    }
    // opponent drawing card(s)
    else {
        // send opponent our local keys for the cards they need to draw
        network.sendScalars(drawingPlayerData.deck.getLocalKeysAtIndices(indicesDrawerDoesntKnow));

        // update our opponent's deck and hands (remove from deck, add to hand)
        for (int i = 0; i < count; i++) {
            // Deck empty, take fatigue damage for remaining card draws
            if (drawingPlayerData.deck.getSize() == 0) {
                std::cout << "  [Draw] Opponent's deck is empty.\n";
                uint8_t remainingDraws = count - i;
                for (int drawIndex = 0; drawIndex < remainingDraws; drawIndex++) {
                    drawingPlayerData.currentHealth -= ++drawingPlayerData.fatigueCount;
                    std::cout << "  [Fatigue] Opponent took " << std::to_string(drawingPlayerData.fatigueCount) << " fatigue damage.\n";
                }
                break;
            }

            // Hand full, we need to mill card to graveyard
            if (std::get<UnknownHand>(drawingPlayerData.hand).isFull()) {
                std::cout << "  [Draw] Opponent's hand is full, so they failed to draw a card!\n";
                uint8_t totalCardsToMill = std::min(drawingPlayerData.deck.getSize(), static_cast<uint8_t>(count - i));
                mill(player, totalCardsToMill, false);
                i += totalCardsToMill - 1;
                continue;
            }

            // Hand not full & deck not empty: draw card
            auto drawnCard = drawingPlayerData.deck.draw();
            std::get<UnknownHand>(drawingPlayerData.hand).addCard();
            std::cout << "  [Draw] Opponent drew " << (drawnCard.has_value() ? CardFactory::create(drawnCard.value())->getName() : "a card") << ".\n";
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

    while (true) {
        auto& hand = std::get<ClearHand>(myData.hand);

        int choice;
        // if bridge between UI and game logic exists i.e., if we're playing with UI enabled)
        if (bridge) {
            auto snapshot = buildSnapshot();
            snapshot.isMyTurn = true;
            choice = bridge->publishStateAndWaitForInput(snapshot);
            if (choice == -1) {
                // Window closed so concede and exit
                network.sendPacketType(PacketType::CONCEDE);
                state.getPlayerData(localPlayer).currentHealth = 0;
                return;
            }
        } else {
            printGameState();

            // check if we can actually play anything
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

            std::cout << "\nEnter card number to play (1-" << std::to_string(hand.getSize()) << "), or 0 to end turn: ";
            std::cin >> choice;
        }

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
    if (bridge) bridge->enqueueEvent(CardPlayedEvent{cardId, localPlayer});
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
    publishSnapshot();
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
                publishSnapshot();
                if (state.isGameOver().first) return;
                break;
            }
            case CONCEDE: {
                std::cout << "  Opponent conceded!\n";
                // Set opponent's health to 0 to trigger game over
                auto& oppData = state.getOpponentPlayerData(localPlayer);
                oppData.currentHealth = 0;
                publishSnapshot();
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
    if (bridge) bridge->enqueueEvent(CardPlayedEvent{cardId, opponent});

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
    if (bridge) bridge->enqueueEvent(DamageDealtEvent{target, amount});
}

void Game::gainLife(PlayerID target, int amount) {
    state.getPlayerData(target).currentHealth += amount;
    verifier.logAction(Action::GainLife(target, amount));
    if (bridge) bridge->enqueueEvent(LifeGainedEvent{target, amount});
}

int Game::getMana(PlayerID player) const {
    return state.getImmutablePlayerData(player).currentMana;
}

std::map<CardID, uint8_t> Game::getLocalPlayerHandContents() const {
    return std::get<ClearHand>(state.getImmutablePlayerData(localPlayer).hand).getUnorderedHandContents();
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
