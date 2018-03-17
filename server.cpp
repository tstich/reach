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

  void handle_receive(const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/)
  {
    if (!error || error == boost::asio::error::message_size)
    {
      auto message = Message::fromBuffer(recv_buffer_.data());

      // socket_.async_send_to(boost::asio::buffer(*message), remote_endpoint_,
      //     boost::bind(&udp_server::handle_send, this, message,
      //       boost::asio::placeholders::error,
      //       boost::asio::placeholders::bytes_transferred));

      start_receive();
    }
  }

  // void handle_send(boost::shared_ptr<std::string> /*message*/,
  //     const boost::system::error_code& /*error*/,
  //     std::size_t /*bytes_transferred*/)
  // {
  // }

  udp::socket socket_;
  udp::endpoint remote_endpoint_;
  boost::array<uint8_t, MAX_MESSAGE_SIZE> recv_buffer_;
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
