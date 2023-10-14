
namespace Serialization {
    void serialize(uint8_t number, boost::asio::streambuf &streambuf) {
        streambuf.sputn((const char *) &number, sizeof(number));
    }

    void serialize(uint16_t number, boost::asio::streambuf &streambuf) {
        uint16_t number_be = htons(number);
        streambuf.sputn((const char *) &number_be, sizeof(number_be));
    }

    void serialize(uint32_t number, boost::asio::streambuf &streambuf) {
        uint32_t number_be = htonl(number);
        streambuf.sputn((const char *) &number_be, sizeof(number_be));
    }

    void serialize(std::string &str, boost::asio::streambuf &streambuf) {
        uint8_t length = (uint8_t) str.size();
        serialize(length, streambuf);
        streambuf.sputn((const char *) str.data(), length);
    }

    void serialize(Position &position, boost::asio::streambuf &streambuf) {
        serialize(position.x, streambuf);
        serialize(position.y, streambuf);
    }

    void serialize_hello_message(boost::asio::streambuf &streambuf, GameInfo &game_info) {
        serialize(HELLO_MESSAGE_CODE, streambuf);
        serialize(game_info.server_name, streambuf);
        serialize(game_info.players_count, streambuf);
        serialize(game_info.board_dimensions.size_x, streambuf);
        serialize(game_info.board_dimensions.size_y, streambuf);
        serialize(game_info.game_length, streambuf);
        serialize(game_info.explosion_radius, streambuf);
        serialize(game_info.bomb_timer, streambuf);
    }

    void serialize_accepted_player_message(boost::asio::streambuf &streambuf, player_id_t &id,
                                           Player &player) {
        serialize(ACCEPTED_PLAYER_MESSAGE_CODE, streambuf);
        serialize(id, streambuf);
        serialize(player.name, streambuf);
        std::string full_address =
                player.address.host + player.address.delimiter + player.address.port;
        serialize(full_address, streambuf);
    }

    void serialize_game_started_message(boost::asio::streambuf &streambuf, PlayersMap &players) {
        serialize(GAME_STARTED_MESSAGE_CODE, streambuf);
        serialize((uint32_t) players.size(), streambuf);
        for (auto &p: players) {
            serialize(p.first, streambuf);
            serialize(p.second.name, streambuf);
            std::string full_address =
                    p.second.address.host + p.second.address.delimiter + p.second.address.port;
            serialize(full_address, streambuf);
        }
    }

    void serialize_turn_message(boost::asio::streambuf &streambuf, Turn &turn) {
        serialize(TURN_MESSAGE_CODE, streambuf);
        serialize(turn.nr, streambuf);
        serialize((uint32_t) (turn.events.size() + turn.explosions.size()), streambuf);
        for (auto &event : turn.explosions) {
            event.second->get_serialized(streambuf);
        }
        for (auto &event: turn.events) {
            event.second->get_serialized(streambuf);
        }
    }

    void serialize_game_ended_message(boost::asio::streambuf &streambuf, PlayerScoreMap &scores) {
        serialize(GAME_ENDED_MESSAGE_CODE, streambuf);
        serialize((uint32_t) scores.size(), streambuf);
        for (auto &player_score: scores) {
            serialize(player_score.first, streambuf);
            serialize(player_score.second, streambuf);
        }
    }
}
