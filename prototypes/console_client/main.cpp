#include <iostream>

#include <asio.hpp>

#include <rxcpp/rx.hpp>

using asio::ip::udp;

/*
 * Simple implementation of udp server
 *
 * Run ./console_client
 * Type 'q' to quit
 * For send the message to the server: echo -n "message" > /dev/udp/localhost/7777
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
            udp::socket socket(service, udp::endpoint(udp::v4(), port));
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
    const uint16_t port = 7777;
    const size_t max_packet_length = 1024;

    auto subscription = integra::udp_server(port, max_packet_length)
        .subscribe_on(rxcpp::observe_on_new_thread())
        .subscribe([](integra::udp_packet msg){
            std::cout << " Received from: "
                      << msg.sender.address().to_string()
                      << ":" << msg.sender.port()
                      << ", message: "
                      << std::string(msg.data.begin(), msg.data.end())
                      << std::endl;
        });

    while (getchar() != 'q');

    subscription.unsubscribe();

    return 0;
}