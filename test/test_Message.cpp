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
	auto message = Message::createPing();

	auto messageBuffer = message->asBuffer();
	std::vector<uint8_t> messageData(boost::asio::buffer_size(messageBuffer));
	boost::asio::buffer_copy(boost::asio::buffer(messageData), messageBuffer); 

	BOOST_CHECK_EQUAL(messageData.size(), 1);
	BOOST_CHECK(messageData[0] == Message::PING);
}