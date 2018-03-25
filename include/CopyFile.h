#pragma once

#include <boost/asio.hpp>
#include <boost/array.hpp>

#include <Message.h>

using boost::asio::ip::udp;

class CopyFile {
public:
	CopyFile(const char* path, 
		std::shared_ptr<udp::socket> socket,
		udp::endpoint& receiverEndpoint);

private:
	void sendRequestFile();
	void sendRequestFileComplete(  
  		const boost::system::error_code& error,
  		std::size_t messageSize);

	void receiveFileInfo( const boost::system::error_code& error,
  		std::size_t messageSize);
	void receiveFileInfoTimeOut(const boost::system::error_code& error);

	void sendRequestFilePackets();
	void sendRequestFilePacketsComplete(  
  		const boost::system::error_code& error,
  		std::size_t messageSize);
	void sendRequestFilePacketsTimeOut(const boost::system::error_code& error);

	void receiveFilePacket(const boost::system::error_code& error,
  		std::size_t messageSize);

private:
	std::shared_ptr<boost::asio::ip::udp::socket> m_socket;
	boost::asio::ip::udp::endpoint m_receiverEndpoint;
	uint64_t m_errorCount;
	boost::asio::deadline_timer m_receiveTimer;

	boost::array<uint8_t, MAX_MESSAGE_SIZE> m_receiveBuffer;
	std::shared_ptr<Message> m_reqFileMessage;
	std::shared_ptr<Message> m_fileInfoMessage;

	Range m_outstandingPackets;
	uint64_t m_packetsInFlight;
};