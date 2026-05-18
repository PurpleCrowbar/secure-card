#include "Deck.h"
#include <format>
#include "../Cryptosystem.h"

/**
 * Empty deck constructor. Called at GameState initialisation for the remote player's deck.
 */
Deck::Deck() : lookupTable(LookupTable::instance()) {}

/**
 * Constructor for Deck with encrypted cards and local keys. As the local deck is initialised as plaintext, and the
 * remote deck is initialised as empty, this should never <i>currently</i> be getting called.
 * @param encryptedDeck Vector of encrypted cards and their corresponding local keys
 */
Deck::Deck(const std::vector<std::pair<Point, Scalar>>& encryptedDeck) : lookupTable(LookupTable::instance()) {
    setEncryptedContents(encryptedDeck);
}

/**
 * Plaintext deck constructor. Called at GameState initialisation for the local player's deck. Importantly, this
 * populates the plaintextContents tracker which is critically important for the task of shuffling the deck.
 * @param plaintextDeckContents Vector of plaintext card IDs
 */
Deck::Deck(const std::map<CardID, uint8_t>& plaintextDeckContents) : lookupTable(LookupTable::instance()) {
    if (plaintextDeckContents.empty()) throw std::invalid_argument("[Deck] plaintextDeck passed to constructor is empty");

    std::vector<CardID> cards;
    for (const auto [id, quantity] : plaintextDeckContents) {
        for (uint8_t i = 0; i < quantity; ++i) {
            plaintextContents.value()[id]++;
            // TODO: Is this even necessary? Is the contents vector ever accessed before setEncryptedContents is called?
            contents.push_back({id, { std::nullopt, std::nullopt }});
        }
    }
}

void Deck::setEncryptedContents(const std::vector<std::pair<Point, Scalar>>& encryptedDeck) {
    if (encryptedDeck.size() > Constants::MAX_DECK_SIZE) [[unlikely]]
        throw std::logic_error("[Deck::setEncryptedContents] Deck size exceeds maximum");
    contents.clear();
    contents.reserve(encryptedDeck.size());
    for (const auto& [cardPoint, key] : encryptedDeck) {
        contents.push_back({cardPoint, { key, std::nullopt }});
    }
}

/**
 * Called when we want to learn the value of a card in either player's deck. Used for peeking, drawing, etc.
 * Card ID must be supplied if card solely encrypted by opponent.
 * @param index Index of card being accessed
 * @param remoteKey Remote key for this card
 */
void Deck::addOpponentKey(uint8_t index, const Scalar& remoteKey) {
    if (index >= contents.size()) [[unlikely]]
        throw std::logic_error(std::format("[Deck::addOpponentKey] Index '{}' is out of bounds", index));
    if (std::get_if<CardID>(&contents[index].card)) [[unlikely]]
        throw std::logic_error("[Deck::addOpponentKey] Card value already known");

    contents[index].keys.second = remoteKey;
    std::optional<CardID> card;
    // if we have a key for this card (90% of cases)
    if (contents[index].keys.first.has_value())
        card = lookupTable.getCardID(
            decrypt(std::get<Point>(contents[index].card),
                contents[index].keys.first.value(), remoteKey)
        );
    else {
        // card only encrypted by opponent
        card = lookupTable.getCardID(
            decrypt(std::get<Point>(contents[index].card), remoteKey)
        );
        if (plaintextContents.has_value()) plaintextContents.value()[card.value()]++;
    }
    if (!card.has_value()) throw std::runtime_error("[Deck::addOpponentKey] Card failed to decrypt; dud key received");
    contents[index].card = card.value();
}

/**
 * When called midgame, adds a card that both players know the value of (unencrypted). May also be called during
 * verification any time cards are added (as all cards are known).
 * @param id ID of card being added
 * @param index Index of position in deck to add card
 */
void Deck::addUnencryptedCard(CardID id, uint8_t index) {
    if (index > contents.size()) [[unlikely]] throw std::logic_error("[Deck::addUnencryptedCard] Index out of bounds");
    if (contents.size() == Constants::MAX_DECK_SIZE) [[unlikely]]
        throw std::logic_error("[Deck::addUnencryptedCard] Deck is already full");

    contents.insert(contents.begin() + index, {id, { std::nullopt, std::nullopt }, true});
    if (plaintextContents.has_value()) plaintextContents.value()[id]++;
}

/**
 * Adds encrypted card value with no keys. Called for opponent's Brainstorm / Donation effects only.\n
 * @param cardCiphertext Ciphertext of card being added
 * @param index Index of position in deck to add card
 */
void Deck::addCardSolelyEncryptedByOpponent(const Point& cardCiphertext, uint8_t index) {
    if (index > contents.size()) [[unlikely]] throw std::logic_error("[Deck::addCardSolelyEncryptedByOpponent] Index out of bounds");
    if (contents.size() == Constants::MAX_DECK_SIZE) [[unlikely]]
        throw std::logic_error("[Deck::addCardSolelyEncryptedByOpponent] Deck is already full");

    contents.insert(contents.begin() + index, {cardCiphertext, { std::nullopt, std::nullopt }, true});
}

/**
 * Adds a card which we know <b>but that our opponent doesn't</b>. Can be due to friendly Brainstorm, Donation, etc.
 * This card has only one encryption layer on it and it was provided by us.
 * @param id ID of card being added
 * @param localKey Local decryption key
 * @param index Index of position in deck to add card
 */
void Deck::addCardSolelyEncryptedLocally(CardID id, const Scalar& localKey, uint8_t index) {
    if (index > contents.size()) [[unlikely]] throw std::logic_error("[Deck::addCardSolelyEncryptedLocally] Index out of bounds");
    if (contents.size() == Constants::MAX_DECK_SIZE) [[unlikely]]
        throw std::logic_error("[Deck::addCardSolelyEncryptedLocally] Deck is already full");

    contents.insert(contents.begin() + index, {id, { localKey, std::nullopt }});
}

// TODO: Must be updated to return std::expected<CardID, DeckQueryError>, which requires update to C++23
/**
 * Returns the card ID of the top card of the deck, then removes it from the deck.\n
 * Resulting value must be assigned to user's hand; this class is not responsible for that.\n
 * <b>Always removes the top card of this deck if it exists, regardless of if we know its value.</b>
 * @return The card ID of the drawn card, else nullopt if it's unknown or the deck is empty
 */
std::optional<CardID> Deck::draw() {
    if (contents.empty()) return std::nullopt;

    auto card = std::get_if<CardID>(&contents.front().card);
    CardID cardId;
    if (card) {
        cardId = *card;
        if (plaintextContents.has_value()) {
            auto& ptc = plaintextContents.value();
            ptc.at(cardId)--;
            if (ptc.at(cardId) == 0) ptc.erase(cardId);
        }
    }
    contents.erase(contents.begin());
    return card ? std::optional(cardId) : std::nullopt;
}

// TODO: This would *hugely* benefit from returning std::expected<std::vector<CardID>, DeckAccessError>
/**
 * Removes some number of cards from the top of the player's deck. <b>Returned value of this method must be
 * added to the appropriate player's graveyard.</b> Throws if any card is unknown.
 * @param count Number of cards to mill
 * @return Vector of milled card IDs
 */
std::vector<CardID> Deck::mill(uint8_t count) {
    uint8_t cardsToMill = std::min(static_cast<size_t>(count), contents.size());
    std::vector<CardID> milledCards;
    milledCards.reserve(cardsToMill);

    for (int i = 0; i < cardsToMill; i++) {
        auto card = std::get_if<CardID>(&contents[i].card);
        if (!card) throw std::logic_error("[Deck::mill] Attempted to mill unknown card. Ensure cards in range are known before milling");
        milledCards.push_back(*card);
    }
    // This is done outside the first loop so that the deck is not corrupted if card identification fails
    for (int i = 0; i < cardsToMill; i++) removeCardAtIndex(0);
    return milledCards;
}

/**
 * @param index Index of card to remove
 * @return Card ID of removed card if we knew it, else nullopt
 */
std::optional<CardID> Deck::removeCardAtIndex(uint8_t index) {
    if (index >= contents.size()) [[unlikely]] throw std::runtime_error("[Deck::removeCardAtIndex]: Index out of bounds");

    auto card = std::get_if<CardID>(&contents[index].card);
    CardID cardId;
    if (card) cardId = *card;
    contents.erase(contents.begin() + index);
    // This code is currently never called as this function is only called within the context of milling, wherein the
    // values of the cards must be known. Even after erasing the value in the vector, `card` remains a non-null pointer;
    // it is simply a **dangling pointer** that must not be accessed but that can still be checked as null.
    if (card == nullptr) return std::nullopt;
    // If we knew the value of the card, remove it from our plaintext deck contents (if we're tracking it)
    if (plaintextContents.has_value()) {
        auto& ptc = plaintextContents.value();
        if (--ptc.at(cardId) == 0) ptc.erase(cardId);
    }

    return cardId;
}

/**
 * Tags a card as being known by the opponent.
 * @param index Index of card to update
 * @param known Whether the card is known by opponent or not. Defaults to true
 */
void Deck::setKnownToOpponent(uint8_t index, bool known) {
    if (index >= contents.size()) [[unlikely]]
        throw std::logic_error("[Deck::setKnownToOpponent] Index {} is out of bounds");
    contents[index].knownToOpponent = known;
}

void Deck::setKnownToOpponentAtIndices(const std::set<uint8_t> &indices) {
    if (!indices.empty() && *indices.rbegin() >= contents.size()) throw std::runtime_error("[Deck::setKnownToOpponentAtIndices] Index out of bounds");

    for (auto index : indices) {
        setKnownToOpponent(index);
    }
}

/**
 * Disables tracking the deck's contents in the <code>plaintextContents</code> map. Must be called for the remote
 * player's deck during gametime, but not during verification.
 */
void Deck::disableContentsTracking() {
    plaintextContents = std::nullopt;
}

/**
 * @return Card ID if known, else nullopt
 */
std::optional<CardID> Deck::getCardIDAtIndex(uint8_t index) const {
    if (index >= contents.size()) [[unlikely]] return std::nullopt;

    auto card = std::get_if<CardID>(&contents[index].card);
    return card ? std::optional(*card) : std::nullopt;
}

/**
 * Gets the local key for the entry at the given index
 * @param index Index of the card to be accessed
 * @return Local decryption key on success. Nullopt if index out of bounds or if we don't have a local key at the index
 */
std::optional<Scalar> Deck::getLocalKey(uint8_t index) const {
    if (index >= contents.size() || !contents[index].keys.first.has_value()) [[unlikely]] return std::nullopt;
    return contents[index].keys.first.value();
}

/**
 * @return Number of cards in deck
 */
uint8_t Deck::getSize() const {
    return contents.size();
}

/**
 * Gets indices of all cards solely encrypted by our opponent <b>that we don't already know the values of</b>. If this
 * deck belongs to the local player this method must return an empty set before shuffling is possible.
 * @return Set of indices pertaining to cards solely encrypted by our opponent that we don't know the values of
 */
std::set<uint8_t> Deck::getIndicesOfCardsSolelyEncryptedByOpponent() const {
    if (contents.empty()) return {};

    std::set<uint8_t> indices;
    for (uint8_t i = 0; i < contents.size(); ++i) {
        const auto& entry = contents[i];
        // If we don't know the card and have no keys
        if (std::holds_alternative<Point>(entry.card) && !entry.keys.first && !entry.keys.second)
            indices.insert(i);
    }
    return indices;
}

/**
 * Gets indices of all cards solely encrypted by us <b>that our opponent doesn't already know the values of</b>. If this
 * deck belongs to the opponent then this method can be called to get the indices of all cards they need access to
 * before they're able to shuffle their deck.
 * @return Set of indices pertaining to cards solely encrypted by us that our opponent doesn't know the values of
 */
std::set<uint8_t> Deck::getIndicesOfCardsSolelyEncryptedLocally() const {
    if (contents.empty()) return {};

    std::set<uint8_t> indices;
    for (uint8_t i = 0; i < contents.size(); ++i) {
        const auto& [card, keys, knownToOpp] = contents[i];
        // if opponent already knows it, skip
        if (knownToOpp) continue;
        // if we know the card value and have a local key but no remote key, this card was solely encrypted by us
        if (std::get_if<CardID>(&card) && keys.first && !keys.second) indices.insert(i);
    }
    return indices;
}

/**
 * @return The known plaintext contents of the deck. Key = card ID, value = quantity present
 */
std::map<CardID, uint8_t> Deck::getContents() const {
    if (!plaintextContents.has_value()) [[unlikely]] throw std::logic_error("[Deck::getContents] Called on deck that isn't tracking contents");
    return plaintextContents.value();
}

/**
 * @param index Index of card being checked
 * @return True if known to opponent, else false
 */
bool Deck::isKnownToOpponent(uint8_t index) const {
    if (index >= contents.size()) [[unlikely]]
        throw std::runtime_error(std::format("[Deck::isKnownToOpponent] Index out of bounds"));
    return contents[index].knownToOpponent;
}

/**
 * Throws on index out of bounds or missing local key at index.
 * @param indices Indices of cards to get corresponding local keys
 * @return Vector of keys in the order of the given set of indices
 */
std::vector<Scalar> Deck::getLocalKeysAtIndices(const std::set<uint8_t>& indices) const {
    if (!indices.empty() && *indices.rbegin() >= contents.size()) throw std::runtime_error("[Deck::getLocalKeysAtIndices] Index out of bounds");

    std::vector<Scalar> localKeys;
    localKeys.reserve(indices.size());
    for (auto index : indices) {
        if (!contents[index].keys.first.has_value())
            throw std::runtime_error(std::format("[Deck::getLocalKeysAtIndices] No local key at index {}", index));
        localKeys.push_back(contents[index].keys.first.value());
    }
    return localKeys;
}

/**
 * @param indexRange Maximum index to check (defaults to unlimited range). Note: invalid (too high) index range = unlimited range
 * @return Set of indices containing cards with unknown values
 */
std::set<uint8_t> Deck::getIndicesOfLocallyUnknown(std::optional<uint8_t> indexRange) const {
    if (contents.empty()) return {};
    std::set<uint8_t> indices;
    uint8_t maxIndex = indexRange.has_value() ? indexRange.value() : contents.size() - 1;
    maxIndex = std::min(maxIndex, static_cast<uint8_t>(contents.size() - 1)); // guarantee index in bounds
    for (int i = 0; i <= maxIndex; i++) {
        if (std::holds_alternative<Point>(contents[i].card)) indices.insert(i);
    }
    return indices;
}

/**
 * @param indexRange Maximum index to check (defaults to unlimited range). Note: invalid (too high) index range = unlimited range
 * @return Set of indices containing cards with unknown values to the remote player
 */
std::set<uint8_t> Deck::getIndicesOfRemotelyUnknown(std::optional<uint8_t> indexRange) const {
    if (contents.empty()) return {};
    std::set<uint8_t> indices;
    uint8_t maxIndex = indexRange.has_value() ? indexRange.value() : contents.size() - 1;
    maxIndex = std::min(maxIndex, static_cast<uint8_t>(contents.size() - 1)); // guarantee index in bounds
    for (int i = 0; i <= maxIndex; i++) {
        if (!contents[i].knownToOpponent) indices.insert(i);
    }
    return indices;
}

/**
 * Exclusively called when preparing the game verifier. <b>Should never be called during gameplay.</b> Only updates the
 * plaintextContents map, does <b>not</b> update the vector of card entries in this deck.
 * @param newPlaintextContents New plaintext contents of deck
 */
void Deck::v_setPlaintextContents(const std::map<CardID, uint8_t>& newPlaintextContents) {
    plaintextContents = newPlaintextContents;
}

/**
 * Called during the verification phase <b>only</b>. Shuffles the deck using its plaintext contents and the two
 * shuffle seeds used during gameplay.
 * @param ownerSeed Shuffle seed used by this deck's owner
 * @param enemySeed Shuffle seed used by the enemy of this deck's owner
 */
void Deck::v_shuffleWithSeeds(ShuffleSeed ownerSeed, ShuffleSeed enemySeed) {
    if (!plaintextContents.has_value()) throw std::logic_error(
        "[Deck::v_shuffleWithSeeds] Deck not tracking contents during verification"
    );
    if (plaintextContents.value().empty()) return;

    std::vector<CardID> cards;
    for (const auto [id, quantity] : plaintextContents.value()) {
        for (uint8_t i = 0; i < quantity; i++) {
            cards.push_back(id);
        }
    }
    shuffleCards(cards, ownerSeed);
    shuffleCards(cards, enemySeed);

    contents.clear();
    contents.reserve(cards.size());
    for (const auto& cardID : cards) {
        contents.push_back({cardID, { std::nullopt, std::nullopt }});
    }
}
