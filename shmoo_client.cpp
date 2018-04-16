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
      m_nextReqid(1),
      m_shutdown(0)
  {
    m_socket->open(udp::v4());
    boost::asio::spawn(io_service, [&](yield_context yield) {
      receiveMessage(yield);
    });
  }

  void shutdown() {
    m_shutdown++;
    BOOST_LOG_TRIVIAL(info) << "===================================================";
    BOOST_LOG_TRIVIAL(info) << "Shutdown: " << m_shutdown;

    if( m_shutdown >= 10 ) {
      m_socket->cancel();
    }
  }


  void fetchFileAsync(const char* path) {
    std::shared_ptr<std::vector<size_t> > ranges(new std::vector<size_t>());
    for( size_t i = 0; i < REQUEST_PREFETCH; i += REQUEST_SIZE) {
      ranges->push_back(i);
    }



    boost::asio::spawn(m_socket->get_io_service(), [ranges, this, path](yield_context yield)
    {
      uint64_t ufid = m_nextUfid++;

      boost::system::error_code ec;
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
      

      m_receiveCallback.erase(ufid);

      if( !fileInfoMessage ) {
        BOOST_LOG_TRIVIAL(error) << "No response from server";
        return;
      } 

      size_t fileSize = fileInfoMessage->fileSize();
      boost::iostreams::mapped_file_params fileParams;
      fileParams.path = std::string("client_received.bin");
      fileParams.new_file_size = fileSize;
      m_fileSink.reset(new boost::iostreams::mapped_file_sink(fileParams));

      size_t totalPackets = (fileSize + fileInfoMessage->packetSize() - 1) / fileInfoMessage->packetSize();
      BOOST_LOG_TRIVIAL(info) << "Total Packet Count : " << totalPackets;

      for( size_t i = REQUEST_PREFETCH; i < totalPackets; i += REQUEST_SIZE) {
        ranges->push_back(i);
      }

    });


    for( size_t i = 0; i < 10; i++) {
      boost::asio::spawn(m_socket->get_io_service(), [this, ranges](yield_context yield)
      {
        while( !ranges->empty() ) {
          size_t startOffset = ranges->back();
          ranges->pop_back();
          size_t remainingPackets = packetRequest(Range(startOffset, startOffset + REQUEST_SIZE), yield);
          if( remainingPackets > 0 ) {
            ranges->push_back(startOffset);
          }
        }
        shutdown();
      });
    }

  }

private:
  size_t packetRequest(Range requestRange, boost::asio::yield_context yield) {
    uint64_t ufid = m_nextUfid++;

    boost::system::error_code ec;

    // Request parameters
    double received = .0;
    std::vector<high_resolution_clock::time_point> receiveTime;
    boost::asio::deadline_timer timer(m_socket->get_io_service());
    BOOST_LOG_TRIVIAL(info) << "===================================================";
    BOOST_LOG_TRIVIAL(info) << "Sending PacketRequest: " << requestRange.elementCount() << " - " << ufid;

    // Receive Logic
    m_receiveCallback[ufid] = [&](std::shared_ptr<Message> receivedMessage)
    {
      if( receivedMessage->type() == Message::FILE_PACKET ) {
        char* dest = m_fileSink->data() + receivedMessage->packetId() * 8 * 1024;
        memcpy(dest, receivedMessage->payloadData(), receivedMessage->payloadSize());

        receiveTime.push_back(high_resolution_clock::now());
        requestRange.subtract(receivedMessage->packetId());
        received += 1.0;
        if( requestRange.elementCount() == 0) {
          timer.cancel();
        }
      }
    };

    // Send packet request and wait for max 10 seconds
    auto reqFilePacketsMessage = Message::createRequestFilePackets(ufid, requestRange);
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    m_socket->async_send_to(reqFilePacketsMessage->asBuffer(), m_receiverEndpoint, yield[ec]);
    timer.expires_from_now(boost::posix_time::milliseconds(200));
    timer.async_wait(yield[ec]);
    
    m_receiveCallback.erase(ufid);

    size_t remainingPackets = requestRange.elementCount();
    // Plot Results

    BOOST_LOG_TRIVIAL(info) << "Remaining Packets: " << remainingPackets;
    if( receiveTime.size() > 0 ) {
      BOOST_LOG_TRIVIAL(info) << "Remaining Details:" << requestRange.toString();
      BOOST_LOG_TRIVIAL(info) << "Latency For First:" << duration_cast<microseconds>( receiveTime.front() - t1 ).count();
      BOOST_LOG_TRIVIAL(info) << "Latency Till Last:" << duration_cast<microseconds>( receiveTime.back() - t1 ).count();
      BOOST_LOG_TRIVIAL(info) << "Net Throughput Till Last:" << (8 * 1024 * received) / duration_cast<microseconds>( receiveTime.back() - receiveTime.front() ).count();
    }

    return remainingPackets;
  }

  void receiveMessage(boost::asio::yield_context yield) {
    boost::array<uint8_t, MAX_MESSAGE_SIZE> buffer;

    while(m_shutdown < 10) {
      boost::system::error_code ec;

      size_t messageSize = m_socket->async_receive_from(
        boost::asio::buffer(buffer),
        m_receiverEndpoint, yield[ec]);

      if( ec == boost::system::errc::success ) {
        std::shared_ptr<Message> receivedMessage = Message::Message::fromBuffer(
          buffer.data(), messageSize);


        auto callbackIt = m_receiveCallback.find(receivedMessage->ufid());
        if( callbackIt != m_receiveCallback.end() ) {
          (callbackIt->second)(receivedMessage);
        }
      }
    }
  }


private:
  std::shared_ptr<udp::socket> m_socket;
  udp::endpoint m_receiverEndpoint;

  std::atomic<uint64_t> m_nextUfid;
  std::atomic<uint64_t> m_nextReqid;
  std::map<uint64_t, std::function<void(std::shared_ptr<Message>)> > m_receiveCallback;
  std::shared_ptr<boost::iostreams::mapped_file_sink> m_fileSink;

  size_t m_shutdown;
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
    client.fetchFileAsync(vm["file"].as<std::string>().c_str());

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}

