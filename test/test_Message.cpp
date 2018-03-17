//Link to Boost
#define BOOST_TEST_DYN_LINK

//Define our Module name (prints at testing)
#define BOOST_TEST_MODULE "Message"

//VERY IMPORTANT - include this last
#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>

#include <Message.h>
#include <config.h>

BOOST_AUTO_TEST_CASE( pingMessage )
{
	// UDP Packet as Struct
	#pragma pack(push, 1)
	struct {
		uint8_t type;
	} messageData;
	#pragma pack(pop)

	// Create
	auto message = Message::createPing();

	// Data Layer
	auto messageBuffer = message->asBuffer();
	BOOST_CHECK_EQUAL(sizeof(messageData), boost::asio::buffer_size(messageBuffer));

	boost::asio::buffer_copy(boost::asio::buffer(&messageData, sizeof(messageData)), messageBuffer); 
	
	BOOST_CHECK_EQUAL(messageData.type, Message::PING);

	// Parse
	auto parsedMessage = Message::fromBuffer(reinterpret_cast<uint8_t*>(&messageData));
	BOOST_CHECK_EQUAL(parsedMessage->type(), Message::PING);
}

BOOST_AUTO_TEST_CASE( ackMessage )
{
	// UDP Packet as Struct
	#pragma pack(push, 1)
	struct {
		uint8_t type;
		uint64_t messageId;
	} messageData;
	#pragma pack(pop)

	// Create
	uint64_t testId = 123456;
	auto message = Message::createACK(testId);

	// Data Layer
	auto messageBuffer = message->asBuffer();
	BOOST_CHECK_EQUAL(sizeof(messageData), boost::asio::buffer_size(messageBuffer));

	boost::asio::buffer_copy(boost::asio::buffer(&messageData, sizeof(messageData)), messageBuffer); 
	
	BOOST_CHECK_EQUAL(messageData.type, Message::ACK);
	BOOST_CHECK_EQUAL(messageData.messageId, testId);

	// Parse
	auto parsedMessage = Message::fromBuffer(reinterpret_cast<uint8_t*>(&messageData));
	BOOST_CHECK_EQUAL(parsedMessage->type(), Message::ACK);
	BOOST_CHECK_EQUAL(parsedMessage->messageId(), testId);

}