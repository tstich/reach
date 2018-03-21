#include <CopyFile.h>
#include <boost/bind.hpp>
#include <Config.h>

#define BOOST_LOG_DYN_LINK 1
#include <boost/log/trivial.hpp>

using boost::asio::ip::udp;

CopyFile::CopyFile(const char* path, 
	std::shared_ptr<udp::socket> socket,
	udp::endpoint& receiverEndpoint) : 
m_path(path),
m_socket(socket),
m_receiverEndpoint(receiverEndpoint),
m_errorCount(0),
m_receiveTimer(socket->get_io_service())
{
	sendRequestFile();
}

void CopyFile::sendRequestFile()
{
	BOOST_LOG_TRIVIAL(info) << "CopyFile::sendRequestFile: " << m_path;
    std::shared_ptr<Message> message = Message::createReqFile(m_path); 

    m_socket->async_send_to(message->asBuffer(), m_receiverEndpoint,
        boost::bind(&CopyFile::sendRequestFileCallback, this, 
        	message,
          	boost::asio::placeholders::error,
          	boost::asio::placeholders::bytes_transferred));

}

void CopyFile::sendRequestFileCallback(
  std::shared_ptr<Message> message,
  const boost::system::error_code& error,
  std::size_t bytes_transferred)
{
	if( error ) {
		m_errorCount++;
		BOOST_LOG_TRIVIAL(error) << "CopyFile::sendRequestFile (" << m_errorCount << "):" << error;

		if(m_errorCount < SEND_RETRY) {
			sendRequestFile();
		}
	} else {
		BOOST_LOG_TRIVIAL(info) << "CopyFile::sendRequestFile: Success";

		m_receiveTimer.expires_from_now(boost::posix_time::milliseconds(RECEIVE_TIMEOUT));
		m_receiveTimer.async_wait(
	        boost::bind(&CopyFile::receiveFileInfoTimeOut, this, 
	          	boost::asio::placeholders::error));
	}
}

void CopyFile::receiveFileInfoTimeOut( const boost::system::error_code& error )
{
	if( !error ) {
		m_errorCount++;
		BOOST_LOG_TRIVIAL(error) << "CopyFile::receiveFileInfo: (" << m_errorCount << "):" << " Timeout";

		if(m_errorCount < SEND_RETRY) {
			sendRequestFile();
		}		
	} 
}