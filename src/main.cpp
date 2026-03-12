#include <iostream>
#include <string>
#include <vector>
#include <SFML/Network.hpp>
#include <sodium.h>
#include "Cryptosystem.h"
#include "Game/Game.h"

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
    std::vector<CardID> selectedDeck;
    for (int i = 0; i < 10; i++) {
        selectedDeck.push_back(CardID::LIGHTNING_BOLT);
    }
    for (int i = 0; i < 5; i++) {
        selectedDeck.push_back(CardID::DISORGANIZE);
    }

    Game game(network, isHost ? PlayerID::ONE : PlayerID::TWO, selectedDeck);
    game.run();

    network.disconnect();
    return 0;
}
