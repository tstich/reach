#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include <Message.h>

using boost::asio::ip::udp;

int main()
{
  try
  {
    boost::asio::io_service io_service;

    udp::socket socket(io_service);
    socket.open(udp::v4());
    
    auto message = Message::create_ping(); 
    udp::endpoint receiver_endpoint(udp::v4(), 52123);
    socket.send_to(message->asBuffer(), receiver_endpoint);
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
