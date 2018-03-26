#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <Config.h>
#include <Message.h>

#define BOOST_LOG_DYN_LINK 1
#include <boost/log/trivial.hpp>


using boost::asio::ip::udp;


class ReachServer
{
public:
    ReachServer(boost::asio::io_service& io_service)
    : socket_(io_service, udp::endpoint(udp::v4(), REACH_PORT))
    {
        start_receive();
    }

private:
    void start_receive()
    {
        socket_.async_receive_from(
            boost::asio::buffer(recv_buffer_), remote_endpoint_,
            boost::bind(&ReachServer::handle_receive, this,
              boost::asio::placeholders::error,
              boost::asio::placeholders::bytes_transferred));
    }

    static void noop_handler(const boost::system::error_code& error,
      std::size_t messageSize) {;}

    void handle_receive(const boost::system::error_code& error,
      std::size_t messageSize)
    {
        if (!error || error == boost::asio::error::message_size)
        {
          auto message = Message::fromBuffer(recv_buffer_.data(), messageSize);

          switch( message->type() ) {
            case Message::REQ_FILE: {
                auto response = Message::createFileInfo(message->ufid(), 1024 * 100, 1024);
                socket_.async_send_to(response->asBuffer(), remote_endpoint_, 
                    ReachServer::noop_handler);
                break;                
            }

            case Message::REQ_FILE_PACKETS: {
                std::vector<uint8_t> payload(1024);
                boost::asio::deadline_timer m_throttleTimer(socket_.get_io_service());
                BOOST_LOG_TRIVIAL(error) << "Sending Packets:" << message->packets().elementCount();

                for( int64_t packetId : message->packets() ) {
                    auto response = Message::createFilePacket(message->ufid(), packetId, payload);
                    socket_.async_send_to(response->asBuffer(), remote_endpoint_, 
                        ReachServer::noop_handler);

                    // m_throttleTimer.expires_from_now(boost::posix_time::microseconds(10));
                    // m_throttleTimer.wait();
                }
                break;
            }
            }
        }
        start_receive();
    }

private:
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    boost::array<uint8_t, 102400> recv_buffer_;
};

int main()
{
  try
  {
    boost::asio::io_service io_service;
    ReachServer server(io_service);
    io_service.run();
}
catch (std::exception& e)
{
    std::cerr << e.what() << std::endl;
}

return 0;
}
