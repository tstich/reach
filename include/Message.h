#pragma once

#include <memory>
#include <boost/asio.hpp>

class Message {
public:
	enum TYPE : uint8_t {ACK, PING, ALIVE, REQ_FILE, FILE_INFO, REQ_FILE_PACKETS, FILE_PACKET};

	// Message Specific Create
	static std::shared_ptr<Message> createPing();

	// Message from Buffer
	static std::shared_ptr<Message> fromBuffer(const uint8_t* data);

	// Message to Buffer
	std::vector<boost::asio::const_buffer> asBuffer() const;

	// Meta
	bool isChecked() const;

protected:
	Message(TYPE type);

private:	
	TYPE m_type;
	uint64_t m_version;
};