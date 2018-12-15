
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
	std::mutex *mmuMutex;
        std::unique_lock<std::mutex> mmuLock;
	void disarmMutex();
	std::mutex *gateMutex;
	std::mutex *acceptedMutex;
	bool gate;
	uint64_t acceptedPackets;

public:
	Bus* upstreamBus;
	Bus* downstreamBus;
	Bus(const int deck, Memory& global):  buffer(false), level(deck), 
            mmuMutex(nullptr), gateMutex(nullptr), acceptedMutex(nullptr),
	    acceptedPackets(0), gate(false), globalMemory(global),
            upstreamBus(nullptr){};
	~Bus();
	void initialiseMutex();
	void fillBottomBuffer(bool& buffer,
		std::mutex *botMutex, MemoryPacket& packet);
	void routeDown(MemoryPacket& packet);
	void assignGlobalMemory(Memory *gMem){ globalMemory = gMem; }
	void joinUpMux(const Mux& left, const Mux& right);
	void assignNumbers(const uint64_t& ll, const uint64_t& ul,
		const uint64_t& lr, const uint64_t& ur);
	const std::tuple<const uint64_t, const uint64_t,
		const uint64_t, const uint64_t> fetchNumbers() const;
	void routePacket(MemoryPacket& pack);
    	bool acceptPacketUp(const MemoryPacket& mPack) const;
	void postPacketUp(MemoryPacket& packet);
	void keepRoutingPacket(MemoryPacket& packet);
    void addMMUMutex();

};	
#endif
