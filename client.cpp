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
#include <map>

using boost::asio::ip::udp;
using namespace boost::asio;

class ReachClient
{
public:
  ReachClient(boost::asio::io_service& io_service)
    : m_socket(new udp::socket(io_service)), 
      m_receiverEndpoint(udp::v4(), REACH_PORT),
      m_nextUfid(0)
  {
    m_socket->open(udp::v4());
  }

  void receiveMessage(boost::asio::yield_context yield) {
    boost::array<uint8_t, MAX_MESSAGE_SIZE> buffer;

    for(;;) {
      size_t messageSize = m_socket->async_receive_from(
        boost::asio::buffer(buffer), 
        m_receiverEndpoint, yield);

      std::shared_ptr<Message> receivedMessage = Message::Message::fromBuffer(
        buffer.data(), messageSize);

//      BOOST_LOG_TRIVIAL(info) << "Received message for ID:" << receivedMessage->ufid();

      auto callbackIt = m_receiveCallback.find(receivedMessage->ufid());
      if( callbackIt != m_receiveCallback.end() ) {
        (callbackIt->second)(receivedMessage);
      }
    }
  }

  void fetchFile(const char* path, boost::asio::yield_context yield) {

    boost::system::error_code ec;
    uint64_t ufid = m_nextUfid++;
    auto fileRequestMessage = Message::createReqFile(ufid, path);

    boost::asio::deadline_timer timer(m_socket->get_io_service());

    std::shared_ptr<Message> fileInfoMessage;

    m_receiveCallback[ufid] = [&](std::shared_ptr<Message> receivedMessage)
    {
      // Store the fileInfoMessage when it is received
      fileInfoMessage = receivedMessage;

      // Cancel timeout timer -> unblock async_wait
      timer.cancel();
    };

    for( int i = 0; i < SEND_RETRY && !fileInfoMessage; ++i) {

      BOOST_LOG_TRIVIAL(info) << "Sending fileRequest Message: " << fileRequestMessage->ufid();
      m_socket->async_send_to(fileRequestMessage->asBuffer(), m_receiverEndpoint, yield[ec]);

      timer.expires_from_now(boost::posix_time::milliseconds(RECEIVE_TIMEOUT));
      
      timer.async_wait(yield[ec]);       
    }
    
    if( !fileInfoMessage ) {
      BOOST_LOG_TRIVIAL(error) << "No response from server";
      return;
    } 

    BOOST_LOG_TRIVIAL(info) << "Received fileInfo: " << fileInfoMessage->packetCount();
    Range filePackets = Range(0, fileInfoMessage->packetCount());

    m_receiveCallback[ufid] = [&](std::shared_ptr<Message> receivedMessage)
    {
      filePackets.subtract(receivedMessage->packetId());
      if( filePackets.elementCount() == 0 ) {
        timer.cancel();
      }
    };
    
    BOOST_LOG_TRIVIAL(info) << "Sending packetRequest Message: ";

    auto reqFilePacketsMessage = Message::createRequestFilePackets(ufid, filePackets);
    m_socket->async_send_to(reqFilePacketsMessage->asBuffer(), m_receiverEndpoint, yield[ec]);

    timer.expires_from_now(boost::posix_time::milliseconds(RECEIVE_TIMEOUT));
    timer.async_wait(yield[ec]);       
    BOOST_LOG_TRIVIAL(info) << "Packets not received: " << filePackets.elementCount();

  }

private: 
  std::shared_ptr<udp::socket> m_socket;
  udp::endpoint m_receiverEndpoint;

  std::atomic<uint64_t> m_nextUfid;
  std::map<uint64_t, std::function<void(std::shared_ptr<Message>)> > m_receiveCallback;
};

int main()
{
  try
  {
    boost::asio::io_service io_service;
    ReachClient client(io_service);
    boost::asio::spawn(io_service, [&](yield_context yield)
    {
      client.fetchFile("/some/random/file", yield);
    });
    boost::asio::spawn(io_service, [&](yield_context yield) {
      client.receiveMessage(yield);
    });
    
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}

