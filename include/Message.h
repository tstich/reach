#pragma once

#include <memory>
#include <boost/asio.hpp>

class Message {
public:
	enum TYPE : uint8_t {PING, ALIVE, REQ_FILE, FILE_INFO, REQ_FILE_PACKETS, FILE_PACKET};

	static std::unique_ptr<Message> create_ping();
	static std::unique_ptr<Message> fromBuffer(const uint8_t* data);

	std::vector<boost::asio::const_buffer> asBuffer() const;


protected:
	Message(TYPE type);

private:	
	TYPE m_type;
};