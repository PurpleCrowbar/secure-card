#include "Deck.h"
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
    for (const auto& [id, quantity] : plaintextDeckContents) {
        for (uint8_t i = 0; i < quantity; ++i) {
            plaintextContents[id]++;
            // TODO: Is this even necessary? Is the contents vector ever accessed before setEncryptedContents is called?
            contents.push_back({id, { std::nullopt, std::nullopt }});
        }
    }
}

void Deck::setEncryptedContents(const std::vector<std::pair<Point, Scalar>>& encryptedDeck) {
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
 * @return True if successful; false if index out of bounds, card value already known
 */
bool Deck::addOpponentKey(uint8_t index, const Scalar& remoteKey) {
    if (index >= contents.size() || std::get_if<CardID>(&contents[index].card)) [[unlikely]] return false;

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
        plaintextContents[card.value()]++;
    }
    if (!card.has_value()) return false;
    contents[index].card = card.value();
    return true;
}

/**
 * When called midgame, adds a card that both players know the value of (unencrypted). May also be called during
 * verification any time cards are added (as all cards are known).
 * @param id
 * @param index
 * @return
 */
bool Deck::addUnencryptedCard(CardID id, uint8_t index) {
    if (index >= contents.size()) [[unlikely]] return false;

    contents.insert(contents.begin() + index, {id, { std::nullopt, std::nullopt }, true});
    plaintextContents[id]++;
    return true;
}

/**
 * Adds encrypted card value with no keys. Called for opponent's Brainstorm / Donation effects only.\n
 * @param cardCiphertext Ciphertext of card being added
 * @param index Where in deck to add card
 * @returns True if successful, false if index out of bounds
 */
bool Deck::addCardSolelyEncryptedByOpponent(const Point &cardCiphertext, uint8_t index) {
    if (index > contents.size()) [[unlikely]] return false;

    contents.insert(contents.begin() + index, {cardCiphertext, { std::nullopt, std::nullopt }, true});
    return true;
}

/**
 * Adds a card which we know <b>but that our opponent doesn't</b>. Can be due to friendly Brainstorm, Donation, etc.
 * This card has only one encryption layer on it and it was provided by us.
 * @param id ID of card being added
 * @param localKey Local decryption key
 * @param index Where in deck to add card
 * @return True if successful, false if index out of bounds
 */
bool Deck::addCardSolelyEncryptedLocally(CardID id, const Scalar &localKey, uint8_t index) {
    if (index > contents.size()) [[unlikely]] return false;

    contents.insert(contents.begin() + index, {id, { localKey, std::nullopt }});
    return true;
}

// TODO: Must be updated to return enum with possible vals { Success, InvalidKeyCount, DeckCorrupted }
/**
 * Fully decrypts the deck. <b>May only be used on an unmodified, newly-shuffled deck during the verification phase.</b>
 * Using this function on a deck that doesn't meet that criteria will result in it becoming corrupted.
 * @param remoteKeys Queue of keys from the remote player.
 * @return True if successfully decrypted whole deck, else false
 */
bool Deck::fullyDecrypt(const std::vector<Scalar>& remoteKeys) {
    if (remoteKeys.size() != contents.size()) return false;

    for (size_t i = 0; i < contents.size(); i++) {
        auto& cardEntry = contents[i];
        cardEntry.keys.second = remoteKeys[i];

        Point pointRep = std::get<Point>(cardEntry.card);
        auto cardID = lookupTable.getCardID(
            decrypt(pointRep, cardEntry.keys.first.value(), cardEntry.keys.second.value())
        );
        if (!cardID.has_value()) return false;
        cardEntry.card = cardID.value();
    }
    return true;
}

// TODO: Must be updated to return std::expected<CardID, DeckQueryErrorEnum>, which requires update to C++23
/**
 * Returns the card ID of the top card of the deck, then removes it from the deck.\n
 * Resulting value must be assigned to user's hand; this class is not responsible for that.
 * @return The card ID of the drawn card, else nullopt if it's unknown or the deck is empty
 */
std::optional<CardID> Deck::draw() {
    if (contents.empty()) return std::nullopt;

    auto card = std::get_if<CardID>(&contents.front().card);
    if (!card) return std::nullopt;
    CardID cardId = *card;
    contents.erase(contents.begin());
    plaintextContents.at(cardId)--;
    if (plaintextContents.at(cardId) == 0) plaintextContents.erase(cardId);
    return cardId;
}

// TODO: improve return type, function may return nullopt for reasons other than not knowing card ID (e.g., index out of bounds)
/**
 * @param index Index of card to remove
 * @return Card ID of removed card if we knew it, else nullopt. Also nullopt if index out of bounds
 */
std::optional<CardID> Deck::removeCardAtIndex(uint8_t index) {
    // TODO: This and other out of bounds access attempts should throw exception; it's always bugged behaviour
    if (index >= contents.size()) [[unlikely]] return std::nullopt;

    auto card = std::get_if<CardID>(&contents[index].card);
    CardID cardId;
    bool known = false;
    if (card) {
        cardId = *card;
        known = true;
    }
    contents.erase(contents.begin() + index);
    if (!known) return std::nullopt;
    // If we knew the value of the card, remove it from our plaintext deck contents
    plaintextContents.at(cardId)--;
    if (plaintextContents.at(cardId) == 0) plaintextContents.erase(cardId);
    return cardId;
}

/**
 * Tags a card as being known by the opponent.
 * @param index Index of card to update
 * @param known Whether the card is known by opponent or not. Defaults to true
 * @return True if successful; false if index out of bounds or card ID unknown
 */
bool Deck::setKnownToOpponent(uint8_t index, bool known) {
    if (index >= contents.size()) [[unlikely]] return false;
    contents[index].knownToOpponent = known;
    return true;
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
    return plaintextContents;
}

// TODO: Need to update this to std::expected<bool, DeckQueryError>. Return value of false currently means two completely different things
/**
 * @param index Index of card being checked
 * @return True if known to opponent, else false. Also returns false if index out of bounds
 */
bool Deck::isKnownToOpponent(uint8_t index) const {
    if (index >= contents.size()) [[unlikely]] return false;
    return contents[index].knownToOpponent;
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
 * @return True if successful, false if plaintextContents empty
 */
bool Deck::v_shuffleWithSeeds(ShuffleSeed ownerSeed, ShuffleSeed enemySeed) {
    if (plaintextContents.empty()) return false;

    std::vector<CardID> cards;
    for (const auto& [id, quantity] : plaintextContents) {
        for (uint8_t i = 0; i < quantity; ++i) {
            cards.push_back(id);
        }
    }
    shuffleDeck(cards, ownerSeed);
    shuffleDeck(cards, enemySeed);

    contents.clear();
    contents.reserve(cards.size());
    for (const auto& cardID : cards) {
        contents.push_back({cardID, { std::nullopt, std::nullopt }});
    }
    return true;
}
