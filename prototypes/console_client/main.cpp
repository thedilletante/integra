#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <future>

#include <asio.hpp>

#include <rxcpp/rx.hpp>

#include <stun++/message.h>

using asio::ip::udp;

/*
 * Simple implementation of udp server
 *
 * Run ./console_client <port_number>
 * Supported commands:
 * `quit` - to quit
 * `send <ip> <port> <message>` - to send the message
 */

namespace integra {

struct udp_packet {
    std::vector<uint8_t> data;
    udp::endpoint sender;
};

auto udp_server(uint16_t port, size_t max_packet_length, std::promise<udp::socket*>& opened_socket_promise) {
    return rxcpp::observable<>::create<udp_packet>(
        [port, max_packet_length, &opened_socket_promise] (rxcpp::subscriber<udp_packet> subscriber) {
            // setup the environment
            std::vector<uint8_t> data(max_packet_length);
            asio::io_service service;
            const auto proto = udp::v4();
            udp::socket socket(service, proto);
            asio::socket_base::reuse_address reuse_address(true);
            socket.set_option(reuse_address);
            socket.bind(udp::endpoint(proto, port));

            opened_socket_promise.set_value(&socket);

            udp::endpoint sender_endpoint;

            // stop asio service on unsubscribe
            subscriber.add(rxcpp::make_subscription([&service](){ service.stop(); }));

            // setup on receive data callback
            using recursive_receive_function = std::function<void()>;
            const auto do_receive =
                [&socket, &data, &sender_endpoint, &subscriber] (const recursive_receive_function& recurse) {
                    socket.async_receive_from(
                        asio::buffer(data.data(), data.size()),
                        sender_endpoint,
                        [&subscriber, &data, &sender_endpoint, &recurse] (std::error_code ec, std::size_t bytes_recvd) {
                            if (!subscriber.is_subscribed()) {
                                return;
                            } else if (!ec && bytes_recvd > 0) {
                                subscriber.on_next(udp_packet{
                                    std::vector<uint8_t>(data.begin(), data.begin() + bytes_recvd),
                                    sender_endpoint
                                });
                            }
                            recurse();
                        }
                    );
                };
            recursive_receive_function recursive_do_receive;
            recursive_do_receive = [&do_receive, &recursive_do_receive] () {
                do_receive(recursive_do_receive);
            };

            // put the first operation into the service
            recursive_do_receive();

            // run the service
            try {
                service.run();
                if (subscriber.is_subscribed()) {
                    subscriber.on_completed();
                }
            } catch (std::exception& e) {
                if (subscriber.is_subscribed()) {
                    subscriber.on_error(std::make_exception_ptr(e));
                }
            }
        }
    );
}

}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <port>" << std::endl;
        return 0;
    }

    const uint16_t port = std::atoi(argv[1]);
    const size_t max_packet_length = 1024;
    std::promise<udp::socket*> socket_promise;
    auto subscription = integra::udp_server(port, max_packet_length, socket_promise)
        .subscribe_on(rxcpp::observe_on_new_thread())
        .subscribe([](integra::udp_packet msg){

            stun::message message { msg.data.begin(), msg.data.end() };

            // Check if this is a STUN message
            if (message.verify() && message.type() == stun::message::binding_response) {
                std::cout << "Received from: "
                          << msg.sender.address().to_string()
                          << ":" << msg.sender.port()
                          << ", stun response: " << std::endl;

                // Iterate over the message attributes
                using namespace stun::attribute;
                for (stun::message::iterator i = message.begin(), ie = message.end(); i != ie; i++) {
                    // First, check the attribute type
                    switch (i->type()) {
                    case type::software:
                        std::cout << " software: " << i->to<type::software>().to_string() << std::endl;
                        break;
                    case type::username:
                        std::cout << " username: " << i->to<type::username>().to_string() << std::endl;
                        break;
                    case type::mapped_address: {
                        sockaddr_storage address;
                        socklen_t client_len = sizeof(sockaddr_storage);
                        if (i->to<type::mapped_address>().to_sockaddr((sockaddr*)&address)) {
                            char hoststr[NI_MAXHOST];
                            char portstr[NI_MAXSERV];

                            if (0 == getnameinfo((struct sockaddr *)&address,
                                                 client_len, hoststr, sizeof(hoststr), portstr, sizeof(portstr),
                                                 NI_NUMERICHOST | NI_NUMERICSERV)) {
                                std::cout << " mapped: " << hoststr << ":" << portstr << std::endl;
                            }
                        }
                        break;
                    }
                    case type::xor_mapped_address: {
                        sockaddr_storage address;
                        socklen_t client_len = sizeof(sockaddr_storage);
                        if (i->to<type::xor_mapped_address>().to_sockaddr((sockaddr*)&address)) {
                            char hoststr[NI_MAXHOST];
                            char portstr[NI_MAXSERV];

                            if (0 == getnameinfo((struct sockaddr *)&address,
                                                 client_len, hoststr, sizeof(hoststr), portstr, sizeof(portstr),
                                                 NI_NUMERICHOST | NI_NUMERICSERV)) {
                                std::cout << " xor_mapped: " << hoststr << ":" << portstr << std::endl;
                            }
                        }
                        break;
                    }
                    default:
                        std::cout << " some more attribute" << std::endl;
                    }
                }
            } else {
                std::cout << "Received from: "
                          << msg.sender.address().to_string()
                          << ":" << msg.sender.port()
                          << ", message: "
                          << std::string(msg.data.begin(), msg.data.end())
                          << std::endl;
            }


        });


    udp::socket* socket = socket_promise.get_future().get();
    for (std::string line; std::getline(std::cin, line);) {
        if ("quit" == line) {
            break;
        } else if (line.find("send ") == 0) {
            static const std::regex send_regex("send *([^ ]*) *([^ ]*) *(.*)");
            std::string address;
            std::string port;
            std::string message;

            using regex_iterator = std::regex_token_iterator<std::string::iterator>;
            auto counter = 0;
            for (regex_iterator it { std::begin(line), std::end(line), send_regex, {1, 2, 3} }, end;
                 it != end; ++it, ++counter) {
                switch (counter) {
                    case 0: address = *it; break;
                    case 1: port = *it; break;
                    case 2: message = *it; break;
                    default: break;
                }
            }

            if (address.empty() || port.empty()) {
                std::cout << "invalid command format" << std::endl;
                continue;
            }

            udp::endpoint endpoint(asio::ip::address::from_string(address), std::stoi(port));
            std::cout << "Sending to "
                      << endpoint.address().to_string()
                      << ":" << endpoint.port()
                      << " message: " << message
                      << std::endl;
            socket->send_to(asio::buffer(message), endpoint);
        } else if (line.find("stun ") == 0) {
            static const std::regex send_regex("stun *([^ ]*) *([^ ]*)$");
            std::string address;
            std::string port;

            using regex_iterator = std::regex_token_iterator<std::string::iterator>;
            auto counter = 0;
            for (regex_iterator it { std::begin(line), std::end(line), send_regex, {1, 2} }, end;
                 it != end; ++it, ++counter) {
                switch (counter) {
                    case 0: address = *it; break;
                    case 1: port = *it; break;
                    default: break;
                }
            }

            if (address.empty() || port.empty()) {
                std::cout << "invalid command format" << std::endl;
                continue;
            }

            udp::endpoint endpoint(asio::ip::address::from_string(address), std::stoi(port));
            uint8_t tsx_id[12] = {0};
            stun::message msg {stun::message::binding_request, tsx_id};

            // Add a SOFTWARE attribute
            msg << stun::attribute::software("integra");

            // Add a PRIORITY attribute
            msg << stun::attribute::priority(0x6e0001fful);

            // Add a ICE-CONTROLLED attribute
            msg << stun::attribute::ice_controlled(0x932ff9b151263b36ull);

            // Appends a FINGERPRINT attribute as last attribute
            msg << stun::attribute::fingerprint();

            // Now, send the message
            socket->send_to(asio::buffer(msg.data(), msg.size()), endpoint);
        } else {
            std::cout << "unsupported command" << std::endl;
        }
    }

    subscription.unsubscribe();

    return 0;
}
