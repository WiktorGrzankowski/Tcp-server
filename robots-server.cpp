#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


#include <boost/lexical_cast.hpp>

#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/program_options.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_string.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <string>
#include <optional>

#include "server_params_parsing.hpp"
#include "declarations.hpp"
#include "includes.hpp"
#include "server_deserialization.hpp"
#include "server_serialization.hpp"

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::detached;
using boost::asio::co_spawn;
using batcp = boost::asio::ip::tcp;
using bastreambuf = boost::asio::streambuf;

struct Server {
    GameInfo game_info;
    uint16_t port;
    std::set<batcp::socket *> sockets;

    Server(GameInfo &game_info, uint16_t &port) : game_info(game_info), port(port) {};

    awaitable <std::pair<player_id_t, Player>>
    receive_join_message(batcp::socket *socket, GameInfo &game_info,
                         Buffer &buffer) {
        // deserialize accepted player, czyli wczytaj stringa name
        Message::ReceiveJoinMessage message = co_await
        Deserialization::deserialize_join_message(buffer, socket);
        std::cout << "zdeserializowane\n";
        std::string full_address = boost::lexical_cast<std::string>(
                socket->remote_endpoint());
        AddressPair address(full_address);
        Player player(message.name, address);
        player_id_t id = (uint8_t) game_info.players.size();
        co_return std::pair<player_id_t, Player>({id, player});
    }

    awaitable<void>
    do_join_message(batcp::socket *socket, Buffer &buffer, player_id_t &my_player_id) {
        std::pair <player_id_t, Player> player_pair = co_await
        receive_join_message(socket, game_info, buffer);
        game_info.players.insert(player_pair);
        my_player_id = player_pair.first;
        game_info.just_accepted_player = true;
    }

    awaitable<void>
    do_place_bomb_message(player_id_t &my_player_id) {
        Message::ReceivePlaceBombMessage message(my_player_id);
        Event::BombPlaced event(game_info.total_bomb_placed_count++,
                                game_info.player_position_map[message.player_id]);
        game_info.turn_working_list.back().events[message.player_id] =
                std::make_shared<Event::BombPlaced>(event);
        co_return;
    }

    awaitable<void>
    do_place_block_message(player_id_t &my_player_id) {
        Message::ReceivePlaceBlockMessage message(my_player_id);
        Event::BlockPlaced event(game_info.player_position_map[message.player_id]);
        game_info.turn_working_list.back().events[message.player_id] =
                std::make_shared<Event::BlockPlaced>(event);
        co_return;
    }

    std::optional <Position>
    get_potential_new_position(Position &position, uint8_t direction) {
        Position potential(position.x, position.y);
        if (direction == Deserialization::UP) {
            if (position.y == game_info.board_dimensions.size_y - 1)
                return std::nullopt;
            potential.y++;
        } else if (direction == Deserialization::RIGHT) {
            if (position.x == game_info.board_dimensions.size_x - 1)
                return std::nullopt;
            potential.x++;
        } else if (direction == Deserialization::DOWN) {
            if (position.y == 0)
                return std::nullopt;
            potential.y--;
        } else if (direction == Deserialization::LEFT) {
            if (position.x == 0)
                return std::nullopt;
            potential.x--;
        }
        if (game_info.block_position_set.contains({potential.x, potential.y}))
            return std::nullopt;
        return potential;
    }

    awaitable<void>
    do_move_message(batcp::socket *socket, Buffer &buffer, player_id_t &my_player_id) {
        Message::ReceiveMoveMessage message = co_await
        Deserialization::deserialize_move_message(buffer, socket, my_player_id);

        Position &my_position = game_info.player_position_map[message.player_id];
        std::optional <Position> potential_position = get_potential_new_position(my_position,
                                                                                 message.direction);
        if (potential_position) {
            Event::PlayerMoved event(my_player_id, potential_position.value());
            game_info.turn_working_list.back().events[my_player_id] =
                    std::make_shared<Event::PlayerMoved>(event);
        }
        co_return;
    }

    void prepare_board() {
        if (game_info.current_turn > 0)
            return;
        for (player_id_t i = 0; i < game_info.players_count; ++i) {
            Position position((uint16_t) game_info.random_number_generator.generate() %
                              game_info.board_dimensions.size_x,
                              (uint16_t) game_info.random_number_generator.generate() %
                              game_info.board_dimensions.size_y);

            game_info.player_position_map[i] = position;
            game_info.player_score_map[i] = 0;
            // dodaj zdarzenie PlayerMoved do listy
            Event::PlayerMoved event(i, position);
            game_info.turn_official_list.back().events[(uint32_t) game_info.turn_official_list.back().events.size()] =
                    std::make_shared<Event::PlayerMoved>(event);
        }

        for (uint16_t i = 0; i < game_info.initial_blocks; ++i) {
            Position position;
            position.x = (uint16_t) game_info.random_number_generator.generate() %
                         game_info.board_dimensions.size_x;
            position.y = (uint16_t) game_info.random_number_generator.generate() %
                         game_info.board_dimensions.size_y;
            game_info.block_position_set.insert(position);
            Event::BlockPlaced event(position);
            game_info.turn_official_list.back().events[(uint32_t) game_info.turn_official_list.back().events.size()] =
                    std::make_shared<Event::BlockPlaced>(event);
        }
    }

    Event::BombExploded do_bomb_exploded(uint32_t bomb_id, Bomb &bomb) {
        Event::BombExploded event;
        event.id = bomb_id;
        // First find where the bomb explodes.
        event.calc_explosion(game_info, bomb);
        // Find robots and blocks standing on positions where the explosion is taking place.
        event.create_blocks_destroyed_list(game_info.block_position_set);
        event.create_robots_destroyed_list(game_info.player_position_map);
        std::cout << "destroyed " << event.robots_destroyed.size() << " and "
                  << event.blocks_destroyed.size() << " blocks\n";
        return event;
    }

    void update_game_info_with_turn_events() {
        for (auto &bomb: game_info.bomb_map) {
            bomb.second.timer--;
            if (bomb.second.timer == 0) {
                Event::BombExploded event = do_bomb_exploded(bomb.first, bomb.second);
                game_info.turn_official_list.back().explosions[(uint32_t) game_info.turn_official_list.back().explosions.size()] = std::make_shared<Event::BombExploded>(
                        event);
            }
        }
        for (auto &explosion: game_info.turn_official_list.back().explosions) {
            explosion.second->update_game_info(game_info);
        }
        for (auto &event: game_info.turn_working_list.back().events) {
            event.second->update_game_info(game_info);
        }
    }

    void update_game_info_with_game_ended() {
        game_info.is_running = false;
        game_info.game_started_to_be_sent = false;
        game_info.current_turn = 0;
        game_info.player_position_map.clear();
        game_info.player_score_map.clear();
        game_info.players.clear();
        game_info.players_working.clear();
        game_info.turn_working_list.clear();
        game_info.bomb_map.clear();
        game_info.block_position_set.clear();
        game_info.total_bomb_placed_count = 0;
    }

    void catch_up_with_running_game(batcp::socket *socket) {
        std::cout << "gra juz chodzi\n";
        bastreambuf streambuf_game_started;
        Serialization::serialize_game_started_message(streambuf_game_started, game_info.players);
        socket->send(streambuf_game_started.data());
        // teraz wszystkie zalegle tury

        for (auto turn: game_info.turn_official_list) {
            bastreambuf streambuf_turn;
            Serialization::serialize_turn_message(streambuf_turn, turn);
            socket->send(streambuf_turn.data());
            std::cout << "NADRABIAM I WYSLALEM TURE NR " << turn.nr << " rozmiaru "
                      << turn.events.size() + turn.explosions.size() << "\n";
        }
    }

    void catch_up_with_game_in_lobby(batcp::socket *socket) {
        // game nie jest running, ale mogli juz dolaczyc jacys zawodnicy
        if (game_info.players.size() > 0) {
            // są już jacyś zawodnicy, powiadom o ich dołączeniu
            for (auto player_pair: game_info.players) {
                bastreambuf streambuf_player;
                player_id_t player_id = player_pair.first;
                Serialization::serialize_accepted_player_message(streambuf_player, player_id,
                                                                 player_pair.second);
                socket->send(streambuf_player.data());
            }
        }
    }

    void catch_up_with_game(batcp::socket *socket) {
        if (game_info.is_running)
            catch_up_with_running_game(socket);
        else
            catch_up_with_game_in_lobby(socket);
    }

    void send_hello_message(batcp::socket *socket) {
        socket->set_option(batcp::no_delay(true));
        bastreambuf streambuf;
        Serialization::serialize_hello_message(streambuf, game_info);
        socket->send(streambuf.data());
    }

    awaitable<void> read_single_event(batcp::socket *socket, Buffer &buffer,
                                      player_id_t &my_player_id) {
        buffer.index = 0;
        co_await Deserialization::receive_n_bytes(buffer, 1, socket);
        if (buffer.get_message_code() == Message::RECEIVE_JOIN_MESSAGE_CODE) {
            co_await do_join_message(socket, buffer, my_player_id);
            if (game_info.enough_clients_joined())
                game_info.game_started_to_be_sent = true;
        } else if (buffer.get_message_code() == Message::RECEIVE_PLACE_BOMB_MESSAGE_CODE) {
            co_await do_place_bomb_message(my_player_id);
        } else if (buffer.get_message_code() == Message::RECEIVE_PLACE_BLOCK_MESSAGE_CODE) {
            co_await do_place_block_message(my_player_id);
        } else if (buffer.get_message_code() == Message::RECEIVE_MOVE_MESSAGE_CODE) {
            co_await do_move_message(socket, buffer, my_player_id);
        } else {
            // todo - error i chyba rozłączenie
            std::cerr << "INVALID MESSAGE FROM CLIENT\n";
            exit(1);
        }
        co_return;
    }

    void prepare_turn(bastreambuf &streambuf) {
        update_game_info_with_turn_events();
        Serialization::serialize_turn_message(streambuf, game_info.turn_official_list.back());
    }

    void prepare_game_ended(bastreambuf &streambuf) {
        Serialization::serialize_game_ended_message(streambuf, game_info.player_score_map);
        update_game_info_with_game_ended();
    }

    void send_new_accepted_player_messages(size_t &players_accepted_sent) {
        if (game_info.players.size() > players_accepted_sent) {
            while (players_accepted_sent < game_info.players.size()) {
                // serializuj i wysylaj player accepted
                bastreambuf streambuf_accepted_player;
                player_id_t player_id = (uint8_t) players_accepted_sent;
                Player player(game_info.players[player_id].name,
                              game_info.players[player_id].address);
                Serialization::serialize_accepted_player_message(streambuf_accepted_player,
                                                                 player_id, player);
                // Niech każdy dowie się o dołączeniu tego zawodnika.
                for (auto socket: sockets) {
                    socket->send(streambuf_accepted_player.data());
                }
                players_accepted_sent++;
            }
        }
    }

    awaitable<void> wait_time_duration() {
        boost::asio::deadline_timer timer(co_await boost::asio::this_coro::executor,
                                          boost::posix_time::milliseconds(game_info.turn_duration));
        co_await
        timer.async_wait(use_awaitable);
        co_return;
    }

    void create_space_for_following_turns() {
        game_info.current_turn++;
        game_info.turn_working_list.push_back(Turn(game_info.current_turn));
        game_info.turn_official_list.push_back(Turn(game_info.current_turn));
    }

    void send_game_started() {
        game_info.game_started_to_be_sent = false;
        game_info.is_running = true;
        game_info.current_turn = 0;
        bastreambuf streambuf_game_started;
        Serialization::serialize_game_started_message(streambuf_game_started,
                                                      game_info.players);
        for (auto socket: sockets)
            socket->send(streambuf_game_started.data());

    }

    void send_game_ended() {
        bastreambuf streambuf;
        prepare_game_ended(streambuf);
        for (auto socket: sockets)
            socket->send(streambuf.data());
    }

    void send_turn() {
        bastreambuf streambuf;
        prepare_turn(streambuf);
        for (auto socket: sockets) {
            socket->send(streambuf.data());
        }
    }

    awaitable<void>
    single_client_listener(batcp::socket socket) {
        send_hello_message(&socket);
        catch_up_with_game(&socket);
        sockets.insert(&socket);
        player_id_t my_player_id;
        Buffer buffer;
        for (;;) {
            co_await read_single_event(&socket, buffer, my_player_id);
        }
        co_return;
    }

    awaitable<void> connections_listener() {
        auto executor = co_await
        boost::asio::this_coro::executor;
        batcp::acceptor acceptor(executor, {batcp::v6(), port});
        for (;;) {
            batcp::socket socket =
                    co_await
            acceptor.async_accept(use_awaitable);

            co_spawn(executor,
                     single_client_listener(std::move(socket)),
                     detached);
        }
        co_return;
    }

    awaitable<void>
    all_clients_informer() {
        size_t players_accepted_sent = 0;
        for (;;) {
            co_await wait_time_duration();
            std::cout << "____STARY_WSTAL\n";
            send_new_accepted_player_messages(players_accepted_sent);
            if (game_info.game_started_to_be_sent) {
                send_game_started();
                continue;
            }
            if (game_info.current_turn == 0) {
                game_info.turn_working_list.push_back(Turn(game_info.current_turn));
                game_info.turn_official_list.push_back(Turn(game_info.current_turn));
                prepare_board();
            } else {
                game_info.turn_official_list.back() = game_info.turn_working_list.back();
            }
            send_turn();
            std::cout << "TURN NR " << game_info.current_turn << "/" << game_info.game_length << "\n";
            if (game_info.last_turn_finished())
                send_game_ended();
            create_space_for_following_turns();
        }
        co_return;
    }


    void run() {
        boost::asio::io_context io_context(1); // concurrency_hint?

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        co_spawn(io_context, connections_listener(), detached);
        co_spawn(io_context, all_clients_informer(), detached);

        io_context.run();
    }

};

int main(int argc, char **argv) {
    try {
        ServerProgramParams::ServerProgramParams program_params =
                ServerProgramParams::parse_program_params(argc, argv);
        std::cout << program_params.server_name << " " << program_params.seed << " "
                  << (uint32_t) std::chrono::system_clock::now().time_since_epoch().count() << "\n";


        GameInfo game_info(program_params);

        Server server(game_info, program_params.port);
        server.run();
    } catch (std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        exit(1);
    }
    return 0;
}
