#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <SFML/Network.hpp>
#include "Cryptosystem.h"

enum class PacketType : uint8_t {
    PLAY_CARD = 1,  // uint16_t card ID always comes after
    PERMITTED = 2,
    DENIED = 3,
    END_TURN = 4,
    CONCEDE = 5,
};

class Network {
public:
    Network() = default;
    ~Network() = default;

    // Connection setup; call one or the other, not both
    void host(unsigned short port);
    void connect(const std::string& address, unsigned short port);
    void disconnect();

    void sendAll(const void* data, std::size_t len);
    void receiveAll(void* buffer, std::size_t len);

    void sendUint8(uint8_t val);
    uint8_t receiveUint8();
    void sendUint16(uint16_t val);
    uint16_t receiveUint16();

    void sendPoint(const Point& p);
    Point receivePoint();
    void sendScalar(const Scalar& s);
    Scalar receiveScalar();
    void sendPoints(const std::vector<Point>& points);
    std::vector<Point> receivePoints();
    void sendScalars(const std::vector<Scalar>& scalars);
    std::vector<Scalar> receiveScalars();

    // most commonly used in the Game loop
    void sendPacketType(PacketType type);
    PacketType receivePacketType();

private:
    sf::TcpListener listener;
    sf::TcpSocket socket;
};
