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


        auto callbackIt = m_receiveCallback.find(receivedMessage->ufid());
        if( callbackIt != m_receiveCallback.end() ) {
          (callbackIt->second)(receivedMessage);
        }
      }
    }
  }

  void packetTest(size_t numPackets, boost::asio::yield_context yield) {

    BOOST_LOG_TRIVIAL(info) << "Sending PacketRequest: " << numPackets;
    boost::system::error_code ec;

    // Request parameters
    uint64_t ufid = m_nextUfid++;
    Range requestRange(0,numPackets);
    std::vector<high_resolution_clock::time_point> receiveTime(numPackets);
    boost::asio::deadline_timer timer(m_socket->get_io_service());

    // Receive Logic
    m_receiveCallback[ufid] = [&](std::shared_ptr<Message> receivedMessage)
    {
      if( receivedMessage->type() == Message::FILE_PACKET ) {
        receiveTime[receivedMessage->packetId()] = high_resolution_clock::now();
        requestRange.subtract(receivedMessage->packetId());
        if( requestRange.elementCount() == 0) {
          timer.cancel();
        }
      }
    };

    // Send packet request and wait for max 10 seconds
    auto reqFilePacketsMessage = Message::createRequestFilePackets(ufid, requestRange);
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    m_socket->async_send_to(reqFilePacketsMessage->asBuffer(), m_receiverEndpoint, yield[ec]);
    timer.expires_from_now(boost::posix_time::seconds(10));
    timer.async_wait(yield[ec]);

    // Plot Results
    BOOST_LOG_TRIVIAL(info) << "Remaining Packets: " << requestRange.elementCount();

  }

  void packetTestShmoo(size_t numPackets, boost::asio::yield_context yield) {
    size_t stepSize = numPackets / 10;

    for( size_t i = stepSize; i<=numPackets; i+= stepSize) {
        packetTest(i, yield);
    }
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
      ("packets", po::value<size_t>()->required(), "Packets to shmoo")
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
    boost::asio::spawn(io_service, [&](yield_context yield) {
      client.receiveMessage(yield);
    });
    boost::asio::spawn(io_service, [&](yield_context yield)
    {
      client.packetTestShmoo(vm["packets"].as<size_t>() , yield);
    });

    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}

