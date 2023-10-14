#include <cstdint>
#include <vector>
#include <string>

#define BUFFER_SIZE 4096
#define UDP_BUFFER_SIZE 16

uint32_t buffer_index = 0;
char shared_buffer[BUFFER_SIZE];
char udp_shared_buffer[UDP_BUFFER_SIZE];

#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3

#define LOBBY_MESSAGE_TO_GUI 0
#define GAME_MESSAGE_TO_GUI 1

#define HELLO_CODE 0
#define ACCEPTED_PLAYER_CODE 1
#define GAME_STARTED_CODE 2
#define TURN_CODE 3
#define GAME_ENDED_CODE 4

#define BOMB_PLACED_EVENT_CODE 0
#define BOMB_EXPLODED_EVENT_CODE 1
#define PLAYER_MOVED_EVENT_CODE 2
#define BLOCK_PLACED_EVENT_CODE 3

#define PLACE_BOMB_GUI_MESSAGE_CODE 0
#define PLACE_BLOCK_GUI_MESSAGE_CODE 1
#define MOVE_GUI_MESSAGE_CODE 2

#define SEND_JOIN_CODE 0
#define SEND_PLACE_BOMB_CODE 1
#define SEND_PLACE_BLOCK_CODE 2
#define SEND_MOVE_CODE 3


using score_t = uint32_t;
using player_id_t = uint8_t;
using coordinate_t = uint16_t;
using bomb_id_t = uint32_t;
using score_t = uint32_t;

struct Player;
struct GameInfo;
struct Position;
using PlayersMap = std::map<player_id_t, Player>;
using ScoresMap = std::map<player_id_t, score_t>;
using PositionsMap = std::map<player_id_t, Position>;

struct Position {
    Position() {}

    Position(uint16_t x, uint16_t y) : x(x), y(y) {};

    coordinate_t x;
    coordinate_t y;

    bool operator<(const Position &position) const {
        return this->x < position.x || (this->x == position.x && this->y < position.y);
    }
};

struct Bomb {
    Bomb() {};

    Bomb(const Position &position, uint16_t timer) : position(position), timer(timer) {};

    Position position;
    uint16_t timer;

};

namespace Event {
    struct EventS {
        virtual void update_game_info(GameInfo &game_info) = 0;
    };

    struct BombPlaced : EventS {
        bomb_id_t id;
        Position position;

        BombPlaced(bomb_id_t i, Position position) : id(i), position(position) {};

        void update_game_info(GameInfo &game_info) override;
    };

    struct BombExploded : EventS {
        bomb_id_t id;
        std::vector <player_id_t> robots_destroyed;
        std::vector <Position> blocks_destroyed;

        BombExploded(bomb_id_t id, const std::vector <player_id_t> &robotsDestroyed,
                     const std::vector <Position> &blocksDestroyed) : id(id), robots_destroyed(
                robotsDestroyed),
                                                                      blocks_destroyed(
                                                                              blocksDestroyed) {};

        void update_game_info(GameInfo &game_info) override;

        void
        update_game_info(GameInfo &game_info, std::set <player_id_t> &players_destroyed_this_turn,
                         std::set <Position> &blocks_destroyed_this_turn);

        void calc_explosion(GameInfo &game_info, uint16_t &x_axis, uint16_t &y_axis);
    };

    struct PlayerMoved : EventS {
        player_id_t id;
        Position position;

        PlayerMoved(player_id_t id, const Position &position) : id(id), position(position) {};

        void update_game_info(GameInfo &game_info) override;
    };

    struct BlockPlaced : EventS {
        Position position;

        BlockPlaced(const Position &position) : position(position) {};

        void update_game_info(GameInfo &game_info) override;
    };

}

struct Player {
    std::string name;
    std::string address;
};

namespace Message {
    struct HelloMessage {
        std::string server_name;
        uint8_t players_count;
        uint16_t size_x;
        uint16_t size_y;
        uint16_t game_length;
        uint16_t explosion_radius;
        uint16_t bomb_timer;
    };

    struct AcceptedPlayerMessage {
        player_id_t id;
        Player player;
    };

    struct GameStartedMessage {
        PlayersMap players;
    };

    struct GameEndedMessage {
        ScoresMap scores;
    };

    struct TurnMessage {
        uint16_t turn;
        std::vector <std::shared_ptr<Event::BombExploded>> explosions;
        std::vector <std::shared_ptr<Event::EventS>> other_events;
        std::set <player_id_t> robots_destroyed_this_turn;
        std::set <Position> blocks_destroyed_this_turn;
    };
}

struct GameInfo {
    bool in_lobby;
    bool join_sent;
    std::string my_player_name;
    std::string server_name;
    uint8_t players_count;
    coordinate_t size_x;
    coordinate_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
    PlayersMap players;
    PositionsMap player_positions;
    std::set <Position> blocks;
    std::map <uint32_t, Bomb> bombs;
    std::set <Position> explosions;
    ScoresMap scores;
    uint16_t turn;

    void update_with_hello_info(Message::HelloMessage &message) {
        in_lobby = true;
        join_sent = false;
        server_name = message.server_name;
        players_count = message.players_count;
        size_x = message.size_x;
        size_y = message.size_y;
        game_length = message.game_length;
        explosion_radius = message.explosion_radius;
        bomb_timer = message.bomb_timer;
    }

    void update_with_accepted_player_info(Message::AcceptedPlayerMessage &message) {
        players[message.id] = message.player;
    }

    void update_with_game_started_info(Message::GameStartedMessage &message) {
        players = message.players;
        turn = 0;
        in_lobby = false;

        for (auto &player: players)
            scores[player.first] = 0;
    }

    void update_with_game_ended_info() {
        in_lobby = true;
        join_sent = false;
        players.clear();
        blocks.clear();
        bombs.clear();
        explosions.clear();
        player_positions.clear();
    }

    void update_with_turn_info(Message::TurnMessage &message) {
        turn = message.turn;

        for (auto &bomb: bombs)
            bomb.second.timer--;

        for (auto &event: message.explosions)
            event->update_game_info(*this, message.robots_destroyed_this_turn,
                                    message.blocks_destroyed_this_turn);

        for (auto &destroyed_block_position: message.blocks_destroyed_this_turn)
            this->blocks.erase(destroyed_block_position);

        for (auto &destroyed_robot_id: message.robots_destroyed_this_turn)
            this->scores[destroyed_robot_id]++;

        for (auto &event: message.other_events)
            event->update_game_info(*this);
    }
};

//GameInfo global_game_info;

void Event::BlockPlaced::update_game_info(GameInfo &game_info) {
    game_info.blocks.insert(this->position);
}

void Event::PlayerMoved::update_game_info(GameInfo &game_info) {
    game_info.player_positions[this->id] = this->position;
}

void Event::BombPlaced::update_game_info(GameInfo &game_info) {
    game_info.bombs.insert({this->id, Bomb(this->position, game_info.bomb_timer)});
}

void Event::BombExploded::calc_explosion(GameInfo &game_info, uint16_t &x_axis, uint16_t &y_axis) {
    for (uint16_t i = y_axis;
         i < y_axis + game_info.explosion_radius + 1 && i < game_info.size_y; ++i) {
        if (game_info.blocks.contains({x_axis, y_axis})) {
            game_info.explosions.insert({x_axis, y_axis});
            return;
        }
        game_info.explosions.insert({x_axis, i});
        if (game_info.blocks.contains({x_axis, i}))
            break;
    }
    for (uint16_t i = y_axis - 1;
         i > y_axis - game_info.explosion_radius - 1 && y_axis != 0; --i) {
        game_info.explosions.insert({x_axis, i});
        if (game_info.blocks.contains({x_axis, i}) || i == 0)
            break;
    }
    for (uint16_t i = x_axis + 1;
         i < x_axis + game_info.explosion_radius + 1 && i < game_info.size_x; ++i) {
        game_info.explosions.insert({i, y_axis});
        if (game_info.blocks.contains({i, y_axis}))
            break;
    }
    for (uint16_t i = x_axis - 1;
         i > x_axis - game_info.explosion_radius - 1 && x_axis != 0; --i) {
        game_info.explosions.insert({i, y_axis});
        if (game_info.blocks.contains({i, y_axis}) || i == 0)
            break;
    }
}

// It's empty, but it's required to override this function in order to save Event::EventS as a virtual base class.
void Event::BombExploded::update_game_info(GameInfo &game_info) {
    game_info.in_lobby = false;
}

void
Event::BombExploded::update_game_info(GameInfo &game_info,
                                      std::set <player_id_t> &players_destroyed_this_turn,
                                      std::set <Position> &blocks_destroyed_this_turn) {
    this->calc_explosion(game_info, game_info.bombs[this->id].position.x,
                         game_info.bombs[this->id].position.y);
    game_info.bombs.erase(this->id);
    for (auto &destroyed_block_position: this->blocks_destroyed) {
        // Don't destroy the block yet, as it could change the look of the explosion and it's effects.
        // Add the block to a list of block that will be destroyed at the very end.
        if (!blocks_destroyed_this_turn.contains(destroyed_block_position)) {
            blocks_destroyed_this_turn.insert(destroyed_block_position);
        }
    }

    for (auto &destroyed_robot_id: this->robots_destroyed) {
        // To avoid killing the same robot a couple of times during one turn.
        if (!players_destroyed_this_turn.contains(destroyed_robot_id)) {
            players_destroyed_this_turn.insert(destroyed_robot_id);
        }
    }
}
