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

    void serialize_position(const Position &position, boost::asio::streambuf &streambuf) {
        serialize(position.x, streambuf);
        serialize(position.y, streambuf);
    }

    void serialize(PlayersMap &players, boost::asio::streambuf &streambuf) {
        serialize((uint32_t) players.size(), streambuf);

        for (auto &player: players) {
            serialize((uint8_t) player.first, streambuf);
            serialize(player.second.name, streambuf);
            serialize(player.second.address, streambuf);
        }
    }

    void serialize(PositionsMap &player_positions, boost::asio::streambuf &streambuf) {
        serialize((uint32_t) player_positions.size(), streambuf);

        for (auto &position: player_positions) {
            serialize((uint8_t) position.first, streambuf);
            serialize_position(position.second, streambuf);
        }
    }

    void serialize(ScoresMap &scores, boost::asio::streambuf &streambuf) {
        serialize((uint32_t) scores.size(), streambuf);

        for (auto &player_score: scores) {
            serialize((uint8_t) player_score.first, streambuf);
            serialize(player_score.second, streambuf);
        }
    }

    void serialize(std::map <uint32_t, Bomb> &bombs, boost::asio::streambuf &streambuf) {
        serialize((uint32_t) bombs.size(), streambuf);

        for (auto &bomb: bombs) {
            serialize_position(bomb.second.position, streambuf);
            serialize(bomb.second.timer, streambuf);
        }
    }

    void serialize(std::set <Position> &positions,
                   boost::asio::streambuf &streambuf) {
        serialize((uint32_t) positions.size(), streambuf);

        for (auto &position: positions)
            serialize_position(position, streambuf);
    }

    boost::asio::awaitable<void> send_lobby_message(boost::asio::ip::udp::socket *socket,
                                                    boost::asio::ip::udp::endpoint &gui_endpoint,
                                                    GameInfo &game_info) {
        boost::asio::streambuf streambuf;
        serialize((uint8_t) LOBBY_MESSAGE_TO_GUI, streambuf); // Message code.
        serialize(game_info.server_name, streambuf);
        serialize(game_info.players_count, streambuf);
        serialize(game_info.size_x, streambuf);
        serialize(game_info.size_y, streambuf);
        serialize(game_info.game_length, streambuf);
        serialize(game_info.explosion_radius, streambuf);
        serialize(game_info.bomb_timer, streambuf);
        serialize(game_info.players, streambuf);
        co_await
        socket->async_send_to(streambuf.data(), gui_endpoint, boost::asio::use_awaitable);
        co_return;
    }

    boost::asio::awaitable<void> send_game_message(boost::asio::ip::udp::socket *socket,
                                                   boost::asio::ip::udp::endpoint &gui_endpoint,
                                                   GameInfo &game_info) {
        boost::asio::streambuf streambuf;
        serialize((uint8_t) GAME_MESSAGE_TO_GUI, streambuf); // Message code.
        serialize(game_info.server_name, streambuf);
        serialize(game_info.size_x, streambuf);
        serialize(game_info.size_y, streambuf);
        serialize(game_info.game_length, streambuf);
        serialize(game_info.turn, streambuf);
        serialize(game_info.players, streambuf);
        serialize(game_info.player_positions, streambuf);
        serialize(game_info.blocks, streambuf);
        serialize(game_info.bombs, streambuf);
        serialize(game_info.explosions, streambuf);
        serialize(game_info.scores, streambuf);
        co_await
        socket->async_send_to(streambuf.data(), gui_endpoint, boost::asio::use_awaitable);
        co_return;
    }

    // PlaceBomb, PlaceBlock.
    boost::asio::awaitable<void>
    send_message_to_server(boost::asio::ip::tcp::socket *socket, uint8_t code) {
        boost::asio::streambuf streambuf;
        serialize(code, streambuf);
        co_await
        socket->async_send(streambuf.data(), boost::asio::use_awaitable);
    }

    // Join.
    boost::asio::awaitable<void>
    send_message_to_server(boost::asio::ip::tcp::socket *socket, uint8_t code,
                           std::string &name) {
        boost::asio::streambuf streambuf;
        serialize(code, streambuf);
        serialize(name, streambuf);
        co_await
        socket->async_send(streambuf.data(), boost::asio::use_awaitable);
        co_return;
    }

    // Move.
    boost::asio::awaitable<void>
    send_message_to_server(boost::asio::ip::tcp::socket *socket, uint8_t code,
                           uint8_t direction) {
        boost::asio::streambuf streambuf;
        serialize(code, streambuf);
        serialize(direction, streambuf);
        co_await
        socket->async_send(streambuf.data(), boost::asio::use_awaitable);
        co_return;
    }
}
