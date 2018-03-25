#include <iostream>
#include <string>
#include <deque>
#include <chrono>

#include <boost/date_time/posix_time/posix_time.hpp>
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
using namespace std::chrono;

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
    Range outstandingPackets = Range(0, fileInfoMessage->packetCount());

    typedef std::pair<uint32_t, Range> TimedRange;
    std::deque<TimedRange> inflightPackets;    

    uint64_t requestSize = 64;
    uint64_t timeoutCount = requestSize / 2;
    uint64_t throttleCount = 8 * requestSize;

    m_receiveCallback[ufid] = [&](std::shared_ptr<Message> receivedMessage)
    {
      bool cancelTimer = false;

      for( TimedRange &timedRange: inflightPackets ) {
        if( timedRange.second.contains(receivedMessage->packetId())) {
          timedRange.second.subtract(receivedMessage->packetId());
          if( timedRange.second.elementCount() == 0  ) {
            cancelTimer = true;
          } else {
            timedRange.first = 0;            
          }

          break;
        } else {
          if( timedRange.first++ > timeoutCount ) {
            cancelTimer = true;
          }
        }
      } 

      if( cancelTimer ) {
        timer.cancel();
      }
    };

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    uint64_t totalRequested = 0;
    for(;;)
    {
      int64_t totalInflightPackets = 0;
      for( auto it = inflightPackets.begin(); it != inflightPackets.end(); ) {
        int64_t currentPackets = it->second.elementCount();
        totalInflightPackets += currentPackets;

        if( currentPackets == 0 ) {
          // All received
          it = inflightPackets.erase(it);
        } else if( it->first > timeoutCount ) {
          // Detected dropped packets -> request again
          outstandingPackets.add(it->second);
          it = inflightPackets.erase(it);
        } else {
          ++it;
        }
      }
      // BOOST_LOG_TRIVIAL(debug) << "inflightPackets: " << totalInflightPackets;
      // BOOST_LOG_TRIVIAL(debug) << "outstandingPackets: " << outstandingPackets.elementCount();

      if( totalInflightPackets == 0 && outstandingPackets.elementCount() == 0 ) {
        m_receiveCallback.erase(ufid);
        break;
      }

      if( totalInflightPackets < throttleCount && outstandingPackets.elementCount() > 0 ) {
        Range requestRange = outstandingPackets.removeFirstN(requestSize);
        // BOOST_LOG_TRIVIAL(info) << "Sending packetRequest Message: " << requestRange.elementCount();
        totalRequested += requestRange.elementCount();

        inflightPackets.push_back(TimedRange(0, requestRange));
        auto reqFilePacketsMessage = Message::createRequestFilePackets(ufid, requestRange);
        m_socket->async_send_to(reqFilePacketsMessage->asBuffer(), m_receiverEndpoint, yield[ec]);      
      }

      if( totalInflightPackets > requestSize || outstandingPackets.elementCount() < requestSize ) {
        timer.expires_from_now(boost::posix_time::milliseconds(10));
        timer.async_wait(yield[ec]);       

        if( ec == boost::system::errc::success ) {
          for( TimedRange &timedRange: inflightPackets) {
            timedRange.first += timeoutCount / inflightPackets.size();
          }
        }
      }
    }
    high_resolution_clock::time_point t2 = high_resolution_clock::now();    
    auto duration = duration_cast<microseconds>( t2 - t1 ).count();

    BOOST_LOG_TRIVIAL(info) << "Transfer Complete: " << fileInfoMessage->packetCount();
    BOOST_LOG_TRIVIAL(info) << "Total Requested: " << totalRequested;
    BOOST_LOG_TRIVIAL(info) << "Duration: " << duration;
    BOOST_LOG_TRIVIAL(info) << "Throughput: " << (fileInfoMessage->packetCount() * fileInfoMessage->packetSize()) / duration;


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

