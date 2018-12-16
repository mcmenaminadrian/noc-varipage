#include <iostream>
#include <map>
#include <vector>
#include <utility>
#include <tuple>
#include <bitset>
#include <mutex>
#include <condition_variable>
#include "mainwindow.h"
#include "memorypacket.hpp"
#include "memory.hpp"
#include "ControlThread.hpp"
#include "bus.hpp"
#include "tile.hpp"
#include "processor.hpp"

using namespace std;

#define WRITE_FACTOR 2

//constructor is in the header

Bus::~Bus()
{
	disarmMutex();
}

void Bus::disarmMutex()
{
	delete gateMutex;
	gateMutex = nullptr;
	if (acceptedMutex) {
		delete acceptedMutex;
		acceptedMutex = nullptr;
    	}
}

void Bus::initialiseMutex()
{
	gateMutex = new mutex();
	acceptedMutex = new mutex();
}

void Bus::routePacket(MemoryPacket& memoryRequest)
{
	fillNextBuffer(memoryRequest);
	return postPacketUp(memoryRequest);
}

void Bus::fillNextBuffer(MemoryPacket& packet)
{
	//backoff
	int backOff = 0;
	packet.getProcessor()->waitGlobalTick();
	while (true) {
		gateMutex->lock();
		if (buffer == false) {
			buffer = true;
			gateMutex->unlock();
			return;
		}
		gateMutex->unlock();
		for (int i = 0; i < (1 << backOff); i++) {
			packet.getProcessor()->waitGlobalTick();
			packet.getProcessor()->incrementBlocks();
		}
		backOff = (backOff++) % 8;
		packet.getProcessor()->waitGlobalTick();
		packet.getProcessor()->incrementBlocks();
	}
		
}

void Bus::routeDown(MemoryPacket& packet)
{

	while (true) {
		packet.getProcessor()->waitGlobalTick();
		acceptedMutex->lock();
		if (acceptedPackets < 4) {
			gateMutex->lock();
			buffer = false;
			gateMutex->unlock();
			break;
		}
		acceptedMutex->unlock();
		packet.getProcessor()->incrementBlocks();
	}

	acceptedPackets++;
	acceptedMutex->unlock();

	uint64_t serviceDelay = MMU_DELAY;
	if (packet.getWrite()) {
		serviceDelay *= WRITE_FACTOR;
	}
	for (unsigned int i = 0; i < serviceDelay; i++) {
		packet.getProcessor()->incrementServiceTime();
		packet.getProcessor()->waitGlobalTick();
	}
	acceptedMutex->lock();
	acceptedPackets--;
	acceptedMutex->unlock();
	//cross to tree
	for (unsigned int i = 0; i < DDR_DELAY; i++) {
		packet.getProcessor()->waitGlobalTick();
	}
	//get memory
	if (packet.getRequestSize() > 0) {
		for (unsigned int i = 0; i < packet.getRequestSize(); i++) {
		packet.fillBuffer(packet.getProcessor()->
			getTile()->readByte(packet.getRemoteAddress() + i));
		}
	}
	return;
}	

void Bus::keepRoutingPacket(MemoryPacket& packet)
{
	return routeDown(packet);	
}

void Bus::postPacketUp(MemoryPacket& packet)
{
	keepRoutingPacket(packet);
}
