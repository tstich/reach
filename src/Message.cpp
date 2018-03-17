#include <Message.h>

#define BOOST_LOG_DYN_LINK 1
#include <boost/log/trivial.hpp>

Message::Message(Message::TYPE type) :
m_type(type)
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


bool Message::isChecked() const 
{
	return m_type == REQ_FILE || m_type == FILE_INFO;
}

std::shared_ptr<Message> Message::fromBuffer(const uint8_t* data)
{
	std::shared_ptr<Message> message(new Message(*reinterpret_cast<const TYPE*>(data)));
	data += sizeof(TYPE);
	BOOST_LOG_TRIVIAL(debug) << "Message::fromBuffer: " << message->m_type;

	if( message->m_type == ACK ) {
		message->m_messageId = *reinterpret_cast<const uint64_t*>(data);
		data += sizeof(uint64_t);
	} 	

	return message;
}

// Create a composite buffer for the Message
std::vector<boost::asio::const_buffer> Message::asBuffer() const {

	std::vector<boost::asio::const_buffer> composite_buffer;


	composite_buffer.push_back(boost::asio::const_buffer(&m_type, sizeof(m_type)));

	if( m_type == ACK ) {
		composite_buffer.push_back(boost::asio::const_buffer(&m_messageId, sizeof(m_messageId)));
	} 

	return composite_buffer;
}