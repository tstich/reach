#pragma once

#include <boost/asio.hpp>
#include <Message.h>

using boost::asio::ip::udp;

class CopyFile {
public:
	CopyFile(const char* path, 
		std::shared_ptr<udp::socket> socket,
		udp::endpoint& receiverEndpoint);

private:
	void sendRequestFile();
	void sendRequestFileCallback(  
		std::shared_ptr<Message> message,
  		const boost::system::error_code& error,
  		std::size_t bytes_transferred);

	// void receiveFileInfo();
	// void receiveFileInfoCallback();
	void receiveFileInfoTimeOut(const boost::system::error_code& error);

	void sendRequestFilePackets();
	void receiveFilePacket();

private:
	const char* m_path;
	std::shared_ptr<boost::asio::ip::udp::socket> m_socket;
	boost::asio::ip::udp::endpoint m_receiverEndpoint;

	uint64_t m_errorCount;

	boost::asio::deadline_timer m_receiveTimer;
};