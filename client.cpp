#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <config.h>
#include <Message.h>

using boost::asio::ip::udp;

class ReachClient
{
public:
  ReachClient(boost::asio::io_service& io_service)
    : socket_(io_service), 
      receiver_endpoint_(udp::v4(), REACH_PORT)
  {
    socket_.open(udp::v4());

    start_send();
  }

private:
  void start_send()
  {
    std::shared_ptr<Message> message = Message::createPing(); 

    socket_.async_send_to(message->asBuffer(), receiver_endpoint_,
        boost::bind(&ReachClient::handle_send, this, message,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }


  void handle_send(std::shared_ptr<Message> /*message*/,
      const boost::system::error_code& /*error*/,
      std::size_t /*bytes_transferred*/)
  {
  }

  udp::socket socket_;
  udp::endpoint receiver_endpoint_;
  boost::array<uint8_t, MAX_MESSAGE_SIZE> recv_buffer_;
};

int main()
{
  try
  {
    boost::asio::io_service io_service;
    ReachClient server(io_service);
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}

