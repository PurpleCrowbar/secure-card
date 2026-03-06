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

uint8_t Network::receiveUint8() {
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

uint16_t Network::receiveUint16() {
    uint8_t bytes[2];
    receiveAll(bytes, 2);
    return static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
}

// Points and Scalars are just 32 byte arrays (in ristretto255)
void Network::sendPoint(const Point& p) {
    sendAll(p.data(), POINT_BYTES);
}

Point Network::receivePoint() {
    Point p;
    receiveAll(p.data(), POINT_BYTES);
    return p;
}

void Network::sendScalar(const Scalar& s) {
    sendAll(s.data(), SCALAR_BYTES);
}

Scalar Network::receiveScalar() {
    Scalar s;
    receiveAll(s.data(), SCALAR_BYTES);
    return s;
}

/**
 * Sends number of bytes followed by data for each point
 * @param points Points to send
 */
void Network::sendPoints(const std::vector<Point>& points) {
    sendUint8(static_cast<uint8_t>(points.size()));
    for (const auto& p : points) {
        sendPoint(p);
    }
}

/**
 * Receives any number of points - first byte of received data expected to be object count
 * @return Vector of points read in
 */
std::vector<Point> Network::receivePoints() {
    uint8_t count = receiveUint8();
    std::vector<Point> result;
    result.reserve(count);
    for (uint8_t i = 0; i < count; i++) {
        result.push_back(receivePoint());
    }
    return result;
}

void Network::sendScalars(const std::vector<Scalar>& scalars) {
    sendUint8(static_cast<uint8_t>(scalars.size()));
    for (const auto& s : scalars) {
        sendScalar(s);
    }
}

std::vector<Scalar> Network::receiveScalars() {
    uint8_t count = receiveUint8();
    std::vector<Scalar> result;
    result.reserve(count);
    for (uint8_t i = 0; i < count; i++) {
        result.push_back(receiveScalar());
    }
    return result;
}

void Network::sendPacketType(PacketType type) {
    sendUint8(static_cast<uint8_t>(type));
}

PacketType Network::receivePacketType() {
    return static_cast<PacketType>(receiveUint8());
}
