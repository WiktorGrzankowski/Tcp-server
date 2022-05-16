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

using std::cerr;
using std::cout;
using std::endl;
using std::exception;
namespace po = boost::program_options;

class ProgramParams {
   private:
    std::string player_name;
    uint16_t port;
    sockaddr *server_address;
    sockaddr *gui_address;

   public:
    ProgramParams(std::string x, uint16_t p, sockaddr *ser_ad, sockaddr *gui_ad)
        : player_name(x),
          port(p),
          server_address(ser_ad),
          gui_address(gui_ad){};

    void set_port(uint16_t port_param) { port = port_param; }

    void set_player_name(std::string name_param) { player_name = name_param; }

    void set_server_address(sockaddr *server_addr_param) {
        server_address = server_addr_param;
    }

    void set_gui_address(sockaddr *gui_addr_param) {
        gui_address = gui_addr_param;
    }

    std::string get_player_name() { return player_name; }

    uint16_t get_port() { return port; }

    sockaddr *get_server_address() { return server_address; }

    sockaddr *get_gui_address() { return gui_address; }
};

std::tuple<std::string, std::string> parse_server_address(
    std::string address_spec, std::string default_service = "https") {
    using namespace boost::spirit::x3;
    auto service = ':' >> +~char_(":") >> eoi;
    auto host = '[' >> *~char_(']') >> ']'  // e.g. for IPV6
                | raw[*("::" | (char_ - service))];

    std::tuple<std::string, std::string> result;
    parse(begin(address_spec), end(address_spec),
          expect[host >> (service | attr(default_service))], result);

    return result;
}

ProgramParams parse_program_params(int argc, char **av) {
    po::options_description desc("Allowed options");
    desc.add_options()("help,h", "produce help message")(
        "port,p", po::value<uint16_t>(), "port")(
        "player-name,n", po::value<std::string>(), "player-name")(
        "gui-address,d", po::value<std::string>(), "gui-address")(
        "server-address,s", po::value<std::string>(), "server-address");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, av, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << "\n";
        exit(0);
    }

    if (!vm.count("port") || !vm.count("player-name") ||
        !vm.count("server-address") || !vm.count("gui-address")) {
        cerr << "Wrong arguments provided\n";
        cerr << desc << "\n";
        exit(1);
    }

    std::tuple<std::string, std::string> server_params =
        parse_server_address(vm["server-address"].as<std::string>());
    addrinfo *server_address;
    getaddrinfo(std::get<0>(server_params).c_str(), std::get<1>(server_params).c_str(), NULL,
                &server_address);

    std::tuple<std::string, std::string> gui_params =
        parse_server_address(vm["gui-address"].as<std::string>());
    addrinfo *gui_address;
    getaddrinfo(std::get<0>(gui_params).c_str(), std::get<1>(gui_params).c_str(), NULL,
                &gui_address);

    ProgramParams program_params(vm["player-name"].as<std::string>(),
                                 vm["port"].as<uint16_t>(),
                                 server_address->ai_addr, gui_address->ai_addr);
    return program_params;
}

int main(int argc, char **argv) { 
    ProgramParams program_params = parse_program_params(argc, argv);

    // test zeby zobaczyc czy mnie z czyms polaczy
    int socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(socket_fd, program_params.get_server_address(), sizeof(program_params.get_server_address()));
    const char* message = "chuj";
    size_t message_length = strlen(message);
    ssize_t sent_length = send(socket_fd, message, message_length, 0);
    cout << "sent " << sent_length << " bytes\n";
    return 0; 
}
