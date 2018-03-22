#include <CopyFile.h>
#include <boost/bind.hpp>
#include <Config.h>

#define BOOST_LOG_DYN_LINK 1
#include <boost/log/trivial.hpp>

using boost::asio::ip::udp;

CopyFile::CopyFile(const char* path, 
	std::shared_ptr<udp::socket> socket,
	udp::endpoint& receiverEndpoint) : 
m_socket(socket),
m_receiverEndpoint(receiverEndpoint),
m_errorCount(0),
m_receiveTimer(socket->get_io_service())
{
	m_reqFileMessage = Message::createReqFile(path);  
	sendRequestFile();
}

void CopyFile::sendRequestFile()
{
	BOOST_LOG_TRIVIAL(info) << "CopyFile::sendRequestFile: " << m_reqFileMessage->path();

    m_socket->async_send_to(m_reqFileMessage->asBuffer(), m_receiverEndpoint,
        boost::bind(&CopyFile::sendRequestFileComplete, this, 
          	boost::asio::placeholders::error,
          	boost::asio::placeholders::bytes_transferred));

    m_socket->async_receive_from(boost::asio::buffer(m_receiveBuffer), 
    		m_receiverEndpoint,
        	boost::bind(&CopyFile::receiveFileInfo, this, 
          	boost::asio::placeholders::error,
          	boost::asio::placeholders::bytes_transferred));    	
}

void CopyFile::sendRequestFileComplete (
  const boost::system::error_code& error,
  std::size_t messageSize)
{
	if( error ) {
		m_errorCount++;
		BOOST_LOG_TRIVIAL(error) << "CopyFile::sendRequestFile (" << m_errorCount << "):" << error;

		if(m_errorCount < SEND_RETRY) {
			sendRequestFile();
		}
	} else {
		BOOST_LOG_TRIVIAL(info) << "CopyFile::sendRequestFile: " << messageSize;

		m_receiveTimer.expires_from_now(boost::posix_time::milliseconds(RECEIVE_TIMEOUT));
		m_receiveTimer.async_wait(
	        boost::bind(&CopyFile::receiveFileInfoTimeOut, this, 
	          	boost::asio::placeholders::error));
	}
}

void CopyFile::receiveFileInfo( const boost::system::error_code& error,
		std::size_t messageSize)
{
	BOOST_LOG_TRIVIAL(info) << "CopyFile::receiveFileInfo: " << messageSize;
	
    if (!error || error == boost::asio::error::message_size)
    {
    	// Cancel timeout timer
    	m_receiveTimer.cancel();

      	m_fileInfoMessage = Message::fromBuffer(m_receiveBuffer.data(), messageSize);
  		m_outstandingPackets = Range(0, m_fileInfoMessage->packetCount());

  		sendRequestFilePackets();
  	} 
}


void CopyFile::receiveFileInfoTimeOut( const boost::system::error_code& error )
{
	if( !error ) {
		m_errorCount++;
		BOOST_LOG_TRIVIAL(error) << "CopyFile::receiveFileInfo: (" << m_errorCount << "):" << " Timeout";

		// Cancel the receive operation
		m_socket->cancel();

		// Retry up to SEND_RETRY times
		if(m_errorCount < SEND_RETRY) {
			sendRequestFile();
		}		
	} 
}

void CopyFile::sendRequestFilePackets()
{
	BOOST_LOG_TRIVIAL(info) << "CopyFile::sendRequestFilePackets PacketCount: " << m_outstandingPackets.elementCount();

	auto reqPacketsMessage = Message::createRequestFilePackets(
		m_fileInfoMessage->ufid(),
		m_outstandingPackets.firstN(256));

    m_socket->async_send_to(reqPacketsMessage->asBuffer(), m_receiverEndpoint,
        boost::bind(&CopyFile::sendRequestFilePacketsComplete, this, 
          	boost::asio::placeholders::error,
          	boost::asio::placeholders::bytes_transferred));

    m_socket->async_receive_from(boost::asio::buffer(m_receiveBuffer), 
    		m_receiverEndpoint,
        	boost::bind(&CopyFile::receiveFilePacket, this, 
          	boost::asio::placeholders::error,
          	boost::asio::placeholders::bytes_transferred));    	

}


void CopyFile::sendRequestFilePacketsComplete(  
		const boost::system::error_code& error,
		std::size_t messageSize)
{
	BOOST_LOG_TRIVIAL(info) << "CopyFile::sendRequestFilePacketsComplete";

	if( error ) {
		m_errorCount++;
		BOOST_LOG_TRIVIAL(error) << "CopyFile::sendRequestFile (" << m_errorCount << "):" << error;

		if(m_errorCount < SEND_RETRY) {
			sendRequestFilePackets();
		}
	} else {
		BOOST_LOG_TRIVIAL(info) << "CopyFile::sendRequestFile: " << messageSize;

		m_receiveTimer.expires_from_now(boost::posix_time::milliseconds(RECEIVE_TIMEOUT));
		m_receiveTimer.async_wait(
	        boost::bind(&CopyFile::sendRequestFilePacketsTimeOut, this, 
	          	boost::asio::placeholders::error));
	}


}


void CopyFile::sendRequestFilePacketsTimeOut( const boost::system::error_code& error )
{
	if( !error ) {
		BOOST_LOG_TRIVIAL(error) << "CopyFile::receiveFilePacket: (" << m_errorCount << "):" << " Timeout";

		// Cancel the receive operation
		m_socket->cancel();

		// Retry up to SEND_RETRY times
		if(m_outstandingPackets.elementCount() > 0 && m_errorCount < SEND_RETRY) {
			sendRequestFilePackets();
		}		
	} 
}

void CopyFile::receiveFilePacket( const boost::system::error_code& error,
		std::size_t messageSize)
{
    if (!error || error == boost::asio::error::message_size)
    {
    	m_receiveTimer.cancel();

      	auto filePacketMessage = Message::fromBuffer(m_receiveBuffer.data(), messageSize);
  		m_outstandingPackets.subtract(filePacketMessage->packetId());

		BOOST_LOG_TRIVIAL(info) << "CopyFile::receiveFilePacket: " << m_outstandingPackets.elementCount() ;

    	// Restart timeout timer
    	if(m_outstandingPackets.elementCount() > 0) {
		    m_socket->async_receive_from(boost::asio::buffer(m_receiveBuffer), 
		    		m_receiverEndpoint,
		        	boost::bind(&CopyFile::receiveFilePacket, this, 
		          	boost::asio::placeholders::error,
		          	boost::asio::placeholders::bytes_transferred));    		

	    	m_receiveTimer.expires_from_now(boost::posix_time::milliseconds(RECEIVE_TIMEOUT));
			m_receiveTimer.async_wait(
		        boost::bind(&CopyFile::sendRequestFilePacketsTimeOut, this, 
		          	boost::asio::placeholders::error));	    	

    	}
  	} 
}
