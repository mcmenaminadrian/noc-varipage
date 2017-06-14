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
#include "tile.hpp"
#include "processor.hpp"
#include "bus.hpp"

using namespace std;

#define WRITE_FACTOR 2

Bus::Bus()
{
	acceptedPackets = 0;
}

Bus::~Bus()
{
	disarmMutex();
}

void Bus::disarmMutex()
{
	delete gateMutex;
	gateMutex = nullptr;
	if (mmuMutex) {
		delete mmuMutex;
		mmuMutex = nullptr;
		delete acceptedMutex;
		acceptedMutex = nullptr;
	}
}

void Bus::initialiseMutex()
{
	gateMutex = new mutex();
}

bool Bus::isFree()
{
	acceptedMutex->lock();
	if (acceptedPackets < 4) {
		return true;
	} else {
		acceptedMutex->unlock();
		return false;
	}
}

void Bus::clearAcceptedMutex()
{
	acceptedMutex->unlock();
}

void Bus::routeDown(MemoryPacket& packet)
{

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
	//
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

void Bus::addMMUMutex()
{
    mmuMutex = new mutex();
    acceptedMutex = new mutex();
    mmuLock =  unique_lock<mutex>(*mmuMutex);
    mmuLock.unlock();
}
