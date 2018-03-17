//Link to Boost
#define BOOST_TEST_DYN_LINK

//Define our Module name (prints at testing)
#define BOOST_TEST_MODULE "Message"

//VERY IMPORTANT - include this last
#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>

#include <Message.h>
#include <Config.h>

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////

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

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////

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

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////

BOOST_AUTO_TEST_CASE( aliveMessage )
{
	// UDP Packet as Struct
	#pragma pack(push, 1)
	struct {
		uint8_t type;
		uint64_t version;
	} messageData;
	#pragma pack(pop)

	// Create
	auto message = Message::createAlive();

	// Data Layer
	auto messageBuffer = message->asBuffer();
	BOOST_CHECK_EQUAL(sizeof(messageData), boost::asio::buffer_size(messageBuffer));

	boost::asio::buffer_copy(boost::asio::buffer(&messageData, sizeof(messageData)), messageBuffer); 
	
	BOOST_CHECK_EQUAL(messageData.type, Message::ALIVE);
	BOOST_CHECK_EQUAL(messageData.version, REACH_VERSION);

	// Parse
	auto parsedMessage = Message::fromBuffer(reinterpret_cast<uint8_t*>(&messageData));
	BOOST_CHECK_EQUAL(parsedMessage->type(), Message::ALIVE);
	BOOST_CHECK_EQUAL(messageData.version, REACH_VERSION);
}

/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////

BOOST_AUTO_TEST_CASE( reqFileMessage )
{
	// UDP Packet as Struct
	#pragma pack(push, 1)
	struct {
		uint8_t type;
		uint64_t messageId;
		char path[PATH_LENGTH];
	} messageData;
	#pragma pack(pop)

	// Create
	const char* testPath = "/some/random/file";
	auto message = Message::createReqFile(testPath);

	// Data Layer
	auto messageBuffer = message->asBuffer();
	BOOST_CHECK_EQUAL(sizeof(messageData), boost::asio::buffer_size(messageBuffer));

	boost::asio::buffer_copy(boost::asio::buffer(&messageData, sizeof(messageData)), messageBuffer); 
	
	BOOST_CHECK_EQUAL(messageData.type, Message::REQ_FILE);
	BOOST_CHECK_EQUAL(messageData.messageId, message->messageId());
	BOOST_CHECK_EQUAL(messageData.path, testPath);

	// Parse
	auto parsedMessage = Message::fromBuffer(reinterpret_cast<uint8_t*>(&messageData));
	BOOST_CHECK_EQUAL(parsedMessage->type(), Message::REQ_FILE);
	BOOST_CHECK_EQUAL(parsedMessage->messageId(), message->messageId());
	BOOST_CHECK_EQUAL(parsedMessage->path(), testPath);


	// Check Message ID is not the same for a second message
	auto secondMessage = Message::createReqFile(testPath);
	BOOST_CHECK(message->messageId() != secondMessage->messageId());
}


/////////////////////////////////////
/////////////////////////////////////
/////////////////////////////////////

BOOST_AUTO_TEST_CASE( fileInfoMessage )
{
	// UDP Packet as Struct
	#pragma pack(push, 1)
	struct {
		uint8_t type;
		uint64_t messageId;
		uint64_t ufid;
		uint64_t packetCount;
	} messageData;
	#pragma pack(pop)

	// Create
	uint64_t testUfid = 1234567;
	uint64_t testPacketCount = 75234;
	auto message = Message::createFileInfo(testUfid, testPacketCount);

	// Data Layer
	auto messageBuffer = message->asBuffer();
	BOOST_CHECK_EQUAL(sizeof(messageData), boost::asio::buffer_size(messageBuffer));

	boost::asio::buffer_copy(boost::asio::buffer(&messageData, sizeof(messageData)), messageBuffer); 
	
	BOOST_CHECK_EQUAL(messageData.type, Message::FILE_INFO);
	BOOST_CHECK_EQUAL(messageData.messageId, message->messageId());
	BOOST_CHECK_EQUAL(messageData.ufid, testUfid);

	// Parse
	auto parsedMessage = Message::fromBuffer(reinterpret_cast<uint8_t*>(&messageData));
	BOOST_CHECK_EQUAL(parsedMessage->type(), Message::FILE_INFO);
	BOOST_CHECK_EQUAL(parsedMessage->messageId(), message->messageId());
	BOOST_CHECK_EQUAL(parsedMessage->ufid(), testUfid);


	// Check Message ID is not the same for a second message
	auto secondMessage = Message::createFileInfo(testUfid, testPacketCount);
	BOOST_CHECK(message->messageId() != secondMessage->messageId());
}
