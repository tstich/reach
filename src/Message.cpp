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

std::shared_ptr<Message> Message::createAlive()
{
	std::shared_ptr<Message> message(new Message(ALIVE));
	return message;
}

std::shared_ptr<Message> Message::createReqFile(uint64_t ufid, const char* path)
{
	std::shared_ptr<Message> message(new Message(REQ_FILE));
	message->m_ufid = ufid;
	strncpy(message->m_path, path, PATH_LENGTH);
	return message;
}

std::shared_ptr<Message> Message::createFileInfo(uint64_t ufid, uint64_t fileSize, uint64_t packetSize)
{
	std::shared_ptr<Message> message(new Message(FILE_INFO));
	message->m_ufid = ufid;
	message->m_fileSize = fileSize;
	message->m_packetSize = packetSize;
	return message;
}

std::shared_ptr<Message> Message::createRequestFilePackets(uint64_t ufid, Range packets)
{
	std::shared_ptr<Message> message(new Message(REQ_FILE_PACKETS));
	message->m_ufid = ufid;
	message->m_packets = packets;
	return message;
}

std::shared_ptr<Message> Message::createFilePacket(uint64_t ufid, uint64_t packetId, const uint8_t* payloadData, size_t payloadSize)
{
	std::shared_ptr<Message> message(new Message(FILE_PACKET));
	message->m_ufid = ufid;
	message->m_packetId = packetId;
	message->m_payloadData = payloadData;
	message->m_payloadSize = payloadSize;
	return message;
}

std::shared_ptr<Message> Message::createFilePacket(uint64_t ufid, uint64_t packetId, const char* payloadData, size_t payloadSize)
{
	return createFilePacket(ufid, packetId, reinterpret_cast<const uint8_t*>(payloadData), payloadSize);
}

std::shared_ptr<Message> Message::fromBuffer(const uint8_t* data, size_t length)
{
	const uint8_t* bufferEnd = data + length;

	std::shared_ptr<Message> message(new Message(*reinterpret_cast<const TYPE*>(data)));
	data += sizeof(TYPE);

	if( message->m_type == ALIVE ) {
		message->m_version = *reinterpret_cast<const uint64_t*>(data);
		data += sizeof(uint64_t);
	}

	if( message->m_type == REQ_FILE || message->m_type == FILE_INFO || message->m_type == REQ_FILE_PACKETS || message->m_type == FILE_PACKET ) {
		message->m_ufid = *reinterpret_cast<const uint64_t*>(data);
		data += sizeof(uint64_t);
	}

	if( message->m_type == REQ_FILE ) {
		strncpy(message->m_path, reinterpret_cast<const char*>(data), PATH_LENGTH);
		data += PATH_LENGTH;
	}

	if( message->m_type == FILE_INFO ) {
		message->m_fileSize = *reinterpret_cast<const uint64_t*>(data);
		data += sizeof(uint64_t);
		message->m_packetSize = *reinterpret_cast<const uint64_t*>(data);
		data += sizeof(uint64_t);
	}

	if( message->m_type == REQ_FILE_PACKETS ) {
		message->m_packets = *Range::fromBuffer(data);
	}

	if( message->m_type == FILE_PACKET ) {
		message->m_packetId = *reinterpret_cast<const uint64_t*>(data);
		data += sizeof(uint64_t);
		message->m_payloadData = data;
		message->m_payloadSize = bufferEnd - data;
	}

	return message;
}

// Create a composite buffer for the Message
std::vector<boost::asio::const_buffer> Message::asBuffer() const {

	std::vector<boost::asio::const_buffer> composite_buffer;


	composite_buffer.push_back(boost::asio::const_buffer(&m_type, sizeof(m_type)));

	if( m_type == ALIVE ) {
		composite_buffer.push_back(boost::asio::const_buffer(&m_version, sizeof(m_version)));
	}

	if( m_type == REQ_FILE || m_type == FILE_INFO || m_type == REQ_FILE_PACKETS || m_type == FILE_PACKET ) {
		composite_buffer.push_back(boost::asio::const_buffer(&m_ufid, sizeof(m_ufid)));
	}

	if( m_type == REQ_FILE ) {
		composite_buffer.push_back(boost::asio::const_buffer(&m_path, sizeof(m_path)));
	}

	if( m_type == FILE_INFO ) {
		composite_buffer.push_back(boost::asio::const_buffer(&m_fileSize, sizeof(m_fileSize)));
		composite_buffer.push_back(boost::asio::const_buffer(&m_packetSize, sizeof(m_packetSize)));
	}

	if( m_type == REQ_FILE_PACKETS ) {
		auto rangeBuffer = m_packets.asBuffer();
		composite_buffer.insert(composite_buffer.end(), rangeBuffer.begin(), rangeBuffer.end());
	}

	if( m_type == FILE_PACKET ) {
		composite_buffer.push_back(boost::asio::const_buffer(&m_packetId, sizeof(m_packetId)));
		composite_buffer.push_back(boost::asio::buffer(m_payloadData, m_payloadSize));
	}


	return composite_buffer;
}