#include "Deck.h"
#include "../Cryptosystem.h"

Deck::Deck(LookupTable &lookupTable) : lookupTable(lookupTable) {}

Deck::Deck(const std::vector<std::pair<Point, Scalar>>& encryptedDeck, LookupTable& lookupTable) : lookupTable(lookupTable) {
    contents.reserve(encryptedDeck.size());
    for (auto cardKeyPair : encryptedDeck) {
        contents.push_back({cardKeyPair.first, { cardKeyPair.second, std::nullopt }});
    }
}

Deck::Deck(const std::vector<CardID>& plaintextDeck, LookupTable& lookupTable) : lookupTable(lookupTable) {
    contents.reserve(plaintextDeck.size());
    for (const auto& cardID : plaintextDeck) {
        contents.push_back({cardID, { std::nullopt, std::nullopt }});
    }
}

void Deck::setPlaintextContents(const std::unordered_map<CardID, uint8_t>& deckContents) {
    plaintextContents = deckContents;
}

void Deck::setPlaintextContents(const std::vector<CardID>& deckContents) {
    for (const auto& card : deckContents) {
        plaintextContents[card]++;
    }
}

/**
 * Called when we want to learn the value of a card in either player's deck. Used for peeking, drawing, etc.
 * Card ID must be supplied if card solely encrypted by opponent.
 * @param index Index of card being accessed
 * @param remoteKey Remote key for this card
 * @param id (Optional) The plaintext ID for this card if it was solely encrypted by our opponent
 * @return True if successful; false if index out of bounds, card value already known, or card ID included despite card not being solely encrypted by opponent
 */
bool Deck::addOpponentKey(uint8_t index, const Scalar& remoteKey, std::optional<CardID> id) {
    if (index >= contents.size() || std::get_if<CardID>(&contents[index].card)) [[unlikely]] return false;
    if (id.has_value() && !getIndicesOfCardsSolelyEncryptedByOpponent().contains(index)) [[unlikely]] return false;

    contents[index].keys.second = remoteKey;
    std::optional<CardID> card;
    // If this card was solely encrypted by our opponent
    if (id.has_value()) {
        // Add the given ID to our lookup table before continuing
        lookupTable.addCardID(id.value());
        card = lookupTable.getCardID(
        decrypt(std::get<Point>(contents[index].card), remoteKey)
        );
        if (!card.has_value() || card.value() != id) return false;
    }
    // if we've encryped this card too (this is the case 95% of the time)
    else {
        card = lookupTable.getCardID(
        decrypt(std::get<Point>(contents[index].card),
            contents[index].keys.first.value(), remoteKey)
        );
        if (!card.has_value()) return false;
    }
    contents[index].card = card.value();
    plaintextContents[card.value()]++;
    return true;
}

/**
 * When called midgame, adds a card that both players know the value of (unencrypted). Also called during verification
 * any time cards are added (as all cards are known).
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
 * Index 0 = put card on top, 1 = second from top, etc.
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

    contents.insert(contents.begin() + index, {id, { localKey, std::nullopt }, false, true});
    return true;
}

// TODO: Must be updated to return enum with possible vals { Success, InvalidKeyCount, DeckCorrupted }
/**
 * Fully decrypts the deck. <b>May only be used on an unmodified, newly-shuffled deck during the verification phase.</b>
 * Using this function on a deck that doesn't meet that criteria will result in it becoming corrupted.
 * @param remoteKeys Queue of keys from the remote player.
 * @return True if successfully decrypted whole deck, else false
 */
bool Deck::fullyDecrypt(std::queue<Scalar>& remoteKeys) {
    if (remoteKeys.size() != contents.size()) return false;

    for (auto& cardEntry : contents) {
        cardEntry.keys.second = remoteKeys.front();
        remoteKeys.pop();

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
 * @return Card ID of removed card if we knew it, else nullopt. Also nullopt if action failed; verify by checking deck size
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
 * Gets the data the opponent needs to read a card. The vast majority of the time this will just be a key, but in the
 * case of cards solely encrypted locally, the opponent will need the plaintext card ID, too. This is because the card
 * ID may be unknown to them (read: not present in their lookup table).
 * @param index Index of the card the opponent wants to access
 * @return Local decryption key (always) and Card ID (if necessary) on success. Nullopt if index out of bounds or if we don't have any local key to send
 */
std::optional<std::pair<Scalar, std::optional<CardID>>> Deck::getCardDataForOpponent(uint8_t index) const {
    if (index >= contents.size() || !contents[index].keys.first.has_value()) [[unlikely]] return std::nullopt;

    return std::make_pair(
        contents[index].keys.first.value(),
        contents[index].solelyEncryptedByLocalPlayer ? std::optional(std::get<CardID>(contents[index].card)) : std::nullopt
    );
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
        if (contents[i].knownToOpponent) indices.insert(i);
    }
    return indices;
}

/**
 * @return The known plaintext contents of the deck. Key = card ID, value = quantity present
 */
std::unordered_map<CardID, uint8_t> Deck::getContents() const {
    return plaintextContents;
}
