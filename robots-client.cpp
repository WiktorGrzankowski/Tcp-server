#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

#include "params_parsing.hpp"
#include "game.hpp"
#include "serialization.hpp"
#include "deserialization.hpp"

static boost::asio::awaitable<void>
gui_listener(boost::asio::ip::udp::socket *socket_listen,
             boost::asio::ip::tcp::socket *server_socket, GameInfo &game_info) {
    size_t read;
    for (;;) {
        co_await Deserialization::receive_udp_datagram(socket_listen, read);
        if (!game_info.join_sent && Deserialization::gui_datagram_is_legit(read)) {
            co_await Serialization::send_message_to_server(server_socket, SEND_JOIN_CODE,
                                                           game_info.my_player_name);
            game_info.join_sent = true;
        } else if (Deserialization::gui_datagram_is_legit(read) && !game_info.in_lobby) {
            co_await Deserialization::react_to_gui_message(server_socket);
        }
    }
    co_return;
}

static boost::asio::awaitable<void>
listen_to_hello_message(GameInfo &game_info, boost::asio::ip::tcp::socket *socket,
                        bool &received_hello) {
    if (!received_hello) {
        received_hello = true;
        Message::HelloMessage message = co_await
        Deserialization::receive_hello_message(socket);
        game_info.update_with_hello_info(message);
    }
}

static boost::asio::awaitable<void>
listen_to_accepted_player_message(GameInfo &game_info, boost::asio::ip::tcp::socket *socket) {
    if (game_info.in_lobby) {
        Message::AcceptedPlayerMessage message = co_await
        Deserialization::receive_accepted_player_message(
                socket);
        game_info.update_with_accepted_player_info(message);
    }
}

static boost::asio::awaitable<void>
listen_to_game_started_message(GameInfo &game_info, boost::asio::ip::tcp::socket *socket,
                               bool &just_received_game_started) {
    if (game_info.in_lobby) {
        just_received_game_started = true;
        Message::GameStartedMessage message = co_await
        Deserialization::receive_game_started_message(
                socket);
        game_info.update_with_game_started_info(message);
    }
}

static boost::asio::awaitable<void>
listen_to_turn_message(GameInfo &game_info, boost::asio::ip::tcp::socket *socket) {
    game_info.explosions.clear();
    Message::TurnMessage message = co_await
    Deserialization::receive_turn_message(socket);
    game_info.update_with_turn_info(message);
}

static boost::asio::awaitable<void>
listen_to_game_ended_message(GameInfo &game_info, boost::asio::ip::tcp::socket *socket) {
    co_await Deserialization::receive_game_ended_message(socket);
    game_info.update_with_game_ended_info();
}

static boost::asio::awaitable<void>
inform_gui(GameInfo &game_info, boost::asio::ip::udp::socket *send_udp_socket,
           boost::asio::ip::udp::endpoint &endpoint,
           bool &just_received_game_started) {
    if (just_received_game_started)
        just_received_game_started = false;
    else if (game_info.in_lobby)
        co_await Serialization::send_lobby_message(send_udp_socket, endpoint, game_info);
    else
        co_await Serialization::send_game_message(send_udp_socket, endpoint, game_info);
}

static boost::asio::awaitable<void>
server_listener(boost::asio::ip::tcp::socket *socket, boost::asio::ip::udp::socket *send_udp_socket,
                boost::asio::ip::udp::endpoint &endpoint, GameInfo &game_info) {
    bool received_hello = false;
    bool just_received_game_started = false;
    for (;;) {
        try {
            buffer_index = 0;
            co_await Deserialization::receive_n_bytes(1, socket);
            switch (shared_buffer[0]) {
                case HELLO_CODE: {
                    co_await listen_to_hello_message(game_info, socket, received_hello);
                    break;
                }
                case ACCEPTED_PLAYER_CODE: {
                    co_await listen_to_accepted_player_message(game_info, socket);
                    break;
                }
                case GAME_STARTED_CODE: {
                    co_await listen_to_game_started_message(game_info, socket,
                                                            just_received_game_started);
                    break;
                }
                case TURN_CODE: {
                    co_await listen_to_turn_message(game_info, socket);
                    break;
                }
                case GAME_ENDED_CODE: {
                    co_await listen_to_game_ended_message(game_info, socket);
                    break;
                }
                default: {
                    throw std::runtime_error("Invalid message from server.");
                    break;
                }
            }
        } catch (std::exception &e) {
            std::cerr << "error: " << e.what() << "\n";
            exit(1);
        }
        co_await inform_gui(game_info, send_udp_socket, endpoint, just_received_game_started);
    }
    co_return;
}

static void robots_client(ProgramParams::ProgramParams &program_params) {
    GameInfo game_info;
    game_info.my_player_name = program_params.player_name; // Set player name.
    boost::asio::io_context io_context;
    // Set up TCP socket.
    boost::asio::ip::tcp::resolver server_resolver(io_context);
    boost::asio::ip::tcp::resolver::results_type server_endpoint = server_resolver.resolve(
            program_params.server_address.host, program_params.server_address.port);
    boost::asio::ip::tcp::no_delay option(true); // Disable Nagle's algorithm.
    boost::asio::ip::tcp::socket server_socket(io_context);
    boost::asio::connect(server_socket, server_endpoint);
    server_socket.set_option(option);
    // Set up UDP socket for sending datagrams.
    boost::asio::ip::udp::resolver gui_resolver(io_context);
    boost::asio::ip::udp::endpoint gui_endpoint = *gui_resolver.resolve(
            program_params.gui_address.host,
            program_params.gui_address.port).begin();
    boost::asio::ip::udp::socket gui_socket(io_context);
    gui_socket.open(boost::asio::ip::udp::v6());
    // Set up UDP socket for receiving datagrams.
    boost::asio::ip::udp::socket gui_socket_listen(io_context);
    gui_socket_listen.open(boost::asio::ip::udp::v6());
    gui_socket_listen.bind({boost::asio::ip::udp::v6(), program_params.port});

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) { io_context.stop(); });

    boost::asio::co_spawn(io_context, gui_listener(&gui_socket_listen, &server_socket, game_info),
                          boost::asio::detached);
    boost::asio::co_spawn(io_context, server_listener(&server_socket, &gui_socket, gui_endpoint,
                                                      game_info), boost::asio::detached);

    io_context.run();
}

int main(int argc, char **argv) {
    try {
        ProgramParams::ProgramParams program_params = ProgramParams::parse_program_params(argc,
                                                                                          argv);
        robots_client(program_params);
    } catch (std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        exit(1);
    }
    return 0;
}
