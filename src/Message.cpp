#include <Message.h>

#define BOOST_LOG_DYN_LINK 1
#include <boost/log/trivial.hpp>


uint64_t Message::m_nextMessageId = 0;


Message::Message(Message::TYPE type) :
m_type(type),
m_version(REACH_VERSION)
{
}

std::shared_ptr<Message> Message::createPing()
{
	return std::shared_ptr<Message>(new Message(PING));
}

std::shared_ptr<Message> Message::createACK(uint64_t messageId)
{
	std::shared_ptr<Message> message(new Message(ACK));
	message->m_messageId = messageId;
	return message;
}

std::shared_ptr<Message> Message::createAlive()
{
	std::shared_ptr<Message> message(new Message(ALIVE));
	return message;
}

std::shared_ptr<Message> Message::createReqFile(const char* path)
{
	std::shared_ptr<Message> message(new Message(REQ_FILE));
	message->m_messageId = generateMessageId();
	strncpy(message->m_path, path, PATH_LENGTH);
	return message;
}

std::shared_ptr<Message> Message::createFileInfo(uint64_t ufid, uint64_t packetCount)
{
	std::shared_ptr<Message> message(new Message(FILE_INFO));
	message->m_messageId = generateMessageId();
	message->m_ufid = ufid;
	message->m_packetCount = packetCount;
	return message;
}

uint64_t Message::generateMessageId()
{
	return m_nextMessageId++;
}

bool Message::isChecked() const 
{
	return m_type == REQ_FILE || m_type == FILE_INFO;
}

std::shared_ptr<Message> Message::fromBuffer(const uint8_t* data)
{
	std::shared_ptr<Message> message(new Message(*reinterpret_cast<const TYPE*>(data)));
	data += sizeof(TYPE);
	BOOST_LOG_TRIVIAL(debug) << "Message::fromBuffer: " << message->m_type;

	if( message->m_type == ACK || message->isChecked() ) {
		message->m_messageId = *reinterpret_cast<const uint64_t*>(data);
		data += sizeof(uint64_t);
	} 	

	if( message->m_type == ALIVE ) {
		message->m_version = *reinterpret_cast<const uint64_t*>(data);
		data += sizeof(uint64_t);		
	}

	if( message->m_type == REQ_FILE ) {
		strncpy(message->m_path, reinterpret_cast<const char*>(data), PATH_LENGTH);
		data += PATH_LENGTH;
	}

	if( message->m_type == FILE_INFO ) {
		message->m_ufid = *reinterpret_cast<const uint64_t*>(data);
		data += sizeof(uint64_t);		
		message->m_packetCount = *reinterpret_cast<const uint64_t*>(data);
		data += sizeof(uint64_t);		
	}


	return message;
}

// Create a composite buffer for the Message
std::vector<boost::asio::const_buffer> Message::asBuffer() const {

	std::vector<boost::asio::const_buffer> composite_buffer;


	composite_buffer.push_back(boost::asio::const_buffer(&m_type, sizeof(m_type)));

	if( m_type == ACK || isChecked() ) {
		composite_buffer.push_back(boost::asio::const_buffer(&m_messageId, sizeof(m_messageId)));
	} 

	if( m_type == ALIVE ) {
		composite_buffer.push_back(boost::asio::const_buffer(&m_version, sizeof(m_version)));		
	}

	if( m_type == REQ_FILE ) {
		composite_buffer.push_back(boost::asio::const_buffer(&m_path, sizeof(m_path)));				
	}

	if( m_type == FILE_INFO ) {
		composite_buffer.push_back(boost::asio::const_buffer(&m_ufid, sizeof(m_ufid)));				
		composite_buffer.push_back(boost::asio::const_buffer(&m_packetCount, sizeof(m_packetCount)));				
	}

	return composite_buffer;
}