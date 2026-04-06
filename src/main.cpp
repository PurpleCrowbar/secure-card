#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <SFML/Network.hpp>
#include <sodium.h>
#include "Cryptosystem.h"
#include "Game/Game.h"
#include "GUI/GameBridge.h"
#include "GUI/GameRenderer.h"

int main(const int argc, char** argv) {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialise libsodium\n";
        return 1;
    }
    if (argc != 2 || (std::string(argv[1]) != "host" && std::string(argv[1]) != "client")) {
        std::cerr << "Usage: " << argv[0] << " <host|client>\n";
        return 1;
    }
    bool isHost = std::string(argv[1]) == "host";

    LookupTable::instance(); // generate lookup table
    Network network;
    constexpr uint16_t PORT = 54321; // some arbitrary port
    if (isHost) network.host(PORT);
    else network.connect("127.0.0.1", PORT);

    // TODO: Implement DeckSelector which allows player to get deck from some menu
    std::map<CardID, uint8_t> selectedDeck;
    selectedDeck[CardID::LIGHTNING_BOLT] = 5;
    selectedDeck[CardID::DISORGANIZE] = 5;
    selectedDeck[CardID::SPECTRAL_WAIL] = 5;

    GameBridge bridge;
    Game game(network, isHost ? PlayerID::ONE : PlayerID::TWO, selectedDeck);
    game.setBridge(&bridge); // TODO: could be part of constructor but very low priority

    // game logic runs on separate thread so UI can render despite network I/O blocking
    std::thread gameThread([&game]() {
        game.run();
    });

    // rendering with SFML occurs on the main thread
    GameRenderer renderer(bridge);
    renderer.run();

    gameThread.join();
    network.disconnect();
    return 0;
}
