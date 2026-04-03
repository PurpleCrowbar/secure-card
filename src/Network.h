#pragma once
#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <SFML/Network.hpp>
#include "Cryptosystem.h"
#include "DeckCommitment.h"
#include "Cards/CardID.h"

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
    [[nodiscard]] uint8_t receiveUint8();
    void sendUint16(uint16_t val);
    [[nodiscard]] uint16_t receiveUint16();
    void sendUint32(uint32_t val);
    [[nodiscard]] uint32_t receiveUint32();

    void sendPoint(const Point& p);
    [[nodiscard]] Point receivePoint();
    void sendScalar(const Scalar& s);
    [[nodiscard]] Scalar receiveScalar();
    /**
     * Sends number of bytes followed by data for each point.
     * Programmer's note: Templates must be defined in headers. This method receives a sized range of Points.
     * @param points Points to send
     */
    template<std::ranges::sized_range R>
    requires std::same_as<std::ranges::range_value_t<R>, Point>
    void sendPoints(R&& points) {
        sendUint8(static_cast<uint8_t>(std::ranges::size(points)));
        for (const auto& point : points) {
            sendPoint(point);
        }
    }
    [[nodiscard]] std::vector<Point> receivePoints();
    void sendScalars(const std::vector<Scalar>& scalars);
    std::vector<Scalar> receiveScalars();

    // Post-game verification helpers
    void sendDeckHash(const DeckHash& hash);
    [[nodiscard]] DeckHash receiveDeckHash();
    void sendDeckHashKey(const DeckHashKey& key);
    [[nodiscard]] DeckHashKey receiveDeckHashKey();
    void sendShuffleSeeds(const std::vector<ShuffleSeed>& seeds);
    [[nodiscard]] std::vector<ShuffleSeed> receiveShuffleSeeds();
    void sendDeckContents(const std::map<CardID, uint8_t>& contents);
    [[nodiscard]] std::map<CardID, uint8_t> receiveDeckContents();
    void sendCommitmentKeys(const std::vector<std::vector<Scalar>>& commitmentKeys);
    [[nodiscard]] std::vector<std::vector<Scalar>> receiveCommitmentKeys();

    // most commonly used in the Game loop
    void sendPacketType(PacketType type);
    [[nodiscard]] PacketType receivePacketType();

private:
    sf::TcpListener listener;
    sf::TcpSocket socket;
};
