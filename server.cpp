#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

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
          size_t packetSize = 8 * 1024;

          switch( message->type() ) {
            case Message::REQ_FILE: {
                BOOST_LOG_TRIVIAL(info) << "Opening File: " << message->path();
                m_fileSource.reset(new boost::iostreams::mapped_file_source(message->path()));

                auto response = Message::createFileInfo(message->ufid(), m_fileSource->size(), packetSize);
                socket_.async_send_to(response->asBuffer(), remote_endpoint_,
                    ReachServer::noop_handler);
                break;
            }

            case Message::REQ_FILE_PACKETS: {
                boost::asio::deadline_timer m_throttleTimer(socket_.get_io_service());
                //BOOST_LOG_TRIVIAL(error) << "Sending Packets:" << message->packets().elementCount();

                for( int64_t packetId : message->packets() ) {
                    const char* payloadData = m_fileSource->data() + (packetId * packetSize);
                    size_t payloadSize = std::min(packetSize, m_fileSource->size() - (packetId * packetSize));

                    auto response = Message::createFilePacket(message->ufid(), packetId, payloadData, payloadSize);
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
    boost::array<uint8_t, 1024000> recv_buffer_;
    std::shared_ptr<boost::iostreams::mapped_file_source> m_fileSource;
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
