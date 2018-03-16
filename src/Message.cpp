#include <Message.h>

#define BOOST_LOG_DYN_LINK 1
#include <boost/log/trivial.hpp>

Message::Message(Message::TYPE type) :
m_type(type)
{

}

std::unique_ptr<Message> Message::create_ping()
{
	return std::unique_ptr<Message>(new Message(PING));
}

std::unique_ptr<Message> Message::fromBuffer(const uint8_t* data)
{
	std::unique_ptr<Message> message(new Message(static_cast<TYPE>(data[0])));
	BOOST_LOG_TRIVIAL(debug) << "Message::fromBuffer: " << message->m_type;
	return message;
}

// Create a composite buffer for the Message
std::vector<boost::asio::const_buffer> Message::asBuffer() const {

	std::vector<boost::asio::const_buffer> composite_buffer;

	composite_buffer.push_back(boost::asio::const_buffer(&m_type, 1));

	return composite_buffer;
}