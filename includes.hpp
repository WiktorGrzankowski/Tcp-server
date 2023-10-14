struct AddressPair {
    AddressPair() {}
    AddressPair(std::string full_address) {
        size_t delimieter_index = full_address.find_last_of(":");
        host = full_address.substr(0, delimieter_index);
        port = full_address.substr(delimieter_index + 1, full_address.size());
    }

    std::string host;
    std::string port;
    std::string delimiter = std::string(1, ':');
};


struct Player {
    Player() {}
    Player(std::string name, AddressPair &address) : name(name), address(address) {};
    std::string name;
    AddressPair address;
};

struct Position {
    Position() {}

    Position(uint16_t x, uint16_t y) : x(x), y(y) {};
    uint16_t x;
    uint16_t y;

    bool operator<(const Position &position) const {
        return this->x < position.x || (this->x == position.x && this->y < position.y);
    }
};

struct BoardDimensions {
    uint16_t size_x;
    uint16_t size_y;
};

//r_0 = (seed * 48271) mod 2147483647
//r_i = (r_{i-1} * 48271) mod 2147483647
struct RandomNumberGenerator {
    uint32_t last_number;

    uint32_t generate() {
        last_number = (last_number * 48271) % 2147483647;
        return last_number;
    }
};

struct Bomb {
    Bomb() {}

    Bomb(Position position, uint16_t timer) : position(position), timer(timer) {};
    Position position;
    uint16_t timer;
};

struct Turn {
    // lista eventów
    Turn() {}

    Turn(uint16_t nr) : nr(nr) {};
    uint16_t nr;
    uint32_t explosions_count = 0;
    std::map <uint32_t, std::shared_ptr<Event::EventS>> events;
    std::map <uint32_t, std::shared_ptr<Event::BombExploded>> explosions;
};


struct GameInfo {
    bool is_running;
    bool game_started_to_be_sent;
    bool just_accepted_player;
    uint8_t players_count;
    uint16_t bomb_timer;
    uint64_t turn_duration;
    uint16_t explosion_radius;
    uint16_t game_length;
    std::string server_name;
    BoardDimensions board_dimensions;
    uint32_t seed; // może niepotrzebne
    // numer tury
    uint16_t current_turn;
    // lista graczy (nazwa, adres IP, numer portu)
    PlayersMap players;
    PlayersMap players_working;
    //stan generatora liczb losowych (innymi słowy stan generatora NIE restartuje się po każdej rozgrywce)
    RandomNumberGenerator random_number_generator;
    // lista wszystkich tur od początku rozgrywki
    std::vector <Turn> turn_working_list;
    std::vector <Turn> turn_official_list;
    // pozycje graczy
    PlayerPositionMap player_position_map;
    // liczba śmierci każdego gracza
    PlayerScoreMap player_score_map;
    // informacje o istniejących bombach (pozycja, czas)
    BombMap bomb_map;
    // pozycje istniejących bloków
    std::set <Position> block_position_set;
    uint16_t initial_blocks;
    uint32_t total_bomb_placed_count;

    GameInfo(ServerProgramParams::ServerProgramParams &program_params) {
        is_running = false;
        game_started_to_be_sent = false;
        just_accepted_player = false;
        players_count = program_params.players_count;
        bomb_timer = program_params.bomb_timer;
        turn_duration = program_params.turn_duration;
        explosion_radius = program_params.explosion_radius;
        game_length = program_params.game_length;
        server_name = program_params.server_name;
        board_dimensions.size_x = program_params.size_x;
        board_dimensions.size_y = program_params.size_y;
        seed = program_params.seed;
        initial_blocks = program_params.initial_blocks;
        random_number_generator.last_number = seed;
        total_bomb_placed_count = 0;
    }

    bool enough_clients_joined() {
        return (uint8_t) players.size() == players_count;
    }

    bool last_turn_finished() {
        return current_turn == game_length;
    }
};

namespace Event {
//    const uint8_t BOMB_PLACED_CODE = 0;
//    const uint8_t BOMB_EXPLODED_CODE = 1;
//    const uint8_t PLAYER_MOVED_CODE = 2;
//    const uint8_t BLOCK_PLACED_CODE = 3;

    struct EventS {
        virtual void get_serialized(boost::asio::streambuf &streambuf) = 0;

        virtual void update_game_info(GameInfo &game_info) = 0;
    };

    struct BombPlaced : EventS {
        BombPlaced() {}

        BombPlaced(bomb_id_t id, Position position) : id(id), position(position) {};
        bomb_id_t id;
        Position position;

        void get_serialized(boost::asio::streambuf &streambuf) override {
//            std::cout << "SER BO_PL\n";
            Serialization::serialize(BOMB_PLACED_CODE, streambuf);
            Serialization::serialize(id, streambuf);
            Serialization::serialize(position, streambuf);
        }

        void update_game_info(GameInfo &game_info) override {
            Bomb bomb(position, game_info.bomb_timer);
            game_info.bomb_map[id] = bomb;
        }

    };

    struct PlayerMoved : EventS {
        PlayerMoved() {}

        PlayerMoved(player_id_t id, Position position) : id(id), position(position) {};

        player_id_t id;
        Position position;

        void get_serialized(boost::asio::streambuf &streambuf) override {
//            std::cout << "SER PL_MV\n";
            Serialization::serialize(PLAYER_MOVED_CODE, streambuf);
            Serialization::serialize(id, streambuf);
            Serialization::serialize(position, streambuf);
        }

        void update_game_info(GameInfo &game_info) override {
            game_info.player_position_map[id] = position;
        }
    };

    struct BombExploded : EventS {
        bomb_id_t id;
        std::vector <player_id_t> robots_destroyed;
        std::vector <Position> blocks_destroyed;
        std::set <Position> explosion_positions;

        void get_serialized(boost::asio::streambuf &streambuf) override {
//            std::cout << "SER BO_EX\n";
            Serialization::serialize(BOMB_EXPLODED_CODE, streambuf);
            Serialization::serialize(id, streambuf);

            Serialization::serialize((uint32_t) robots_destroyed.size(), streambuf);
            for (auto &robot_id: robots_destroyed)
                Serialization::serialize(robot_id, streambuf);

            Serialization::serialize((uint32_t) blocks_destroyed.size(), streambuf);
            for (auto &block_position: blocks_destroyed)
                Serialization::serialize(block_position, streambuf);
        }

        void update_game_info(GameInfo &game_info) override {
            for (auto &player_id: robots_destroyed) {
                game_info.player_score_map[player_id]++;
                Position position((uint16_t) game_info.random_number_generator.generate() %
                                  game_info.board_dimensions.size_x,
                                  (uint16_t) game_info.random_number_generator.generate() %
                                  game_info.board_dimensions.size_y);
                game_info.player_position_map[player_id] = position;
                Event::PlayerMoved moved_event;
                moved_event.id = player_id;
                moved_event.position = position;
                game_info.turn_official_list.back().events[player_id] =
                        std::make_shared<Event::PlayerMoved>(moved_event);
            }

            for (auto &position: blocks_destroyed) {
                game_info.block_position_set.erase(position);
            }

            game_info.bomb_map.erase(id);
        }

        void calc_explosion(GameInfo &game_info, Bomb &bomb) {
            for (uint16_t i = bomb.position.y;
                 i < bomb.position.y + game_info.explosion_radius + 1 &&
                 i < game_info.board_dimensions.size_y; ++i) {
                if (game_info.block_position_set.contains({bomb.position.x, bomb.position.y})) {
                    explosion_positions.insert({bomb.position.x, bomb.position.y});
                    return;
                }
                explosion_positions.insert({bomb.position.x, i});

                if (game_info.block_position_set.contains({bomb.position.x, i}))
                    break;
            }
            for (uint16_t i = bomb.position.y - 1;
                 i > bomb.position.y - game_info.explosion_radius - 1 &&
                 bomb.position.y != 0; --i) {
                explosion_positions.insert({bomb.position.x, i});
                if (game_info.block_position_set.contains({bomb.position.x, i}) || i == 0)
                    break;
            }
            for (uint16_t i = bomb.position.x + 1;
                 i < bomb.position.x + game_info.explosion_radius + 1 &&
                 i < game_info.board_dimensions.size_x; ++i) {
                explosion_positions.insert({i, bomb.position.y});
                if (game_info.block_position_set.contains({i, bomb.position.y}))
                    break;
            }
            for (uint16_t i = bomb.position.x - 1;
                 i > bomb.position.x - game_info.explosion_radius - 1 &&
                 bomb.position.x != 0; --i) {
                explosion_positions.insert({i, bomb.position.y});
                if (game_info.block_position_set.contains({i, bomb.position.y}) || i == 0)
                    break;
            }
        }

        void create_blocks_destroyed_list(std::set <Position> &block_positions) {
            for (auto &position: explosion_positions) {
                if (block_positions.contains(position))
                    blocks_destroyed.push_back(position);
            }
        }

        void create_robots_destroyed_list(PlayerPositionMap &player_positions) {
            for (auto &player: player_positions) {
                if (explosion_positions.contains(player.second))
                    robots_destroyed.push_back(player.first);
            }
        }
    };

    struct BlockPlaced : EventS {
        BlockPlaced() {}

        BlockPlaced(Position position) : position(position) {};
        Position position;

        void get_serialized(boost::asio::streambuf &streambuf) override {
//            std::cout << "SER BL_PL\n";
            Serialization::serialize(BLOCK_PLACED_CODE, streambuf);
            Serialization::serialize(position, streambuf);
        }

        void update_game_info(GameInfo &game_info) override {
            game_info.block_position_set.insert(position);
        }
    };
}

namespace Message {
    struct ReceiveMessageS {
        virtual uint8_t get_code() = 0;
    };

    struct ReceiveJoinMessage : ReceiveMessageS {
        ReceiveJoinMessage() {}

        ReceiveJoinMessage(std::string name) : name(name) {};
        std::string name;

        uint8_t get_code() override {
            return RECEIVE_JOIN_MESSAGE_CODE;
        }
    };

    struct ReceivePlaceBombMessage : ReceiveMessageS {
        ReceivePlaceBombMessage() {}

        ReceivePlaceBombMessage(player_id_t player_id) : player_id(player_id) {};
        player_id_t player_id;

        uint8_t get_code() override {
            return RECEIVE_PLACE_BOMB_MESSAGE_CODE;
        }
    };

    struct ReceivePlaceBlockMessage : ReceiveMessageS {
        ReceivePlaceBlockMessage() {}

        ReceivePlaceBlockMessage(player_id_t player_id) : player_id(player_id) {}

        player_id_t player_id;

        uint8_t get_code() override {
            return RECEIVE_PLACE_BLOCK_MESSAGE_CODE;
        }
    };

    struct ReceiveMoveMessage : ReceiveMessageS {
        ReceiveMoveMessage() {}

        ReceiveMoveMessage(player_id_t player_id, uint8_t direction) : player_id(player_id),
                                                                       direction(direction) {};
        player_id_t player_id;
        uint8_t direction;

        uint8_t get_code() override {
            return RECEIVE_MOVE_MESSAGE_CODE;
        }
    };
}

struct Buffer {
//    const size_t MAX_BUFFER_SIZE = 4096;
    char data[4096];
    size_t index = 0;

    uint8_t get_message_code() {
        return data[0];
    }
};
