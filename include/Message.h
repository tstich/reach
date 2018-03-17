#pragma once

#include <memory>
#include <boost/asio.hpp>

//////////////////////////////
// The message class represents all message types of our protocol. 
// It can be created via special createXXX functions or from a byte string.
// To sent the message via socket use the asBuffer() method.
//////////////////////////////

class Message {
//////////////////////////////
// Types
//////////////////////////////
public:
	enum TYPE : uint8_t {ACK, PING, ALIVE, REQ_FILE, FILE_INFO, REQ_FILE_PACKETS, FILE_PACKET};


//////////////////////////////
// Methods
//////////////////////////////
public:
	// Message Specific Create
	static std::shared_ptr<Message> createACK(uint64_t messageId);
	static std::shared_ptr<Message> createPing();

	// Message from Buffer
	static std::shared_ptr<Message> fromBuffer(const uint8_t* data);

	// Message to Buffer
	std::vector<boost::asio::const_buffer> asBuffer() const;

	// Meta
	bool isChecked() const;
	TYPE type() const { return m_type; }
	uint64_t messageId() const { return m_messageId; }

protected:
	Message(TYPE type);

private:

//////////////////////////////
// Variables
//////////////////////////////
private:	
	TYPE m_type;
	uint64_t m_version;
	uint64_t m_messageId;
};