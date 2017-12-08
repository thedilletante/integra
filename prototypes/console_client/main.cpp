#include <iostream>
#include <string>
#include <vector>
#include <regex>

#include <asio.hpp>

#include <rxcpp/rx.hpp>

using asio::ip::udp;

/*
 * Simple implementation of udp echo server
 *
 * Run ./console_client
 * Type 'q' to quit
 * For send the message to the server: echo -n "message" > /dev/udp/localhost/7777
 * Or to start interractive session: nc 127.0.0.1 7777 -u
 *    server will print incoming messages
 *    so you can grab the port from the log and start listening udp to see echoing: nc 127.0.0.1 <port> -ul
 */

namespace integra {

struct udp_packet {
    std::vector<uint8_t> data;
    udp::endpoint sender;
};

auto udp_server(uint16_t port, size_t max_packet_length) {
    return rxcpp::observable<>::create<udp_packet>(
        [port, max_packet_length] (rxcpp::subscriber<udp_packet> subscriber) {
            // setup the environment
            std::vector<uint8_t> data(max_packet_length);
            asio::io_service service;
            const auto proto = udp::v4();
            udp::socket socket(service, proto);
            asio::socket_base::reuse_address reuse_address(true);
            socket.set_option(reuse_address);
            socket.bind(udp::endpoint(proto, port));

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

    asio::io_service service;
    udp::socket socket(service, udp::v4());
    auto subscription = integra::udp_server(port, max_packet_length)
        .subscribe_on(rxcpp::observe_on_new_thread())
        .subscribe([&service, &socket](integra::udp_packet msg){
            std::cout << "Received from: "
                      << msg.sender.address().to_string()
                      << ":" << msg.sender.port()
                      << ", message: "
                      << std::string(msg.data.begin(), msg.data.end())
                      << std::endl;
        });

    for (std::string line; std::getline(std::cin, line);) {
        if ("quit" == line) {
            break;
        } else {
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
            socket.send_to(asio::buffer(message), endpoint);
        }
    }

    subscription.unsubscribe();

    return 0;
}