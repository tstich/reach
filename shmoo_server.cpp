#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/async_result.hpp>

#include <Config.h>
#include <Message.h>

#define BOOST_LOG_DYN_LINK 1
#include <boost/log/trivial.hpp>


using boost::asio::ip::udp;
using namespace boost::asio;


class ReachServer
{
public:
    ReachServer(boost::asio::io_service& io_service)
    : socket_(io_service, udp::endpoint(udp::v4(), REACH_PORT))
    {
    }

    void receiveMessage(boost::asio::yield_context yield)
    {
        for(;;)
        {
            boost::system::error_code error;
            size_t messageSize = socket_.async_receive_from(
                boost::asio::buffer(recv_buffer_), remote_endpoint_, yield[error]);

            BOOST_LOG_TRIVIAL(info) << "Received Message";

            if (!error || error == boost::asio::error::message_size)
            {
              auto message = Message::fromBuffer(recv_buffer_.data(), messageSize);
              size_t packetSize = 8 * 1024;

              switch( message->type() ) {
                case Message::REQ_FILE_PACKETS: {
                    boost::asio::deadline_timer throttle_timer(socket_.get_io_service());

                    BOOST_LOG_TRIVIAL(info) << "Got Packet Request: " << message->packets().elementCount();
                    uint8_t payloadData[packetSize];

                    for( int64_t packetId : message->packets() ) {
                        throttle_timer.expires_from_now(boost::posix_time::microseconds(100));

                        for( int32_t i = 0; i < packetSize; ++i) {
                            payloadData[i] = rand() % 255;
                        }

                        size_t payloadSize = packetSize;

                        auto response = Message::createFilePacket(message->ufid(), packetId, payloadData, payloadSize);
                        socket_.async_send_to(response->asBuffer(), remote_endpoint_, yield);
                        throttle_timer.async_wait(yield);
                    }
                    break;
                }
                }
            }
        }
    }

private:
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    boost::array<uint8_t, 1024000> recv_buffer_;

};

int main()
{
  try
  {
    boost::asio::io_service io_service;
    ReachServer server(io_service);
    boost::asio::spawn(io_service, [&](yield_context yield) {
      server.receiveMessage(yield);
    });

    io_service.run();
}
catch (std::exception& e)
{
    std::cerr << e.what() << std::endl;
}

return 0;
}
