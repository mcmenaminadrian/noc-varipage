#include <QObject>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <bitset>
#include <mutex>
#include <tuple>
#include <condition_variable>
#include <climits>
#include <cstdlib>
#include "mainwindow.h"
#include "ControlThread.hpp"
#include "memorypacket.hpp"
#include "mux.hpp"
#include "tile.hpp"
#include "memory.hpp"
#include "processor.hpp"

//page table flags
//bit 0 - 0 for invalid entry, 1 for valid
//bit 1 - 0 for moveable, 1 for fixed
//bit 2 - 0 for CLOCKed out, 1 for CLOCKed in
//bit 3 - 0 for read/write, 1 for read only

//TLB model
//first entry - virtual address 
//second entry - physical address
//third entry - bool for validity

//statusWord
//Bit 0 :   true = REAL, false = VIRTUAL
//Bit 1 :   CarryBit

const static uint64_t KERNELPAGES = 2;	//2 gives 1k kernel on 512b paging
const static uint64_t STACKPAGES = 2; 	//2 gives 1k stack on 512b paging
const static uint64_t BITMAPDELAY = 0;	//0 for subcycle bitmap checks
const static uint64_t FREEPAGES = 25;	//25 for 512b pages, 12 for 1k pages
const static uint64_t BASEPAGES = 5;	//5 for 512b pages, 3 for 1k pages 
const static uint64_t COREOFFSET = 1024;
const static uint64_t CACHES_AVAILABLE = 256;
const static uint64_t lineSize = 16;

using namespace std;

Processor::Processor(Tile *parent, MainWindow *mW, uint64_t numb):
    masterTile(parent), mode(REAL), mainWindow(mW)
{
	registerFile = vector<uint64_t>(REGISTER_FILE_SIZE, 0);
	statusWord[0] = true;
	totalTicks = 1;
	uninterruptedTicks = 1;
	hardFaultCount = 0;
    	blocks = 0;
	inInterrupt = false;
    	processorNumber = numb;
    	clockDue = false;
	lineMask = 0x0F; //16 bytes per line
    	QObject::connect(this, SIGNAL(hardFault()),
        	mW, SLOT(updateHardFaults()));
}

void Processor::resetCounters()
{
	hardFaultCount = 0;
	blocks = 0;
	serviceTime = 0;
}

void Processor::setMode()
{
	if (!statusWord[0]) {
		mode = REAL;
		statusWord[0] = true;
	} else {
		mode = VIRTUAL;
		statusWord[0] = false;
	}
}

void Processor::switchModeReal()
{
	if (!statusWord[0]) {
		mode = REAL;
		statusWord[0] = true;
	}
}

void Processor::switchModeVirtual()
{
	if (statusWord[0]) {
		mode = VIRTUAL;
		statusWord[0] = false;
	}
}

void Processor::writeOutBasicPageTableEntries()
{
	const uint64_t tablesOffset = COREOFFSET 
		+ PAGESLOCAL;
	for (unsigned int i = 0; i < CACHES_AVAILABLE; i++) {
        	uint64_t memoryLocalOffset = i * PAGETABLEENTRY + tablesOffset;
        	masterTile->writeLong(
           		memoryLocalOffset + VOFFSET,
			10 * COREOFFSET + i * 16 + PAGESLOCAL);
        	masterTile->writeLong(
            		memoryLocalOffset + POFFSET,
			10 * COREOFFSET + i * 16 + PAGESLOCAL);
		masterTile->writeLong(
            		memoryLocalOffset + FRAMEOFFSET, i);
		masterTile->writeWord32(memoryLocalOffset + FLAGOFFSET, 0);
		masterTile->writeLong(memoryLocalOffset + CLOCKOFFSET, 0); 
	}
}

void Processor::flushPagesStart()
{
    interruptBegin();
}

void Processor::flushPagesEnd()
{
    interruptEnd();
}


//hard wire in: 16 byte line size
//1024 bytes used for 'kernel'
//1024 bytes used for 'stack'
//9216 bytes used for mapping
//4096 bytes used for store

void Processor::createMemoryMap(Memory *local)
{
	localMemory = local;
	memoryAvailable = localMemory->getSize();

	stackPointer = TILE_MEM_SIZE + PAGESLOCAL;
	stackPointerUnder = stackPointer;
	stackPointerOver = stackPointer - 1024;
	writeOutBasicPageTableEntries();
}

uint64_t Processor::generateAddress(const uint64_t& frame,
	const uint64_t& address) const
{
	uint64_t offset = address & lineMask;
	return frame * lineSize + offset + PAGESLOCAL;
}

void Processor::interruptBegin()
{
	interruptLock.lock();
	inInterrupt = true;
	switchModeReal();
	for (auto i: registerFile) {
		waitATick();
		pushStackPointer();	
		waitATick();
		masterTile->writeLong(stackPointer, i);
	}
}

void Processor::interruptEnd()
{
	for (int i = registerFile.size() - 1; i >= 0; i--) {
		waitATick();
		registerFile[i] = masterTile->readLong(stackPointer);
		waitATick();
		popStackPointer();
	}
	switchModeVirtual();
	inInterrupt = false;
	interruptLock.unlock();
}

//tuple - vector of bytes, size of vector, success

const vector<uint8_t> Processor::requestRemoteMemory(
	const uint64_t& size, const uint64_t& remoteAddress,
	const uint64_t& localAddress, const bool& write)
{
	//assemble request
	MemoryPacket memoryRequest(this, remoteAddress,
		localAddress, size);
	if (write) {
		memoryRequest.setWrite();
	}
	//wait for response
	if (masterTile->treeLeaf->acceptPacketUp(memoryRequest)) {
		masterTile->treeLeaf->routePacket(memoryRequest);
	} else {
		cerr << "FAILED" << endl;
		exit(1);
	}
	return memoryRequest.getMemory();
}

void Processor::transferGlobalToLocal(const uint64_t& remoteAddress,
	const uint64_t& localAddress, const uint64_t& size, const bool& write)
{
	//mimic a DMA call - so need to advance PC
	uint64_t maskedAddress = remoteAddress & BITMAP_MASK;
	int offset = 0;
	vector<uint8_t> answer = requestRemoteMemory(size,
		maskedAddress, localAddress +
		(maskedAddress & lineMask), write);
	for (auto x: answer) {
		masterTile->writeByte(localAddress + offset + 
			(maskedAddress & lineMask), x);
		offset++;
	}
}

void Processor::transferLocalToGlobal(const uint64_t& address,
	const uint64_t& frameNo)
{
	//again - this is like a DMA call, there is a delay, but no need
	//to advance the PC
	//make the call - ignore the results
	const uint64_t localAddress = masterTile->readLong(COREOFFSET +
		frameNo * PAGETABLEENTRY + VOFFSET);
	requestRemoteMemory(16, address, localAddress, true);
}

//nominate a frame to be used
pair<uint64_t, bool> Processor::getFreeFrame()
{
	//emergency - mark 25 frames for drop
	uint64_t killerTime = uninterruptedTicks;
	uint64_t nominatedFrame = 0;
	for (uint64_t i = 0; i < 25; i++) {
		uint32_t flags = masterTile->readWord32(
			COREOFFSET
			+ i * PAGETABLEENTRY + FLAGOFFSET + PAGESLOCAL);
		flags = flags^0x04;
		waitATick();
		uint64_t timeToKill = masterTile->readLong(COREOFFSET + i *
			PAGETABLEENTRY * CLOCKOFFSET + PAGESLOCAL);
		waitATick();
		if (timeToKill < killerTime) {
			waitATick();
			killerTime = timeToKill;
			waitATick();
			nominatedFrame = i;
		}
		waitATick();
		masterTile->writeWord32(COREOFFSET + i * PAGETABLEENTRY +
			FLAGOFFSET + PAGESLOCAL, flags);
	}
	return pair<const uint64_t, bool>(nominatedFrame, true);
}

pair<uint64_t, bool> Processor::getFrameStatus(int nominatedFrame)
{
	uint32_t flags = masterTile->readWord32(COREOFFSET + nominatedFrame *
		PAGETABLEENTRY + FLAGOFFSET + PAGESLOCAL);
	if (!(flags & 0x01)) {
		return pair<const uint64_t, bool>(nominatedFrame, false);
	} else {
		return pair<const uint64_t, bool>(nominatedFrame, true);
	}
}

//only used to dump a frame
void Processor::writeBackMemory(const uint64_t& frameNo)
{
	//is this read-only?
	if (localMemory->readWord32(COREOFFSET + frameNo * PAGETABLEENTRY +
		FLAGOFFSET) & 0x08) {
    		return;
	}
	const uint64_t physicalAddress = mapToGlobalAddress(
		localMemory->readLong(COREOFFSET +
		frameNo * PAGETABLEENTRY)).first;
	transferLocalToGlobal(physicalAddress, frameNo);
	for (int i = 0; i < 16; i+= sizeof(uint64_t)) {	
		uint64_t toGo = masterTile->readLong(fetchAddressRead(
			localMemory->readLong(frameNo * PAGETABLEENTRY + 
				COREOFFSET + VOFFSET)) + i);
		masterTile->writeLong(fetchAddressWrite(physicalAddress + i), 
			toGo);
	}
}

void Processor::fixPageMap(const uint64_t& frameNo,
    const uint64_t& address, const bool& readOnly)
{
	const uint64_t pageAddress = address & lineMask;
	const uint64_t writeBase = COREOFFSET
		 + frameNo * PAGETABLEENTRY;
	waitATick();
	localMemory->writeLong(writeBase + VOFFSET, pageAddress);
	waitATick();
	if (readOnly) {
		localMemory->writeWord32(writeBase + FLAGOFFSET, 0x0D);
	} else {
		localMemory->writeWord32(writeBase + FLAGOFFSET, 0x05);
	}
	waitATick();
	localMemory->writeLong(writeBase + CLOCKOFFSET, uninterruptedTicks);
}

//write in initial page of code
void Processor::fixPageMapStart(const uint64_t& frameNo,
	const uint64_t& address) 
{
	const uint64_t pageAddress = address & pageMask;
	localMemory->writeLong((1 << pageShift) * KERNELPAGES +
		frameNo * PAGETABLEENTRY + VOFFSET, pageAddress);
	localMemory->writeWord32((1 << pageShift) * KERNELPAGES  +
		frameNo * PAGETABLEENTRY + FLAGOFFSET, 0x0D);
}


// bit lengths
static inline uint64_t bit_mask(uint64_t x)
{
	uint64_t mask = 0;
	const uint64_t digit = 1;
	for (uint64_t i = 0; i < x; i++) {
		mask |= (digit << i);
	}
	return mask;
}	

const static uint64_t SUPER_DIR_BL = 11;
const static uint64_t SUPER_DIR_SHIFT = 37;
const static uint64_t DIR_BL = 9;
const static uint64_t DIR_SHIFT = 28;
const static uint64_t SUPER_TAB_BL = 9;
const static uint64_t SUPER_TAB_SHIFT = 19;
const static uint64_t TAB_BL = 19; //10 + 9 for 512 byte pages
const static uint64_t TAB_SHIFT = 9; //9 for 512, 10 for 1024 pages
const static uint64_t ADDRESS_SPACE_LEN = 48;
//below is always called from the interrupt context 
const pair<uint64_t, uint8_t>
    Processor::mapToGlobalAddress(const uint64_t& address)
{
	uint64_t globalPagesBase = 0x800;
	//48 bit addresses
	uint64_t address48 = address & bit_mask(ADDRESS_SPACE_LEN);
	uint64_t superDirectoryIndex =
		(address48 >> SUPER_DIR_SHIFT) & bit_mask(SUPER_DIR_BL);
	uint64_t directoryIndex = (address48 >> DIR_SHIFT) & bit_mask(DIR_BL);
	uint64_t superTableIndex =
		(address48 >> SUPER_TAB_SHIFT) & bit_mask(SUPER_TAB_BL);
	uint64_t tableIndex = (address48 & bit_mask(TAB_BL)) >> TAB_SHIFT; 
	waitATick();
	//read off the superDirectory number
	//simulate read of global table
	fetchAddressToRegister();
	uint64_t ptrToDirectory = masterTile->readLong(globalPagesBase +
        	superDirectoryIndex * (sizeof(uint64_t) + sizeof(uint8_t)));
	if (ptrToDirectory == 0) {
		cerr << "Bad SuperDirectory: " << hex << address << endl;
		throw new bad_exception();
	}
	waitATick();
	fetchAddressToRegister();
	uint64_t ptrToSuperTable = masterTile->readLong(ptrToDirectory +
		directoryIndex * (sizeof(uint64_t) + sizeof(uint8_t)));
	if (ptrToSuperTable == 0) {
		cerr << "Bad Directory: " << hex << address << endl;
		throw new bad_exception();
	}
	waitATick();
	fetchAddressToRegister();
	uint64_t ptrToTable = masterTile->readLong(ptrToSuperTable +
		superTableIndex * (sizeof(uint64_t) + sizeof(uint8_t)));
	if (ptrToTable == 0) {
		cerr << "Bad SuperTable: " << hex << address << endl;
		throw new bad_exception();
	}
	waitATick();
	fetchAddressToRegister();
	pair<uint64_t, uint8_t> globalPageTableEntry(
		masterTile->readLong(ptrToTable + tableIndex *
		(sizeof(uint64_t) + sizeof(uint8_t))),
		masterTile->readByte(ptrToTable + tableIndex *
		(sizeof(uint64_t) + sizeof(uint8_t)) + sizeof(uint64_t)));
	waitATick();
	return globalPageTableEntry;
}

uint64_t Processor::triggerHardFault(const uint64_t& address,
    const bool& readOnly, const bool& write, const int& nomineeFrame)
{
	emit hardFault();
	hardFaultCount++;
	interruptBegin();
	pair<uint64_t, bool> frameData;
	if (nomineeFrame < 0) {
		//emergency
		frameData = getFreeFrame();
	} else {
		frameData = getFrameStatus(nomineeFrame);
	}
	if (frameData.second) {
		writeBackMemory(frameData.first);
	}
	pair<uint64_t, uint8_t> translatedAddress = mapToGlobalAddress(address);
	transferGlobalToLocal(translatedAddress.first + (address & lineMask),
		masterTile->readLong(frameData.first * PAGETABLEENTRY + 
		COREOFFSET + PAGESLOCAL + POFFSET + (address & lineMask)), 
		BITMAP_BYTES, write);
	fixPageMap(frameData.first, translatedAddress.first, readOnly);
	interruptEnd();
	return generateAddress(frameData.first, translatedAddress.first +
		(address & lineMask));
}

void Processor::incrementBlocks()
{
	ControlThread *pBarrier = masterTile->getBarrier();
	pBarrier->incrementBlocks();
        blocks++;
}

void Processor::incrementServiceTime()
{
        serviceTime++;
}

//when this returns, address guarenteed to be present at returned local address
uint64_t Processor::fetchAddressRead(const uint64_t& address,
	const bool& readOnly, const bool& write)
{
	//implement paging logic
	int nomineeFrame = -1;
	if (mode == VIRTUAL) {
		uint64_t lineSought = address & lineMask;
		waitATick(); 
		for (unsigned int i = 0; i < CACHES_AVAILABLE; i++) {
			waitATick();
            		uint64_t addressInPageTable = PAGESLOCAL +
                        	(i * PAGETABLEENTRY) + 
				COREOFFSET;
            		uint64_t flags =
				masterTile->readWord32(addressInPageTable
                        	+ FLAGOFFSET);
            		if (!(flags & 0x01)) {
				nomineeFrame = i;
                		continue;
            		}
            		waitATick();
            		uint64_t storedLine = masterTile->readLong(
                        	addressInPageTable + VOFFSET);
            		waitATick();
            		if (lineSought == storedLine) {
                		waitATick();
                		flags |= 0x04;
                		masterTile->writeWord32(
					addressInPageTable + FLAGOFFSET,
                    			flags);
				masterTile->writeLong(addressInPageTable +
					CLOCKOFFSET, uninterruptedTicks); 
                		waitATick();
                		return generateAddress(i, address);
            		}
			waitATick();
        	}
        	waitATick();
        	return triggerHardFault(address, readOnly, write, nomineeFrame);
	} else {
		//what do we do if it's physical address?
		return address;
	}
}

uint64_t Processor::fetchAddressWrite(const uint64_t& address)
{
	const bool readOnly = false;
	int nomineeFrame = -1;
	//implement paging logic
	if (mode == VIRTUAL) {
		uint64_t lineSought = address & lineMask;
		waitATick();
		for (unsigned int i = 0; i < CACHES_AVAILABLE; i++) {
			waitATick();
			uint64_t addressInPageTable = PAGESLOCAL +
				(i * PAGETABLEENTRY) + COREOFFSET;
			uint32_t flags = masterTile->readWord32(addressInPageTable
				+ FLAGOFFSET);
			if (!(flags & 0x01)) {
				nomineeFrame = i;
				continue;
			}
			waitATick();
			uint64_t storedLine = masterTile->readLong(
				addressInPageTable + VOFFSET);
			waitATick();
			if (lineSought == storedLine) {
				waitATick();
				flags |= 0x04;
				masterTile->writeWord32(addressInPageTable +
					FLAGOFFSET, flags);
				masterTile->writeLong(addressInPageTable +
					CLOCKOFFSET, uninterruptedTicks);
				waitATick();
				return generateAddress(i, address);
			}
			waitATick();
		}
		waitATick();
		return triggerHardFault(address, readOnly, true, nomineeFrame);
	} else {
		//what do we do if it's physical address?
		return address;
	}
}

//function to mimic delay from read of global page tables
void Processor::fetchAddressToRegister()
{
    emit hardFault();
    hardFaultCount++;
    requestRemoteMemory(0x0, 0x0, 0x0, false);
}
		
void Processor::writeAddress(const uint64_t& address,
	const uint64_t& value)
{
	uint64_t fetchedAddress = fetchAddressWrite(address);
	masterTile->writeLong(fetchedAddress, value);
}

void Processor::writeAddress64(const uint64_t& address)
{
    writeAddress(address, 0);
}

void Processor::writeAddress32(const uint64_t& address)
{
    writeAddress(address, 0);
}

void Processor::writeAddress16(const uint64_t& address)
{
    writeAddress(address, 0);
}

void Processor::writeAddress8(const uint64_t& address)
{
    writeAddress(address, 0);
}

uint64_t Processor::getLongAddress(const uint64_t& address)
{
	return masterTile->readLong(fetchAddressRead(address));
}

uint8_t Processor::getAddress(const uint64_t& address)
{
	return masterTile->readByte(fetchAddressRead(address));
}

uint64_t Processor::getStackPointer() const
{
    return stackPointer;
}

void Processor::setRegister(const uint64_t& regNumber,
    const uint64_t& value)
{
	//R0 always a zero
	if (regNumber == 0) {
		return;
	} else if (regNumber > REGISTER_FILE_SIZE - 1) {
		throw "Bad register number";
	}
	else {
		registerFile[regNumber] = value;
	}
}

uint64_t Processor::getRegister(const uint64_t& regNumber) const
{
	if (regNumber == 0) {
		return 0;
	}
	else if (regNumber > REGISTER_FILE_SIZE - 1) {
		throw "Bad register number";
	}
	else {
		return registerFile[regNumber];
	}
}

void Processor::setPCNull()
{
	programCounter = 0;
}

void Processor::start()
{
	switchModeVirtual();
	ControlThread *pBarrier = masterTile->getBarrier();
	pBarrier->waitForBegin();
}	

void Processor::pcAdvance(const long count)
{
	programCounter += count;
	fetchAddressRead(programCounter);
	waitATick();
}

void Processor::waitATick()
{
	ControlThread *pBarrier = masterTile->getBarrier();
	pBarrier->releaseToRun();
	totalTicks++;
	if (!inInterrupt) {
		uninterruptedTicks++;
		if (uninterruptedTicks%clockTicks == 0) {
			clockDue = true;
		}
	}	
	if (clockDue && inClock == false) {
		clockDue = false;
		activateClock();
	}
}

void Processor::waitGlobalTick()
{
    for (uint64_t i = 0; i < GLOBALCLOCKSLOW; i++) {
		waitATick();
	}
}

void Processor::pushStackPointer()
{
	stackPointer -= sizeof(uint64_t);
    	if (stackPointer >= stackPointerUnder) {
        	cerr << "Stack Underflow" << endl;
        	throw "Stack Underflow\n";
	}
}

void Processor::popStackPointer()
{
	stackPointer += sizeof(uint64_t);
	if (stackPointer < stackPointerOver) {
        	cerr << "Stack Overflow" << endl;
        	throw "Stack Overflow\n";
	}
}

void Processor::activateClock()
{
	//WS window 10 times bigger than clock sweep
	if (inInterrupt) {
		return;
	}
	inClock = true;
	interruptBegin();
	uint64_t cutoffTime = uninterruptedTicks - (clockTicks * 10);
	for (uint64_t i = 0; i < CACHES_AVAILABLE; i++) {
		waitATick();
		uint64_t flagAddress = COREOFFSET 
			+ PAGESLOCAL + FLAGOFFSET +
			i * PAGETABLEENTRY;
		uint64_t clockAddress = COREOFFSET + PAGESLOCAL +
			CLOCKOFFSET + i * PAGETABLEENTRY;
		uint64_t flags = masterTile->readLong(flagAddress);
		uint64_t clockWas = masterTile->readLong(clockAddress);
		waitATick();
		if (!(flags & 0x01) || !(flags & 0x04)) {
			continue;
		}
		if (clockWas < cutoffTime) {
			flags = flags & (~0x04);
			waitATick();
			masterTile->writeWord32(flagAddress, flags);
			waitATick();
		}
		waitATick();
	}
	waitATick();
	inClock = false;
	interruptEnd();
}
