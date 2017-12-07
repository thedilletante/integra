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

int main(int argc, char* argv[]) {

    struct message {
        std::vector<uint8_t> data;
        udp::endpoint sender;
    };

    auto server = rxcpp::observable<>::create<message>(
        [](rxcpp::subscriber<message> subscriber){
            constexpr const static size_t max_length = 1024;
            uint8_t data[max_length] = {0};
            asio::io_service service;
            udp::socket socket(service, udp::endpoint(udp::v4(), 7777));
            udp::endpoint sender_endpoint;

            std::function<void()> do_receive;
            do_receive = [&](){
                socket.async_receive_from(asio::buffer(data, max_length), sender_endpoint,
                    [&](std::error_code ec, std::size_t bytes_recvd) {
                        if (!subscriber.is_subscribed()) {
                            return;
                        } else if (!ec && bytes_recvd > 0) {
                            const message msg { std::vector<uint8_t>(data, data + bytes_recvd), sender_endpoint };
                            subscriber.on_next(msg);
                        }
                        do_receive();
                    }
                );
            };
            do_receive();

            subscriber.add(rxcpp::make_subscription([&service](){
                service.stop();
                std::cout << std::this_thread::get_id() << " Unsubscibed, stop" << std::endl;
            }));

            try {
                std::cout << std::this_thread::get_id() << " Start the service" << std::endl;
                service.run();
                if (subscriber.is_subscribed()) {
                    subscriber.on_completed();
                }
                std::cout << std::this_thread::get_id() << " Stop the service" << std::endl;
            } catch (std::exception& e) {
                std::cout << std::this_thread::get_id() << " Exception: " << e.what() << std::endl;
                if (subscriber.is_subscribed()) {
                    subscriber.on_error(std::make_exception_ptr(e));
                }
            }
        }
    );

    auto subscription = server
        .subscribe_on(rxcpp::observe_on_new_thread())
        .subscribe([](message msg){
            std::cout << std::this_thread::get_id()
                      << " Received from: "
                      << msg.sender.address().to_string()
                      << ", message: "
                      << std::string(msg.data.begin(), msg.data.end())
                      << std::endl;
        });


    std::cout << std::this_thread::get_id() << " Enter loop" << std::endl;

    while (getchar() != 'q');

    std::cout << std::this_thread::get_id() << " Gracefull shutdown" << std::endl;
    subscription.unsubscribe();

    return 0;
}