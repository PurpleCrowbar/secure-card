#include "Network.h"
#include <iostream>
#include <stdexcept>

/**
 * Called by player hosting. Sets up TCP listener on the specified port and accepts connection to connectee
 * @param port Port on which to listen
 */
void Network::host(unsigned short port) {
    if (listener.listen(port) != sf::Socket::Status::Done) {
        throw std::runtime_error("Failed to listen on port " + std::to_string(port));
    }
    std::cout << "[Network] Hosting on port " << port << ", waiting for opponent...\n";

    if (listener.accept(socket) != sf::Socket::Status::Done) {
        throw std::runtime_error("Failed to accept incoming connection");
    }
    listener.close();
    std::cout << "[Network] Opponent connected!\n";
}

void Network::connect(const std::string& address, unsigned short port) {
    auto ip = sf::IpAddress::resolve(address);
    if (!ip.has_value()) {
        throw std::runtime_error("Invalid address: " + address);
    }
    std::cout << "[Network] Connecting to " << address << ":" << port << "...\n";

    if (socket.connect(ip.value(), port) != sf::Socket::Status::Done) {
        throw std::runtime_error("Failed to connect, is host running?");
    }
    std::cout << "[Network] Connected!\n";
}

void Network::disconnect() {
    socket.disconnect();
}

/**
 * TcpSocket::send already <i>does</i> send all. This method is just a little wrapper that lets us throw an
 * exception on failure
 * @param data Data to send
 * @param len Length of data to send
 */
void Network::sendAll(const void* data, std::size_t len) {
    auto status = socket.send(data, len);
    if (status != sf::Socket::Status::Done) {
        throw std::runtime_error("send() failed, connection lost");
    }
}

/**
 * Because TcpSocket::receive can return partial data, we need to loop it until we've read all the bytes we expect
 * @param buffer Buffer to read data into
 * @param len Total bytes of data to read into buffer
 */
void Network::receiveAll(void* buffer, std::size_t len) {
    // cast empty buffer to an empty array of bytes
    auto* ptr = static_cast<char*>(buffer);
    std::size_t totalReceived = 0;

    while (totalReceived < len) {
        std::size_t received = 0;
        auto status = socket.receive(ptr + totalReceived, len - totalReceived, received);
        if (status == sf::Socket::Status::Done || status == sf::Socket::Status::Partial) totalReceived += received;
        else throw std::runtime_error("receive() failed, connection lost");
    }
}

void Network::sendUint8(uint8_t val) {
    sendAll(&val, 1);
}

[[nodiscard]] uint8_t Network::receiveUint8() {
    uint8_t val;
    receiveAll(&val, 1);
    return val;
}

void Network::sendUint16(uint16_t val) {
    // big-endian encoding so we don't need platform-specific htons
    uint8_t bytes[2] = {
        static_cast<uint8_t>((val >> 8) & 0xFF), // high byte first
        static_cast<uint8_t>(val & 0xFF) // low byte second
    };
    sendAll(bytes, 2);
}

[[nodiscard]] uint16_t Network::receiveUint16() {
    uint8_t bytes[2];
    receiveAll(bytes, 2);
    return static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
}

void Network::sendUint32(uint32_t val) {
    uint8_t bytes[4] = {
        static_cast<uint8_t>((val >> 24) & 0xFF),
        static_cast<uint8_t>((val >> 16) & 0xFF),
        static_cast<uint8_t>((val >> 8) & 0xFF),
        static_cast<uint8_t>(val & 0xFF),
    };
    sendAll(bytes, 4);
}

[[nodiscard]] uint32_t Network::receiveUint32() {
    uint8_t bytes[4];
    receiveAll(bytes, 4);
    // read each of the four bytes into a single uint32_t
    return static_cast<uint32_t>(bytes[0]) << 24
         | static_cast<uint32_t>(bytes[1]) << 16
         | static_cast<uint32_t>(bytes[2]) << 8
         | static_cast<uint32_t>(bytes[3]);
}

// Points and Scalars are just 32 byte arrays (in ristretto255)
void Network::sendPoint(const Point& p) {
    sendAll(p.data(), POINT_BYTES);
}

[[nodiscard]] Point Network::receivePoint() {
    Point p;
    receiveAll(p.data(), POINT_BYTES);
    return p;
}

void Network::sendScalar(const Scalar& s) {
    sendAll(s.data(), SCALAR_BYTES);
}

[[nodiscard]] Scalar Network::receiveScalar() {
    Scalar s;
    receiveAll(s.data(), SCALAR_BYTES);
    return s;
}

/**
 * Receives any number of points - first byte of received data expected to be object count
 * @return Vector of points read in
 */
[[nodiscard]] std::vector<Point> Network::receivePoints() {
    uint8_t count = receiveUint8();
    std::vector<Point> result;
    result.reserve(count);
    for (uint8_t i = 0; i < count; i++) {
        result.push_back(receivePoint());
    }
    return result;
}

/**
 * Sends a vector of keys (scalars) to the other player. First byte is the number of scalars sent, rest of
 * data is the 32 byte scalars themselves.
 * @param scalars Vector of keys (scalars)
 */
void Network::sendScalars(const std::vector<Scalar>& scalars) {
    sendUint8(static_cast<uint8_t>(scalars.size()));
    for (const auto& s : scalars) {
        sendScalar(s);
    }
}

[[nodiscard]] std::vector<Scalar> Network::receiveScalars() {
    uint8_t count = receiveUint8();
    std::vector<Scalar> result;
    result.reserve(count);
    for (uint8_t i = 0; i < count; i++) {
        result.push_back(receiveScalar());
    }
    return result;
}

void Network::sendDeckHash(const DeckHash& hash) {
    sendAll(hash.data(), DECK_HASH_BYTES);
}

[[nodiscard]] DeckHash Network::receiveDeckHash() {
    DeckHash hash;
    receiveAll(hash.data(), DECK_HASH_BYTES);
    return hash;
}

void Network::sendDeckHashKey(const DeckHashKey& key) {
    sendAll(key.data(), DECK_HASH_KEY_BYTES);
}

[[nodiscard]] DeckHashKey Network::receiveDeckHashKey() {
    DeckHashKey key;
    receiveAll(key.data(), DECK_HASH_KEY_BYTES);
    return key;
}

/**
 * Sends a vector of shuffle seeds. First byte of data is number of seeds being sent, rest of the data is the
 * 32 bit seeds themselves.
 * @param seeds Vector of shuffle seeds
 */
void Network::sendShuffleSeeds(const std::vector<ShuffleSeed>& seeds) {
    sendUint8(static_cast<uint8_t>(seeds.size()));
    for (const auto& seed : seeds) {
        sendUint32(seed);
    }
}

[[nodiscard]] std::vector<ShuffleSeed> Network::receiveShuffleSeeds() {
    uint8_t count = receiveUint8();
    std::vector<ShuffleSeed> result;
    result.reserve(count);
    for (uint8_t i = 0; i < count; i++) {
        result.push_back(receiveUint32());
    }
    return result;
}

/**
 * Sends plaintext deck contents. First byte is number of different card IDs; rest of data is 3-byte chunks which
 * contain the card ID (2 bytes) and the quantity present in the deck (1 byte).
 * @param contents Deck contents in map form
 */
void Network::sendDeckContents(const std::map<CardID, uint8_t>& contents) {
    sendUint8(static_cast<uint8_t>(contents.size()));
    for (const auto& [id, qty] : contents) {
        sendUint16(static_cast<uint16_t>(id));
        sendUint8(qty);
    }
}

[[nodiscard]] std::map<CardID, uint8_t> Network::receiveDeckContents() {
    uint8_t count = receiveUint8();
    std::map<CardID, uint8_t> contents;
    for (uint8_t i = 0; i < count; i++) {
        auto id = static_cast<CardID>(receiveUint16());
        uint8_t qty = receiveUint8();
        contents[id] = qty;
    }
    return contents;
}

/**
 * Sends a vector of subvectors which contain commitment keys. Each subvector pertains to a single commitment.
 * First byte is the number of subvectors. See sendScalars method for rest of data transmitted.
 * This function is called at the end of the game, right before verification begins.
 * @param commitmentKeys Vector of subvectors containing commitment keys. Each subvector contains the keys for one commitment.
 */
void Network::sendCommitmentKeys(const std::vector<std::vector<Scalar>>& commitmentKeys) {
    sendUint8(static_cast<uint8_t>(commitmentKeys.size()));
    for (const auto& inner : commitmentKeys) {
        sendScalars(inner);
    }
}

[[nodiscard]] std::vector<std::vector<Scalar>> Network::receiveCommitmentKeys() {
    uint8_t outerCount = receiveUint8();
    std::vector<std::vector<Scalar>> result;
    result.reserve(outerCount);
    for (uint8_t i = 0; i < outerCount; i++) {
        result.push_back(receiveScalars());
    }
    return result;
}

void Network::sendPacketType(PacketType type) {
    sendUint8(static_cast<uint8_t>(type));
}

[[nodiscard]] PacketType Network::receivePacketType() {
    return static_cast<PacketType>(receiveUint8());
}
