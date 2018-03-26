#pragma once

#include <memory>
#include <random>
#include <boost/asio.hpp>
#include <Config.h>
#include <Range.h>

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
	enum TYPE : uint8_t {PING, ALIVE, REQ_FILE, FILE_INFO, REQ_FILE_PACKETS, FILE_PACKET};


//////////////////////////////
// Methods
//////////////////////////////
public:
	// Message Specific Create
	static std::shared_ptr<Message> createPing();
	static std::shared_ptr<Message> createAlive();	
	static std::shared_ptr<Message> createReqFile(uint64_t ufid, const char* path);
	static std::shared_ptr<Message> createFileInfo(uint64_t ufid, uint64_t packetCount, uint64_t packetSize);
	static std::shared_ptr<Message> createRequestFilePackets(uint64_t ufid, Range packets);
	static std::shared_ptr<Message> createFilePacket(uint64_t ufid, uint64_t packetId, const uint8_t* payloadData, size_t payloadSize);
	static std::shared_ptr<Message> createFilePacket(uint64_t ufid, uint64_t packetId, const char* payloadData, size_t payloadSize);

	// Message from Buffer
	static std::shared_ptr<Message> fromBuffer(const uint8_t* data, size_t length);

	// Message to Buffer
	std::vector<boost::asio::const_buffer> asBuffer() const;

	// Meta
	TYPE type() const { return m_type; }
	const char* path() const { return m_path; }
	uint64_t ufid() const { return m_ufid; }
	uint64_t packetCount() const { return m_packetCount; }
	uint64_t packetSize() const { return m_packetSize; }
	Range packets() const { return m_packets; }

	uint64_t packetId() const { return m_packetId; }
	const uint8_t* payloadData() const { return m_payloadData; }
	size_t payloadSize() const { return m_payloadSize; }

protected:
	Message(TYPE type);

//////////////////////////////
// Variables
//////////////////////////////
private:	
	TYPE m_type;
	uint64_t m_version;
	char m_path[PATH_LENGTH];
	uint64_t m_ufid;
	uint64_t m_packetCount;
	uint64_t m_packetSize;
	Range m_packets;
	uint64_t m_packetId;
	const uint8_t* m_payloadData;
	size_t m_payloadSize;


	static uint64_t m_nextMessageId;
};