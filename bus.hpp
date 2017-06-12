
#ifndef _BUS_CLASS_
#define _BUS_CLASS_


//8 ticks plus MMU time
static const uint64_t MMU_DELAY = 50;
static const uint64_t DDR_DELAY = 8;
static const uint64_t PACKET_LIMIT = 4;

class Memory;

class Bus {
private:
	Memory* globalMemory;
	std::mutex *mmuMutex;
        std::unique_lock<std::mutex> mmuLock;
	void disarmMutex();
	std::mutex *gateMutex;
	std::mutex *acceptedMutex;
	bool gate;
	uint64_t acceptedPackets;

public:
	Bus(Memory *gMem): globalMemory(gMem) {};
	~Mux();
	void initialiseMutex();
	void routeDown(MemoryPacket& packet);
	void assignGlobalMemory(Memory *gMem){ globalMemory = gMem; }
	void addMMUMutex();
	const bool isFree();
	void clearAcceptedMutex();
};	
#endif
