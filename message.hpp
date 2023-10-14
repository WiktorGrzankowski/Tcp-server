#include <string>
#include <cstdlib>
#include <map>
#include <vector>

#include "err.hpp"

using score_t = uint32_t;
using player_id_t = uint8_t;
using coordinate_t = uint16_t;
using bomb_id_t = uint32_t;


struct HelloMessage {
    std::string server_name;
    uint8_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
};

struct Player {
    std::string name;
    std::string address;
};

struct AcceptedPlayerMessage {
    player_id_t id;
    Player player;
};

using PlayersMap = std::map<player_id_t, Player>;

struct GameStartedMessage {
    PlayersMap players;
};

using ScoresMap = std::map<player_id_t, score_t>;

struct GameEndedMessage {
    ScoresMap scores;
};

struct TurnMessage {
    uint16_t turn;
    std::vector<Event::EventS> events;
};


//2.1. Komunikaty od klienta do serwera
//
//enum ClientMessage {
//    [0] Join { name: String },
//    [1] PlaceBomb,
//    [2] PlaceBlock,
//    [3] Move { direction: Direction },
//}

enum Direction {
    Up = 0,
    Right = 1,
    Down = 2,
    Left = 3
};


struct MessageToSend {
    uint8_t message_code;

    MessageToSend(uint8_t ms_c) : message_code(ms_c) {};
};

struct ClientMessage : MessageToSend {
    ClientMessage(uint8_t ms_c) : MessageToSend(ms_c) {};
};

struct DrawMessage : MessageToSend {
    DrawMessage(uint8_t ms_c) : MessageToSend(ms_c) {};
};

struct Join : ClientMessage {
    std::string name;

    Join(std::string nm) : ClientMessage(0), name(nm) {};
};

struct PlaceBomb : ClientMessage {
    PlaceBomb() : ClientMessage(1) {};
};

struct PlaceBlock : ClientMessage {
    PlaceBlock() : ClientMessage(2) {};
};

struct Move : ClientMessage {
    Direction direction;

    Move(Direction dir) : ClientMessage(3), direction(dir) {};
};

struct Lobby : DrawMessage {
    std::string server_name;
    player_id_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
    PlayersMap players;

    Lobby(uint8_t msC, const std::string &serverName, player_id_t playersCount, uint16_t sizeX, uint16_t sizeY,
          uint16_t gameLength, uint16_t explosionRadius, uint16_t bombTimer, const PlayersMap &players) : DrawMessage(
            msC), server_name(serverName), players_count(playersCount), size_x(sizeX), size_y(sizeY), game_length(
            gameLength), explosion_radius(explosionRadius), bomb_timer(bombTimer), players(players) {}
};
