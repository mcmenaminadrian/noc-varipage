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


Bus::Bus(const int deck, Memory& global):level(deck)
{
	globalMemory = global;
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

void Bus::fillNextBuffer(bool& buffer, mutex *gMutex,
	MemoryPacket& packet)
{
	if (levels == 0) {
		//backoff
		int backOff = 0;
		while (true) {
			packet.getProcessor()->waitGlobalTick();
			gMutex->lock();
			if (buffer == false) {
				buffer = true;
				gMutex->unlock();
				return;
			}
			gMutex->unlock();
			for (int i = 0; i < (1 << backOff); i++) {
				packet.getProcessor()->waitGlobalTick();
				packet.getProcessor()->incrementBlocks();
			}
			backOff = (backOff++) % 8;
			packet.getProcessor()->incrementBlocks();
		}
	}
		
	while (true) {
		packet.getProcessor()->waitGlobalTick();
		gMutex->lock();
		if (buffer == false) {
			buffer = true;
			gMutex->unlock();
			return;
		}
		gMutex->unlock();
		packet.getProcessor()->incrementBlocks();
	}
}

void Bus::routeDown(MemoryPacket& packet)
{
	bool *bufferToUnblock = nullptr;
	while (true) {
		packet.getProcessor()->waitGlobalTick();
                acceptedMutex->lock();
                if (acceptedPackets < 4) {
	 	    bottomLeftMutex->lock();
		    bottomRightMutex->lock();
		    if (leftBuffer && rightBuffer) {
			bothBuffers = true;
		    } else {
			bothBuffers = false;
		    }
		    if (!bothBuffers) {
			if (packetOnLeft) {
        	    		bufferToUnblock = &leftBuffer;
				bottomRightMutex->unlock();
				bottomLeftMutex->unlock();
				gateMutex->lock();
                                gate = !gate;
				gateMutex->unlock();
				goto fillDDR;
			} else {
                		bufferToUnblock = &rightBuffer;
				bottomRightMutex->unlock();
				bottomLeftMutex->unlock();
				gateMutex->lock();
                                gate = !gate;
				gateMutex->unlock();
				goto fillDDR;
			}
		    }
		    else {
			gateMutex->lock();
			if (gate) {
				gateMutex->unlock();
				//prioritise right
				if (!packetOnLeft) {
					bufferToUnblock = &rightBuffer;
					bottomRightMutex->unlock();
					bottomLeftMutex->unlock();
					gateMutex->lock();
					gate = false;
					gateMutex->unlock();
					goto fillDDR;
				}
			} else {
				gateMutex->unlock();
				if (packetOnLeft) {
					bufferToUnblock = &leftBuffer;
					bottomRightMutex->unlock();
					bottomLeftMutex->unlock();
					gateMutex->lock();
					gate = true;
					gateMutex->unlock();
					goto fillDDR;
				}
			}
		}
		bottomRightMutex->unlock();
		bottomLeftMutex->unlock();
                }
                acceptedMutex->unlock();
		packet.getProcessor()->incrementBlocks();
	}

fillDDR:
        bottomLeftMutex->lock();
        bottomRightMutex->lock();
        *bufferToUnblock = false;
        bottomRightMutex->unlock();
        bottomLeftMutex->unlock();
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

void Mux::keepRoutingPacket(MemoryPacket& packet)
{
	if (upstreamMux == nullptr) {
		return routeDown(packet);
	} else {
		return postPacketUp(packet);
	}
}

void Mux::postPacketUp(MemoryPacket& packet)
{
	//one method here allows us to vary priorities between left and right
	//first step - what is the buffer we are targetting
	const uint64_t processorIndex = packet.getProcessor()->
		getTile()->getOrder();
	mutex *targetMutex = upstreamMux->bottomRightMutex;
	bool targetOnRight = true;
	if (processorIndex <= upstreamMux->lowerLeft.second) {
		targetOnRight = false;
		targetMutex = upstreamMux->bottomLeftMutex;
	}

	bool bothBuffers = false;
	while (true) {
		packet.getProcessor()->waitGlobalTick();
        	//in this implementation we alternate 
		bottomLeftMutex->lock();
		bottomRightMutex->lock();
        	if (leftBuffer && rightBuffer) {
            		bothBuffers = true;
        	} else {
			bothBuffers = false;
		}
        	if (!bothBuffers) {
            		//which are we, left or right?
			//only one buffer in use...so our packet has to be there
            		if (leftBuffer) {
                		targetMutex->lock();
                		if (targetOnRight && upstreamMux->rightBuffer == false)	
                		{
                    			leftBuffer = false;
                    			upstreamMux->rightBuffer = true;
                    			targetMutex->unlock();
                    			bottomRightMutex->unlock();
                    			bottomLeftMutex->unlock();
                    			gateMutex->lock();
                                        gate = !gate;
                    			gateMutex->unlock();
                    			return upstreamMux->keepRoutingPacket(packet);
                		}
                		else if (!targetOnRight && upstreamMux->leftBuffer == false)
                		{
                    			leftBuffer = false;
                    			upstreamMux->leftBuffer = true;
                    			targetMutex->unlock();
                    			bottomRightMutex->unlock();
                    			bottomLeftMutex->unlock();
                  	  		gateMutex->lock();
                    			gate = !gate;
                    			gateMutex->unlock();
                    			return upstreamMux->keepRoutingPacket(packet);
                		}
                		targetMutex->unlock();
            		} else {
                    		targetMutex->lock();
                   	 	if (targetOnRight && upstreamMux->rightBuffer == false)
                    		{
                        		rightBuffer = false;
                        		upstreamMux->rightBuffer = true;
         	       	        	targetMutex->unlock();
                	        	bottomRightMutex->unlock();
                        		bottomLeftMutex->unlock();
                        		gateMutex->lock();
                                        gate = !gate;
                        		gateMutex->unlock();
                        		return upstreamMux->keepRoutingPacket(packet);
                    		}
                    		else if (!targetOnRight && upstreamMux->leftBuffer == false)
                    		{
                        		rightBuffer = false;
                       			upstreamMux->leftBuffer = true;
                        		targetMutex->unlock();
                        		bottomRightMutex->unlock();
                        		bottomLeftMutex->unlock();
                        		gateMutex->lock();
                                        gate = !gate;
                        		gateMutex->unlock();
                        		return upstreamMux->keepRoutingPacket(packet);
                    		}
                    		targetMutex->unlock();
	    		}
		} else {
			//two packets here so which one are we?
			gateMutex->lock();
                	if (gate == true) {
                		gateMutex->unlock();
                    		//prioritise right
				if (processorIndex > lowerLeft.second) {
                    			targetMutex->lock();
                    			if (targetOnRight &&
                        			upstreamMux->rightBuffer == false)
                    			{
                        			rightBuffer = false;
                        			upstreamMux->rightBuffer = true;
                        			targetMutex->unlock();
        	                		bottomRightMutex->unlock();
                	        		bottomLeftMutex->unlock();
                        			gateMutex->lock();
                        			gate = false;
 		                       		gateMutex->unlock();
                	        		return upstreamMux->keepRoutingPacket(packet);
                	    		}
                    			else if (!targetOnRight &&
                        			upstreamMux->leftBuffer == false)
    	                		{
        	                		rightBuffer = false;
                	        		upstreamMux->leftBuffer = true;
                        			targetMutex->unlock();
                        			bottomRightMutex->unlock();
	                        		bottomLeftMutex->unlock();
        	               			gateMutex->lock();
                	        		gate = false;
                        			gateMutex->unlock();
                        			return upstreamMux->keepRoutingPacket(packet);
                    			}
                    			targetMutex->unlock();
				}
			} else {
        	            	gateMutex->unlock();
                	    	//target left - if we are on the left
				if (processorIndex < lowerRight.first) {
					targetMutex->lock();
		    			if (targetOnRight && upstreamMux->rightBuffer == false)
                    			{
 		                       		leftBuffer = false;
        	                		upstreamMux->rightBuffer = true;
                	        		targetMutex->unlock();
                        			bottomRightMutex->unlock();
                        			bottomLeftMutex->unlock();
 		                       		gateMutex->lock();
                	        		gate = !gate;
                        			gateMutex->unlock();
                        			return upstreamMux->keepRoutingPacket(packet);
                    			}
                    			else if (!targetOnRight &&
                        			upstreamMux->leftBuffer == false)
  	                  		{
        	                		leftBuffer = false;
                	        		upstreamMux->leftBuffer = true;
                        			targetMutex->unlock();
	                        		bottomRightMutex->unlock();
        	                		bottomLeftMutex->unlock();
                	        		gateMutex->lock();
                        			gate = !gate;
                       				gateMutex->unlock();
                        			return upstreamMux->keepRoutingPacket(packet);
                    			}
					targetMutex->unlock();
				}
                	}
		}
		bottomRightMutex->unlock();
		bottomLeftMutex->unlock();
		packet.getProcessor()->incrementBlocks();
	}
}

void Mux::addMMUMutex()
{
    mmuMutex = new mutex();
    acceptedMutex = new mutex();
    mmuLock =  unique_lock<mutex>(*mmuMutex);
    mmuLock.unlock();
}
