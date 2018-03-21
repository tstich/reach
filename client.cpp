#include <iostream>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <Config.h>
#include <CopyFile.h>

#define BOOST_LOG_DYN_LINK 1
#include <boost/log/trivial.hpp>

using boost::asio::ip::udp;

class ReachClient
{
public:
  ReachClient(boost::asio::io_service& io_service)
    : m_socket(new udp::socket(io_service)), 
      m_receiverEndpoint(udp::v4(), REACH_PORT)
  {
    m_socket->open(udp::v4());

    m_randomFile.reset(new CopyFile("/random/file", m_socket, m_receiverEndpoint) );
  }

private: 
  std::shared_ptr<CopyFile> m_randomFile;
  std::shared_ptr<udp::socket> m_socket;
  udp::endpoint m_receiverEndpoint;
  boost::array<uint8_t, MAX_MESSAGE_SIZE> recv_buffer_;
};

int main()
{
  try
  {
    boost::asio::io_service io_service;
    // boost::asio::io_service::work work(io_service);    

    ReachClient client(io_service);
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}

