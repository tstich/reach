#include <iostream>
#include <string>
#include <deque>
#include <chrono>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/program_options.hpp>

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
  ReachClient(boost::asio::io_service& io_service, boost::asio::ip::address_v4 address)
    : m_socket(new udp::socket(io_service)),
      m_receiverEndpoint(address, REACH_PORT),
      m_nextUfid(1),
      m_shutdown(false)
  {
    m_socket->open(udp::v4());
  }

  void receiveMessage(boost::asio::yield_context yield) {
    boost::array<uint8_t, MAX_MESSAGE_SIZE> buffer;

    while(!m_shutdown) {
      boost::system::error_code ec;

      size_t messageSize = m_socket->async_receive_from(
        boost::asio::buffer(buffer),
        m_receiverEndpoint, yield[ec]);

      if( ec == boost::system::errc::success ) {
        std::shared_ptr<Message> receivedMessage = Message::Message::fromBuffer(
          buffer.data(), messageSize);

  //      BOOST_LOG_TRIVIAL(info) << "Received message for ID:" << receivedMessage->ufid();

        auto callbackIt = m_receiveCallback.find(receivedMessage->ufid());
        if( callbackIt != m_receiveCallback.end() ) {
          (callbackIt->second)(receivedMessage);
        }
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
      if( receivedMessage->type() == Message::FILE_INFO ) {
        // Store the fileInfoMessage when it is received
        fileInfoMessage = receivedMessage;

        // Cancel timeout timer -> unblock async_wait
        timer.cancel();
      }
    };

    for( int i = 0; i < SEND_RETRY && !fileInfoMessage; ++i) {

      BOOST_LOG_TRIVIAL(info) << "Sending fileRequest: " << path;
      m_socket->async_send_to(fileRequestMessage->asBuffer(), m_receiverEndpoint, yield[ec]);

      timer.expires_from_now(boost::posix_time::milliseconds(RECEIVE_TIMEOUT));

      timer.async_wait(yield[ec]);
    }

    if( !fileInfoMessage ) {
      BOOST_LOG_TRIVIAL(error) << "No response from server";
      return;
    }

    BOOST_LOG_TRIVIAL(info) << "Received fileInfo: " << fileInfoMessage->fileSize();

    boost::iostreams::mapped_file_params fileParams;
    fileParams.path = std::string("client_received.bin");
    fileParams.new_file_size = fileInfoMessage->fileSize();

    boost::iostreams::mapped_file_sink fileSink(fileParams);

    size_t packetSize = fileInfoMessage->packetSize();
    size_t packetCount = (fileInfoMessage->fileSize() + packetSize - 1) / packetSize;
    Range outstandingPackets = Range(0, packetCount);

    typedef std::pair<uint32_t, Range> TimedRange;
    std::deque<TimedRange> inflightPackets;

    uint64_t requestSize = 10;
    uint64_t timeoutCount = requestSize / 2;
    uint64_t throttleCount = 32 * requestSize;
    uint64_t totalUnexpectedPackages = 0;

    std::vector<uint64_t> lostSize(requestSize+1, 0);

    m_receiveCallback[ufid] = [&](std::shared_ptr<Message> receivedMessage)
    {
      if( receivedMessage->type() == Message::FILE_PACKET ) {
        char* dest = fileSink.data() + receivedMessage->packetId() * packetSize;
        memcpy(dest, receivedMessage->payloadData(), receivedMessage->payloadSize());

        bool cancelTimer = false;
        bool expectedPackage = false;

        for( TimedRange &timedRange: inflightPackets ) {
          if( timedRange.second.contains(receivedMessage->packetId())) {
            expectedPackage = true;
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

        if( !expectedPackage ) {
          totalUnexpectedPackages++;
        }

        if( cancelTimer ) {
          timer.cancel();
        }
      }
    };

    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    uint64_t totalRequested = 0;
    boost::asio::deadline_timer m_throttleTimer(m_socket->get_io_service());
    m_throttleTimer.expires_from_now(boost::posix_time::microseconds(0));

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
          lostSize[it->second.elementCount()]++;
          outstandingPackets.add(it->second);
          it = inflightPackets.erase(it);
        } else {
          ++it;
        }
      }
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
        m_throttleTimer.async_wait(yield[ec]);
        m_socket->async_send_to(reqFilePacketsMessage->asBuffer(), m_receiverEndpoint, yield[ec]);
        m_throttleTimer.expires_from_now(boost::posix_time::microseconds(1000));
      }

      if( totalInflightPackets > requestSize || outstandingPackets.elementCount() < requestSize ) {
        timer.expires_from_now(boost::posix_time::milliseconds(100));
        timer.async_wait(yield[ec]);

        if( ec == boost::system::errc::success ) {
          BOOST_LOG_TRIVIAL(debug) << "Timeout: " << static_cast<float>(outstandingPackets.elementCount()) / packetCount;
          BOOST_LOG_TRIVIAL(debug) << "inflightPackets: " << totalInflightPackets;
          for( int i=0; i < requestSize+1; ++i) {
            BOOST_LOG_TRIVIAL(info) << "Lost Statistics " << i << ": " << lostSize[i];
          }

          for( TimedRange &timedRange: inflightPackets) {
            timedRange.first += timeoutCount / inflightPackets.size();
          }
        }
      }
    }
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>( t2 - t1 ).count();

    BOOST_LOG_TRIVIAL(info) << "Transfer Complete: " << packetCount;
    BOOST_LOG_TRIVIAL(info) << "Total Requested: " << totalRequested;
    BOOST_LOG_TRIVIAL(info) << "Total Unexpected Packages: " << totalUnexpectedPackages;
    BOOST_LOG_TRIVIAL(info) << "Duration: " << duration;
    BOOST_LOG_TRIVIAL(info) << "Throughput: " << fileInfoMessage->fileSize() / duration;


    m_shutdown = true;
    m_socket->cancel();
  }

private:
  std::shared_ptr<udp::socket> m_socket;
  udp::endpoint m_receiverEndpoint;

  std::atomic<uint64_t> m_nextUfid;
  std::map<uint64_t, std::function<void(std::shared_ptr<Message>)> > m_receiveCallback;

  bool m_shutdown;
};

int main(int argc, char** argv)
{
    namespace po = boost::program_options;
    po::options_description desc("Copy Files via REACH UDP");
    desc.add_options()
      ("help", "Print help messages")
      ("file", po::value<std::string>()->required(), "File on server to be copied")
      ("address", po::value<std::string>(), "Address of the client");

    po::positional_options_description positionalOptions;
    positionalOptions.add("file", 1);

    po::variables_map vm;
    try
    {
      po::store(po::command_line_parser(argc, argv).options(desc).positional(positionalOptions).run(),
                vm);

      if ( vm.count("help")  )
      {
        std::cout << desc << std::endl;
        return 0;
      }

      po::notify(vm); // throws on error, so do after help in case
                      // there are any problems
    }
    catch(po::error& e)
    {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cerr << desc << std::endl;
      return 1;
    }
  try
  {
    boost::asio::io_service io_service;
    std::string address("127.0.0.1");
    if( vm.count("address") ) {
      address = vm["address"].as<std::string>();
    }
    boost::asio::ip::address_v4 targetIP = boost::asio::ip::address_v4::from_string(address);

    ReachClient client(io_service, targetIP);
    boost::asio::spawn(io_service, [&](yield_context yield)
    {
      client.fetchFile(vm["file"].as<std::string>().c_str() , yield);
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

