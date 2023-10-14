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


namespace ProgramParams {
    struct AddressPair {
        std::string host;
        std::string port;
    };

    struct ProgramParams {
        ProgramParams(std::string player_name, uint16_t port,
                      AddressPair server_address, AddressPair gui_address) :
                player_name(player_name), port(port), server_address(server_address),
                gui_address(gui_address) {};

        std::string player_name;
        uint16_t port;
        AddressPair server_address;
        AddressPair gui_address;
    };

    AddressPair parse_server_address(
            std::string address_spec, std::string default_service = "https") {
        using namespace boost::spirit::x3;
        auto service = ':' >> +~char_(":") >> eoi;
        auto host = '[' >> *~char_(']') >> ']'  // e.g. for IPV6
                    | raw[*("::" | (char_ - service))];

        std::tuple <std::string, std::string> result;
        parse(begin(address_spec), end(address_spec),
              expect[host >> (service | attr(default_service))], result);

        return AddressPair(std::get<0>(result), std::get<1>(result));
    }

    ProgramParams parse_program_params(int argc, char **av) {
        boost::program_options::options_description desc("Allowed options");
        desc.add_options()
                ("help,h", "produce help message")
                ("port,p", boost::program_options::value<uint16_t>(), "port")
                ("player-name,n", boost::program_options::value<std::string>(), "player-name")
                ("gui-address,d", boost::program_options::value<std::string>(), "gui-address")
                ("server-address,s", boost::program_options::value<std::string>(),
                 "server-address");

        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, av, desc),
                                      vm);
        boost::program_options::notify(vm);
        if (vm.count("help")) {
            std::cout << desc << "\n";
            exit(0);
        }

        if (!vm.count("port") || !vm.count("player-name") ||
            !vm.count("server-address") || !vm.count("gui-address")) {
            std::cerr << "Wrong arguments provided\n";
            std::cerr << desc << "\n";
            exit(1);
        }
        AddressPair server_params =
                parse_server_address(vm["server-address"].as<std::string>());
        AddressPair gui_params =
                parse_server_address(vm["gui-address"].as<std::string>());

        ProgramParams program_params(vm["player-name"].as<std::string>(),
                                     vm["port"].as<uint16_t>(),
                                     server_params,
                                     gui_params);
        return program_params;
    }
}
