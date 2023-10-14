#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/array.hpp>
#include <boost/asio.hpp>

#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/program_options.hpp>
#include <utility>
#include <utility>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_string.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <string>

namespace ServerProgramParams {
    struct ServerProgramParams {
        ServerProgramParams(uint16_t bomb_timer, uint16_t players_count, uint64_t turn_duration,
                            uint16_t explosion_radius, uint16_t initial_blocks,
                            uint16_t game_length, std::string server_name, uint16_t port,
                            uint16_t size_x, uint16_t size_y,
                            uint32_t seed = (uint32_t) std::chrono::system_clock::now().time_since_epoch().count())
                :
                bomb_timer(bomb_timer), players_count((uint8_t)players_count),
                turn_duration(turn_duration), explosion_radius(explosion_radius),
                initial_blocks(initial_blocks), game_length(game_length),
                server_name(server_name), port(port), size_x(size_x),
                size_y(size_y), seed(seed) {
            if (players_count > 255) {
                std::cerr << "Players count number is too large.";
                exit(1);
            }
        };

        uint16_t bomb_timer;
        uint8_t players_count;
        uint64_t turn_duration;
        uint16_t explosion_radius;
        uint16_t initial_blocks;
        uint16_t game_length;
        std::string server_name;
        uint16_t port;
        uint16_t size_x;
        uint16_t size_y;
        uint32_t seed;
    };

    bool help_provided(boost::program_options::variables_map &vm) {
        return vm.count("help");
    }

    bool seed_provided(boost::program_options::variables_map &vm) {
        return vm.count("seed");
    }

    bool necessary_arguments_provided(boost::program_options::variables_map &vm) {
        return vm.count("bomb-timer") && vm.count("players-count")
               && vm.count("turn-duration") && vm.count("explosion-radius")
               && vm.count("initial-blocks") && vm.count("game-length")
               && vm.count("server-name") && vm.count("port")
               && vm.count("size-x") && vm.count("size-y");
    }

    ServerProgramParams get_server_program_params(boost::program_options::variables_map &vm) {
        if (seed_provided(vm)) {
            return ServerProgramParams(vm["bomb-timer"].as<uint16_t>(),
                                       vm["players-count"].as<uint16_t>(),
                                               vm["turn-duration"].as<uint64_t>(),
                                               vm["explosion-radius"].as<uint16_t>(),
                                               vm["initial-blocks"].as<uint16_t>(),
                                               vm["game-length"].as<uint16_t>(),
                                               vm["server-name"].as<std::string>(),
                                               vm["port"].as<uint16_t>(),
                                               vm["size-x"].as<uint16_t>(),
                                               vm["size-y"].as<uint16_t>(),
                                               vm["seed"].as<uint32_t>());
        } else {
            return ServerProgramParams(vm["bomb-timer"].as<uint16_t>(),
                                               vm["players-count"].as<uint16_t>(),
                                               vm["turn-duration"].as<uint64_t>(),
                                               vm["explosion-radius"].as<uint16_t>(),
                                               vm["initial-blocks"].as<uint16_t>(),
                                               vm["game-length"].as<uint16_t>(),
                                               vm["server-name"].as<std::string>(),
                                               vm["port"].as<uint16_t>(),
                                               vm["size-x"].as<uint16_t>(),
                                               vm["size-y"].as<uint16_t>());
        }
    }

    ServerProgramParams parse_program_params(int argc, char **av) {
        boost::program_options::options_description desc("Allowed options");
        desc.add_options()
                ("bomb-timer,b", boost::program_options::value<uint16_t>(), "bomb-timer")
                ("players-count,c", boost::program_options::value<uint16_t>(), "players-count")
                ("turn-duration,d", boost::program_options::value<uint64_t>(), "turn-duration")
                ("explosion-radius,e", boost::program_options::value<uint16_t>(),
                 "explosion-radius")
                ("help,h", "produce help message")
                ("initial-blocks,k", boost::program_options::value<uint16_t>(), "initial-blocks")
                ("game-length,l", boost::program_options::value<uint16_t>(), "game-length")
                ("server-name,n", boost::program_options::value<std::string>(), "server-name")
                ("port,p", boost::program_options::value<uint16_t>(), "port")
                ("seed,s", boost::program_options::value<uint32_t>(), "seed")
                ("size-x,x", boost::program_options::value<uint16_t>(), "size-x")
                ("size-y,y", boost::program_options::value<uint16_t>(), "size-y");

        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, av, desc),
                                      vm);
        boost::program_options::notify(vm);
        if (help_provided(vm)) {
            std::cout << desc << "\n";
            exit(0);
        }
        if (!necessary_arguments_provided(vm)) {
            std::cerr << "Wrong arguments provided\n";
            std::cerr << desc << "\n";
            exit(1);
        }
        return get_server_program_params(vm);
    }
}
