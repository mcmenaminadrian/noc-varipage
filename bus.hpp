
#ifndef _MUX_CLASS_
#define _MUX_CLASS_


//8 ticks plus MMU time
static const uint64_t MMU_DELAY = 50;
static const uint64_t DDR_DELAY = 8;
static const uint64_t PACKET_LIMIT = 4;

class Memory;

class Bus {
private:
	Memory* globalMemory;
	bool buffer;
	void disarmMutex();
	std::mutex *gateMutex;
	std::mutex *acceptedMutex;
	uint64_t acceptedPackets;
	uint level;

public:
	Bus* upstreamBus;
	Bus* downstreamBus;
	Bus(const int deck, Memory* global):  buffer(false),
		gateMutex(nullptr), acceptedMutex(nullptr),
		acceptedPackets(0),
		upstreamBus(nullptr), level(deck), globalMemory(global)
	{};
	~Bus();
	void initialiseMutex();
	void fillNextBuffer(MemoryPacket& packet);
	void routeDown(MemoryPacket& packet);
	void assignGlobalMemory(Memory *gMem){ globalMemory = gMem; }
	void routePacket(MemoryPacket& pack);
	void postPacketUp(MemoryPacket& packet);
	void keepRoutingPacket(MemoryPacket& packet);

};	
#endif
