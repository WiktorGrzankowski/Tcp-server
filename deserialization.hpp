#define PLACE_CORRECT_LENGTH 1
#define MOVE_CORRECT_LENGTH 2
#define MESSAGE_CODE_INDEX 0
#define MOVE_DIRECTION_INDEX 1

namespace Deserialization {
    inline uint8_t get_gui_message_code() {
        return (uint8_t) udp_shared_buffer[MESSAGE_CODE_INDEX];
    }

    inline uint8_t get_gui_move_message_direction() {
        return (uint8_t) udp_shared_buffer[MOVE_DIRECTION_INDEX];
    }

    inline bool gui_datagram_is_legit(size_t &read) {
        if ((get_gui_message_code() == PLACE_BOMB_GUI_MESSAGE_CODE ||
             get_gui_message_code() == PLACE_BLOCK_GUI_MESSAGE_CODE) &&
            read == PLACE_CORRECT_LENGTH)
            return true;
        if (get_gui_message_code() == MOVE_GUI_MESSAGE_CODE && read == MOVE_CORRECT_LENGTH) {
            if (get_gui_move_message_direction() == UP || get_gui_move_message_direction() == RIGHT
                || get_gui_move_message_direction() == DOWN ||
                get_gui_move_message_direction() == LEFT)
                return true;
        }
        return false;
    }

    boost::asio::awaitable<void> receive_n_bytes(size_t n, boost::asio::ip::tcp::socket *socket) {
        size_t read = 0;
        while (read < n)
            read += co_await
        socket->async_read_some(
                boost::asio::buffer(shared_buffer + buffer_index + read, n - read),
                boost::asio::use_awaitable);
        co_return;
    }


    boost::asio::awaitable<void>
    receive_udp_datagram(boost::asio::ip::udp::socket *socket, size_t &read) {
        read = co_await
        socket->async_receive(
                boost::asio::buffer(udp_shared_buffer, UDP_BUFFER_SIZE),
                boost::asio::use_awaitable);
        co_return;
    }


    boost::asio::awaitable<void> react_to_gui_message(boost::asio::ip::tcp::socket *socket) {
        uint8_t code = (uint8_t) udp_shared_buffer[MESSAGE_CODE_INDEX];
        switch (code) {
            case MOVE_GUI_MESSAGE_CODE: {
                uint8_t direction = (uint8_t) udp_shared_buffer[MOVE_DIRECTION_INDEX];
                if (direction == UP || direction == RIGHT || direction == DOWN ||
                    direction == LEFT) {
                    co_await Serialization::send_message_to_server(socket, SEND_MOVE_CODE,
                                                                   direction);
                }
                break;
            }
            case PLACE_BOMB_GUI_MESSAGE_CODE: {
                co_await Serialization::send_message_to_server(socket, SEND_PLACE_BOMB_CODE);
                break;
            }
            case PLACE_BLOCK_GUI_MESSAGE_CODE: {
                co_await Serialization::send_message_to_server(socket, SEND_PLACE_BLOCK_CODE);
                break;
            }
            default : {
                break;
            }
        }
        co_return;
    }


    boost::asio::awaitable<void>
    deserialize(std::string &str, size_t length, boost::asio::ip::tcp::socket *socket) {
        buffer_index = 0;
        uint32_t starting_point = buffer_index;
        co_await receive_n_bytes(length, socket);
        for (buffer_index = starting_point; buffer_index < starting_point + length; buffer_index++)
            str += shared_buffer[buffer_index];
        buffer_index = 0;
        co_return;
    }

    boost::asio::awaitable<void>
    deserialize(uint32_t &number, boost::asio::ip::tcp::socket *socket) {
        co_await receive_n_bytes(sizeof(number), socket);
        memcpy(&number, shared_buffer + buffer_index, sizeof(number));
        number = be32toh(number);
        co_return;
    }

    boost::asio::awaitable<void>
    deserialize(uint16_t &number, boost::asio::ip::tcp::socket *socket) {
        co_await receive_n_bytes(sizeof(number), socket);
        memcpy(&number, shared_buffer + buffer_index, sizeof(number));
        number = be16toh(number);
        co_return;
    }

    boost::asio::awaitable<void>
    deserialize(uint8_t &number, boost::asio::ip::tcp::socket *socket) {
        co_await receive_n_bytes(sizeof(number), socket);
        number = static_cast<uint8_t>(shared_buffer[buffer_index]);
        co_return;
    }

    boost::asio::awaitable <Position> receive_position(boost::asio::ip::tcp::socket *socket) {
        uint16_t x_value, y_value;
        co_await deserialize(x_value, socket);
        co_await deserialize(y_value, socket);
        Position position(x_value, y_value);
        co_return position;
    }

    boost::asio::awaitable <Message::HelloMessage>
    receive_hello_message(boost::asio::ip::tcp::socket *socket) {
        Message::HelloMessage message;
        uint8_t server_name_length;
        co_await deserialize(server_name_length, socket);
        co_await deserialize(message.server_name, (size_t) server_name_length, socket);
        co_await deserialize(message.players_count, socket);
        co_await deserialize(message.size_x, socket);
        co_await deserialize(message.size_y, socket);
        co_await deserialize(message.game_length, socket);
        co_await deserialize(message.explosion_radius, socket);
        co_await deserialize(message.bomb_timer, socket);
        co_return message;
    }

    boost::asio::awaitable <Message::AcceptedPlayerMessage>
    receive_accepted_player_message(boost::asio::ip::tcp::socket *socket) {
        Message::AcceptedPlayerMessage message;
        co_await deserialize(message.id, socket);
        uint8_t length_str;
        co_await deserialize(length_str, socket);
        co_await deserialize(message.player.name, (size_t) length_str, socket);
        co_await deserialize(length_str, socket);
        co_await deserialize(message.player.address, (size_t) length_str, socket);
        co_return message;
    }

    boost::asio::awaitable <Message::GameStartedMessage>
    receive_game_started_message(boost::asio::ip::tcp::socket *socket) {
        Message::GameStartedMessage message;
        uint32_t size;
        co_await deserialize(size, socket);
        for (uint32_t i = 0; i < size; ++i) {
            uint8_t id, length_name, length_address;
            Player p;
            co_await deserialize(id, socket);
            co_await deserialize(length_name, socket);
            co_await deserialize(p.name, (size_t) length_name, socket);
            co_await deserialize(length_address, socket);
            co_await deserialize(p.address, (size_t) length_address, socket);
            message.players[id] = p;
        }
        co_return message;
    }

    boost::asio::awaitable<void>
    receive_bomb_placed(Message::TurnMessage &message, boost::asio::ip::tcp::socket *socket) {
        uint32_t bomb_id;
        co_await deserialize(bomb_id, socket);
        Position
        position = co_await
        receive_position(socket);
        Event::BombPlaced event(bomb_id, position);
        message.other_events.push_back(std::make_shared<Event::BombPlaced>(event));
        co_return;
    }

    boost::asio::awaitable<void>
    receive_bomb_exploded(Message::TurnMessage &message, boost::asio::ip::tcp::socket *socket) {
        uint32_t bomb_id, robots_destroyed_length, blocks_destroyed_length;
        co_await deserialize(bomb_id, socket);
        std::vector <player_id_t> robots_destroyed;
        std::vector <Position> blocks_destroyed;
        co_await deserialize(robots_destroyed_length, socket);
        for (uint32_t i = 0; i < robots_destroyed_length; ++i) {
            player_id_t id;
            co_await deserialize(id, socket);
            robots_destroyed.push_back(id);
        }
        co_await deserialize(blocks_destroyed_length, socket);
        for (uint32_t i = 0; i < blocks_destroyed_length; ++i) {
            Position
            position = co_await
            receive_position(socket);
            blocks_destroyed.push_back(position);
        }
        Event::BombExploded event(bomb_id, robots_destroyed, blocks_destroyed);
        message.explosions.push_back(std::make_shared<Event::BombExploded>(event));
        co_return;
    }

    boost::asio::awaitable<void>
    receive_player_moved(Message::TurnMessage &message, boost::asio::ip::tcp::socket *socket) {
        player_id_t player_id;
        co_await deserialize(player_id, socket);
        Position
        position = co_await
        receive_position(socket);
        Event::PlayerMoved event(player_id, position);
        message.other_events.push_back(std::make_shared<Event::PlayerMoved>(event));
        co_return;
    }

    boost::asio::awaitable<void>
    receive_block_placed(Message::TurnMessage &message, boost::asio::ip::tcp::socket *socket) {
        Position
        position = co_await
        receive_position(socket);
        Event::BlockPlaced event(position);
        message.other_events.push_back(std::make_shared<Event::BlockPlaced>(event));
        co_return;
    }

    boost::asio::awaitable<void>
    receive_event(Message::TurnMessage &message, boost::asio::ip::tcp::socket *socket) {
        uint8_t code;
        co_await deserialize(code, socket);
        switch (code) {
            case BOMB_PLACED_EVENT_CODE: {
                co_await receive_bomb_placed(message, socket);
                break;
            }
            case BOMB_EXPLODED_EVENT_CODE: {
                co_await receive_bomb_exploded(message, socket);
                break;
            }
            case PLAYER_MOVED_EVENT_CODE: {
                co_await receive_player_moved(message, socket);
                break;
            }
            case BLOCK_PLACED_EVENT_CODE: {
                co_await receive_block_placed(message, socket);
                break;
            }
            default: {
                break;
            }
        }
        co_return;
    }

    boost::asio::awaitable <Message::TurnMessage>
    receive_turn_message(boost::asio::ip::tcp::socket *socket) {
        Message::TurnMessage message;
        uint32_t length;
        co_await deserialize(message.turn, socket);
        co_await deserialize(length, socket);
        for (uint32_t i = 0; i < length; ++i)
            co_await receive_event(message, socket);
        co_return message;
    }

    boost::asio::awaitable <Message::GameEndedMessage>
    receive_game_ended_message(boost::asio::ip::tcp::socket *socket) {
        Message::GameEndedMessage message;
        uint32_t size;
        co_await deserialize(size, socket);
        for (uint32_t i = 0; i < size; ++i) {
            uint8_t id;
            uint32_t score;
            co_await deserialize(id, socket);
            co_await deserialize(score, socket);
            message.scores[id] = score;
        }
        co_return message;
    }
}
