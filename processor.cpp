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
//bit 4 - 0 for ordinary page, 1 for combo page
//bit 5 - 0 for combo low, 1 for combo high

const static uint32_t _VALID_ = 0x01;
const static uint32_t _FIXED_ = 0x02;
const static uint32_t _CLOCK_ = 0x04;
const static uint32_t _READO_ = 0x08;
const static uint32_t _COMBO_ = 0x10;
const static uint32_t _CHIGH_ = 0x20;

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

using namespace std;

Processor::Processor(Tile *parent, MainWindow *mW, uint64_t numb):
	masterTile(parent), mode(REAL), mainWindow(mW)
{
	registerFile = vector<uint64_t>(REGISTER_FILE_SIZE, 0);
	statusWord[0] = true;
	totalTicks = 1;
	currentTLB = 0;
	hardFaultCount = 0;
	smallFaultCount = 0;
    	blocks = 0;
        randomPage = 7;
	inInterrupt = false;
    	processorNumber = numb;
    	clockDue = false;
    	QObject::connect(this, SIGNAL(hardFault()),
        	mW, SLOT(updateHardFaults()));
    	QObject::connect(this, SIGNAL(smallFault()),
        	mW, SLOT(updateSmallFaults()));
}

void Processor::resetCounters()
{
	hardFaultCount = 0;
	smallFaultCount = 0;
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

void Processor::zeroOutTLBs(const uint64_t& frames)
{
	for (unsigned int i = 0; i < frames; i++) {
		tlbs.push_back(tuple<uint64_t, uint64_t, bool>
			(PAGESLOCAL + (1 << pageShift) * i,
			 PAGESLOCAL + (1 << pageShift) * i, false));
	}
}

void Processor::writeOutPageAndBitmapLengths(const uint64_t& reqPTEPages,
	const uint64_t& reqBitmapPages)
{
	masterTile->writeLong(PAGESLOCAL, reqPTEPages);
	masterTile->writeLong(PAGESLOCAL + sizeof(uint64_t),
		reqBitmapPages);
}

void Processor::writeOutBasicPageEntries(const uint64_t& pagesAvailable)
{
	const uint64_t tablesOffset = KERNELPAGES * (1 << pageShift) 
		+ PAGESLOCAL;
	for (unsigned int i = 0; i < pagesAvailable; i++) {
        	uint64_t memoryLocalOffset = i * PAGETABLEENTRY + tablesOffset;
        	masterTile->writeLong(
           		memoryLocalOffset + VOFFSET,
                    	i * (1 << pageShift) + PAGESLOCAL);
        	masterTile->writeLong(
            		memoryLocalOffset + POFFSET,
                    	i * (1 << pageShift) + PAGESLOCAL);
		masterTile->writeLong(
            		memoryLocalOffset + FRAMEOFFSET, i);
		masterTile->writeWord32(memoryLocalOffset + FLAGOFFSET, 0);
	}
}

void Processor::markUpBasicPageEntries(const uint64_t& reqPTEPages,
	const uint64_t& reqBitmapPages)
{
	//mark for page tables, bit map and 2 notional page for kernel
	for (unsigned int i = 0;
			i < (reqPTEPages + reqBitmapPages + KERNELPAGES); i++) {
		const uint64_t pageEntryBase = (1 << pageShift) * KERNELPAGES +
			i * PAGETABLEENTRY + PAGESLOCAL;
		const uint64_t mappingAddress = PAGESLOCAL +
			i * (1 << pageShift);
        	masterTile->writeLong(pageEntryBase + VOFFSET,
            		mappingAddress);
        	masterTile->writeLong(pageEntryBase + POFFSET,
            		mappingAddress);
		masterTile->writeWord32(pageEntryBase + FLAGOFFSET, 0x07);
	}
	//stack
    	uint64_t stackFrame = (TILE_MEM_SIZE >> pageShift) - 1;
	uint64_t stackInTable = (1 << pageShift) * KERNELPAGES + 
        	stackFrame * PAGETABLEENTRY + PAGESLOCAL;
	for (unsigned int i = 0; i < STACKPAGES; i++) {
    		masterTile->writeLong(stackInTable + VOFFSET,
        		stackFrame * (1 << pageShift) + PAGESLOCAL);
    		masterTile->writeLong(stackInTable + POFFSET,
       			stackFrame * (1 << pageShift) + PAGESLOCAL);
		masterTile->writeWord32(stackInTable + FLAGOFFSET, 0x07);
		stackInTable -= PAGETABLEENTRY;
		stackFrame--;
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


void Processor::createMemoryMap(Memory *local, long pShift)
{
	localMemory = local;
	pageShift = pShift;
	memoryAvailable = localMemory->getSize();
	pagesAvailable = memoryAvailable >> pageShift;
	uint64_t requiredPTESize = pagesAvailable * PAGETABLEENTRY;
	uint64_t requiredPTEPages = requiredPTESize >> pageShift;
	if ((requiredPTEPages << pageShift) != requiredPTESize) {
		requiredPTEPages++;
	}

	stackPointer = TILE_MEM_SIZE + PAGESLOCAL;
	stackPointerUnder = stackPointer;
	stackPointerOver = stackPointer - (STACKPAGES << pageShift);

	zeroOutTLBs(pagesAvailable);

	//how many pages needed for bitmaps?
	uint64_t bitmapSize = ((1 << pageShift) / (BITMAP_BYTES)) / 8;
	uint64_t totalBitmapSpace = bitmapSize * pagesAvailable;
	uint64_t requiredBitmapPages = totalBitmapSpace >> pageShift;
	if ((requiredBitmapPages << pageShift) != totalBitmapSpace) {
		requiredBitmapPages++;
	}
	writeOutPageAndBitmapLengths(requiredPTEPages, requiredBitmapPages);
	writeOutBasicPageEntries(pagesAvailable);
	markUpBasicPageEntries(requiredPTEPages, requiredBitmapPages);
	pageMask = 0xFFFFFFFFFFFFFFFF;
	pageMask = pageMask >> pageShift;
	pageMask = pageMask << pageShift;
	bitMask = ~ pageMask;
	uint64_t pageCount =
		requiredPTEPages + requiredBitmapPages + KERNELPAGES;
	for (unsigned int i = 0; i <= pageCount; i++) {
		const uint64_t pageStart =
			PAGESLOCAL + i * (1 << pageShift);
		fixTLB(i, pageStart);
        	for (unsigned int j = 0; j < bitmapSize * BITS_PER_BYTE; j++) {
			markBitmapInit(i, pageStart + j * BITMAP_BYTES);
		}
	}
	//TLB and bitmap for stack
	uint64_t stackPage = PAGESLOCAL + TILE_MEM_SIZE; 
	uint64_t stackPageNumber = pagesAvailable;
	for (unsigned int i = 0; i < STACKPAGES; i++) {
		stackPageNumber--;
		stackPage -= (1 << pageShift);
		fixTLB(stackPageNumber, stackPage);
		for (unsigned int i = 0; i < bitmapSize * BITS_PER_BYTE; i++) {
			markBitmapInit(stackPageNumber, stackPage +
				i * BITMAP_BYTES);
		}
	}
}

bool Processor::isBitmapValid(const uint64_t& address,
	const uint64_t& physAddress) const
{
	const uint64_t pageTablePages = masterTile->readLong(PAGESLOCAL);
	const uint64_t bitmapSize = (1 << pageShift) / (BITMAP_BYTES * 8);
	const uint64_t bitmapOffset = (KERNELPAGES + pageTablePages) * (1 << pageShift);
	uint64_t bitToCheck = ((address & bitMask) / BITMAP_BYTES);
	const uint64_t bitToCheckOffset = bitToCheck / 8;
	bitToCheck %= 8;
	const uint64_t frameNo =
		(physAddress - PAGESLOCAL) >> pageShift;
	const uint8_t bitFromBitmap = 
		masterTile->readByte(PAGESLOCAL + bitmapOffset +
		frameNo * bitmapSize + bitToCheckOffset);
	return bitFromBitmap & (1 << bitToCheck);
}

uint64_t Processor::generateAddress(const uint64_t& frame,
	const uint64_t& address) const
{
	uint64_t offset = address & bitMask;
	return (frame << pageShift) + offset + PAGESLOCAL;
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

void Processor::transferGlobalToLocal(const uint64_t& address,
	const tuple<uint64_t, uint64_t, bool>& tlbEntry,
	const uint64_t& size, const bool& write)
{
	//mimic a DMA call - so need to advance PC
	uint64_t maskedAddress = address & BITMAP_MASK;
	int offset = 0;
	vector<uint8_t> answer = requestRemoteMemory(size,
		maskedAddress, get<1>(tlbEntry) +
		(maskedAddress & bitMask), write);
	for (auto x: answer) {
		masterTile->writeByte(get<1>(tlbEntry) + offset + 
			(maskedAddress & bitMask), x);
		offset++;
	}
}

void Processor::transferLocalToGlobal(const uint64_t& address,
	const tuple<uint64_t, uint64_t, bool>& tlbEntry,
	const uint64_t& size)
{
	//again - this is like a DMA call, there is a delay, but no need
	//to advance the PC
	uint64_t maskedAddress = address & BITMAP_MASK;
	//make the call - ignore the results
	requestRemoteMemory(size, get<0>(tlbEntry), maskedAddress, true);
}

uint64_t Processor::triggerSmallFault(
	const tuple<uint64_t, uint64_t, bool>& tlbEntry,
	const uint64_t& address, const bool& write)
{
	emit smallFault();
	smallFaultCount++;
	interruptBegin();
	transferGlobalToLocal(address, tlbEntry, BITMAP_BYTES, write);
	const uint64_t frameNo =
		(get<1>(tlbEntry) - PAGESLOCAL) >> pageShift;
	markBitmap(frameNo, address);
	interruptEnd();
	return generateAddress(frameNo, address);
}

const pair<const uint64_t, bool> Processor::getRandomFrame()
{
	waitATick();
	//See 3.2.1 of Knuth (third edition)
	//simple ramdom number generator
	randomPage = (1 * randomPage + 1)%FREEPAGES;
	waitATick(); //store
	waitATick(); //read
	uint32_t flags = masterTile->readWord32((1 << pageShift) * KERNELPAGES
		+ randomPage * PAGETABLEENTRY + FLAGOFFSET + PAGESLOCAL);
	waitATick();
	if (flags & _COMBO_) {
		waitATick();
		if (flags & _CHIGH_) {
			//have to take 'random' page but opt for lower one
			waitATick();
			randomPage--;
		}
	}
	return pair<const uint64_t, bool>(randomPage + BASEPAGES, true);
}

//nominate a frame to be used
const pair<const uint64_t, bool> Processor::getFreeFrame()
{
	//have we any empty frames?
	//we assume this to be subcycle
	uint64_t frames = (localMemory->getSize()) >> pageShift;
	uint64_t couldBe = 0xFFFF;
	const uint64_t basePageTable = (1 << pageShift) * KERNELPAGES +
		PAGESLOCAL;
	for (uint64_t i = 0; i < frames; i++) {
		uint32_t flags = masterTile->readWord32(basePageTable
			+ i * PAGETABLEENTRY + FLAGOFFSET);
		if (!(flags & _VALID_)) {
			return pair<const uint64_t, bool>(i, false);
		}
        	if (flags & _FIXED_) {
			continue;
		}
        	else if (!(flags & _CLOCK_)) {
			//no longer subcycle - too complex
			waitATick();
			if (flags & _COMBO_) {
				waitATick();
				uint32_t comboFlags = masterTile->
					readWord32(basePageTable +
					(i + 1) * PAGETABLEENTRY + FLAGOFFSET);
				waitATick();
				if (comboFlags & _CLOCK_) {
					waitATick();
					i++;
					continue;
				} else {
					waitATick();
					couldBe = i;
					waitATick();
					i++;
					continue;
				}
			} 
			couldBe = i;
		}
	}
	if (couldBe < 0xFFFF) {
		return pair<const uint64_t, bool>(couldBe, true);
	}
	//no free frames, so we have to pick one
	return getRandomFrame();
}

//drop page from TLBs and page tables - no write back
void Processor::dropPage(const uint64_t& frameNo)
{
	waitATick();
	//firstly get the address
	const uint64_t pageAddress = masterTile->readLong(
		frameNo * PAGETABLEENTRY + PAGESLOCAL + VOFFSET +
		(1 << pageShift)* KERNELPAGES);
	dumpPageFromTLB(pageAddress);
	//mark as invalid in page table
	waitATick();
	masterTile->writeWord32(frameNo * PAGETABLEENTRY + PAGESLOCAL +
		FLAGOFFSET + (1 << pageShift) * KERNELPAGES, 0);
}

//only used to dump a frame
void Processor::writeBackMemory(const uint64_t& frameNo)
{
	//is this a read-only frame?
	if (localMemory->readWord32((1 << pageShift) * KERNELPAGES +
		frameNo * PAGETABLEENTRY + FLAGOFFSET) & 0x08) {
    		return;
	}
	//find bitmap for this frame
	const uint64_t totalPTEPages =
		masterTile->readLong(fetchAddressRead(PAGESLOCAL));
	const uint64_t bitmapOffset =
		(KERNELPAGES + totalPTEPages) * (1 << pageShift);
	const uint64_t bitmapSize = (1 << pageShift) / BITMAP_BYTES;
	uint64_t bitToRead = frameNo * bitmapSize;
	const uint64_t physicalAddress = mapToGlobalAddress(
		localMemory->readLong((1 << pageShift) * KERNELPAGES +
		frameNo * PAGETABLEENTRY)).first;
	long byteToRead = -1;
	uint8_t byteBit = 0;
	for (unsigned int i = 0; i < bitmapSize; i++)
	{
		long nextByte = bitToRead / 8;
		if (nextByte != byteToRead) {
			byteBit =
				localMemory->readByte(bitmapOffset + nextByte);
			byteToRead = nextByte;
		}
		uint8_t actualBit = bitToRead%8;
		if (byteBit & (1 << actualBit)) {
			//simulate transfer
			transferLocalToGlobal(frameNo * (1 << pageShift) +
				PAGESLOCAL +
				i * BITMAP_BYTES, tlbs[frameNo], BITMAP_BYTES);
			for (unsigned int j = 0;
				j < BITMAP_BYTES/sizeof(uint64_t); j++)
			{
				//actual transfer done in here
				waitATick();
				uint64_t toGo = masterTile->readLong(
					fetchAddressRead(
					frameNo * (1 << pageShift) +
					PAGESLOCAL + i * BITMAP_BYTES +
					j * sizeof(uint64_t)));
				masterTile->writeLong(fetchAddressWrite(
					physicalAddress + i * BITMAP_BYTES
					+ j * sizeof(uint64_t)), toGo);
			}
		}
		bitToRead++;
	}
}

void Processor::fixPageMap(const uint64_t& frameNo,
	const uint64_t& address, const bool& readOnly)
{
	const uint64_t pageAddress = address & pageMask;
	const uint64_t writeBase =
		KERNELPAGES * (1 << pageShift) + frameNo * PAGETABLEENTRY;
	waitATick();
	localMemory->writeLong(writeBase + VOFFSET, pageAddress);
	waitATick();
	if (readOnly) {
		localMemory->writeWord32(writeBase + FLAGOFFSET, 0x0D);
	} else {
		localMemory->writeWord32(writeBase + FLAGOFFSET, 0x05);
	}
}

void Processor::fixPageMapCombo(const uint64_t& frameNo,
	const uint64_t& address, const bool& readOnly)
{
	const uint64_t pageAddress = address & pageMask;
	const uint64_t writeBase =
		KERNELPAGES * (1 << pageShift) + frameNo * PAGETABLEENTRY;
	waitATick();
	localMemory->writeLong(writeBase + VOFFSET, pageAddress);
	waitATick();
	if (readOnly) {
		localMemory->writeWord32(writeBase + FLAGOFFSET,
			_VALID_|_CLOCK_|_COMBO_|_CHIGH_|_READO_);
		waitATick();
		localMemory->writeWord32(
			writeBase + FLAGOFFSET - PAGETABLEENTRY,
			_VALID_|_CLOCK_|_COMBO_|_READO_);
	} else {
		localMemory->writeWord32(writeBase + FLAGOFFSET,
			_VALID_|_CLOCK_|_COMBO_|_CHIGH_);
		waitATick();
		localMemory->writeWord32(
			writeBase + FLAGOFFSET - PAGETABLEENTRY,
			_VALID_|_CLOCK_|_COMBO_);
	}
	
}

void Processor::cleanPageMapCombo(const uint64_t& frameNo)
{
	const uint64_t writeBase = KERNELPAGES * (1 << pageShift) +
		frameNo * PAGETABLEENTRY;
	waitATick();
	uint32_t flags = localMemory->readWord32(writeBase + FLAGOFFSET);
	waitATick();
	if (!flags & _COMBO_) {
		return;
	}
	waitATick();
	flags = localMemory->readWord32(
		writeBase + PAGETABLEENTRY + FLAGOFFSET);
	waitATick();
	flags &= 0xCF;
	waitATick();
	localMemory->writeWord32(
		writeBase + PAGETABLEENTRY + FLAGOFFSET, flags);
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

void Processor::fixBitmap(const uint64_t& frameNo)
{
	const uint64_t totalPTEPages =
		masterTile->readLong(fetchAddressRead(PAGESLOCAL));
	uint64_t bitmapOffset =
		(KERNELPAGES + totalPTEPages) * (1 << pageShift);
	const uint64_t bitmapSizeBytes =
		(1 << pageShift) / (BITMAP_BYTES * 8);
	const uint64_t bitmapSizeBits = bitmapSizeBytes * 8;
	uint8_t bitmapByte = localMemory->readByte(
		frameNo * bitmapSizeBytes + bitmapOffset);
	uint8_t startBit = (frameNo * bitmapSizeBits) % 8;
	for (uint64_t i = 0; i < bitmapSizeBits; i++) {
		bitmapByte = bitmapByte & ~(1 << startBit);
		startBit++;
		startBit %= 8;
		if (startBit == 0) {
			localMemory->writeByte(
				frameNo * bitmapSizeBytes + bitmapOffset,
				bitmapByte);
			bitmapByte = localMemory->readByte(
				++bitmapOffset + frameNo * bitmapSizeBytes);
		}
	}
}

void Processor::markBitmapStart(const uint64_t &frameNo,
    const uint64_t &address)
{
	const uint64_t totalPTEPages =
		masterTile->readLong(PAGESLOCAL);
	uint64_t bitmapOffset =
		(KERNELPAGES + totalPTEPages) * (1 << pageShift);
	const uint64_t bitmapSizeBytes =
		(1 << pageShift) / (BITMAP_BYTES * 8);
	for (unsigned int i = 0; i < bitmapSizeBytes; i++) {
		localMemory->writeByte(
			frameNo * bitmapSizeBytes + i + bitmapOffset, '\0');
	}
	uint64_t bitToMark = (address & bitMask) / BITMAP_BYTES;
	const uint64_t byteToFetch = (bitToMark / 8) +
		frameNo * bitmapSizeBytes + bitmapOffset;
	bitToMark %= 8;
	uint8_t bitmapByte = localMemory->readByte(byteToFetch);
	bitmapByte |= (1 << bitToMark);
	localMemory->writeByte(byteToFetch, bitmapByte);
}

void Processor::markBitmap(const uint64_t& frameNo,
	const uint64_t& address)
{
	const uint64_t totalPTEPages =
		masterTile->readLong(PAGESLOCAL);
	uint64_t bitmapOffset =
		(KERNELPAGES + totalPTEPages) * (1 << pageShift);
	const uint64_t bitmapSizeBytes =
		(1 << pageShift) / (BITMAP_BYTES * 8);
	uint64_t bitToMark = (address & bitMask) / BITMAP_BYTES;
	const uint64_t byteToFetch = (bitToMark / 8) +
		frameNo * bitmapSizeBytes + bitmapOffset;
	bitToMark %= 8;
	uint8_t bitmapByte = localMemory->readByte(byteToFetch);
	bitmapByte |= (1 << bitToMark);
	localMemory->writeByte(byteToFetch, bitmapByte);
	for (uint64_t i = 0; i < BITMAPDELAY; i++) {
		waitATick();
    	}
}

void Processor::markBitmapInit(const uint64_t& frameNo,
    const uint64_t& address)
{
	const uint64_t totalPTEPages =
		masterTile->readLong(PAGESLOCAL);
	uint64_t bitmapOffset =
		(KERNELPAGES + totalPTEPages) * (1 << pageShift);
	const uint64_t bitmapSizeBytes =
		(1 << pageShift) / (BITMAP_BYTES * 8);
	uint64_t bitToMark = (address & bitMask) / BITMAP_BYTES;
	const uint64_t byteToFetch = (bitToMark / 8) +
		frameNo * bitmapSizeBytes + bitmapOffset;
	bitToMark %= 8;
	uint8_t bitmapByte = localMemory->readByte(byteToFetch);
	bitmapByte |= (1 << bitToMark);
	localMemory->writeByte(byteToFetch, bitmapByte);
}

void Processor::fixTLB(const uint64_t& frameNo,
	const uint64_t& address)
{
	const uint64_t pageAddress = address & pageMask;
	get<1>(tlbs[frameNo]) = frameNo * (1 << pageShift) + PAGESLOCAL;
	get<0>(tlbs[frameNo]) = pageAddress;
	get<2>(tlbs[frameNo]) = true;
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
	const bool& readOnly, const bool& write)
{
	emit hardFault();
	hardFaultCount++;
	interruptBegin();
	const pair<const uint64_t, bool> frameData = getFreeFrame();
	if (frameData.second) {
		//have to process for combo pages now
		cleanPageMapCombo(frameData.first);
		writeBackMemory(frameData.first);
	}
	fixBitmap(frameData.first);
	pair<uint64_t, uint8_t> translatedAddress = mapToGlobalAddress(address);
	fixTLB(frameData.first, translatedAddress.first);
	transferGlobalToLocal(translatedAddress.first + (address & bitMask),
		tlbs[frameData.first], BITMAP_BYTES, write);
	fixPageMap(frameData.first, translatedAddress.first, readOnly);
	markBitmapStart(frameData.first, translatedAddress.first +
		(address & bitMask));
	for (uint64_t i = 0; i < BITMAPDELAY; i++) {
		waitATick();
	}
	interruptEnd();
	return generateAddress(frameData.first, translatedAddress.first +
		(address & bitMask));
}

uint64_t Processor::triggerComboPageCreate(int& frameNo,
	const uint64_t& address, const bool& readOnly,
	const bool& write)
{
	emit hardFault();
	hardFaultCount++;
	interruptBegin();
	waitATick();
	frameNo++;
	writeBackMemory(frameNo);
	fixBitmap(frameNo);
	pair<uint64_t, uint8_t> translatedAddress = mapToGlobalAddress(address);
	fixTLB(frameNo, translatedAddress.first);
	transferGlobalToLocal(translatedAddress.first + (address & bitMask),
		tlbs[frameNo], BITMAP_BYTES, write);
	fixPageMapCombo(frameNo, translatedAddress.first, readOnly);
	markBitmapStart(frameNo, translatedAddress.first +
		(address & bitMask));
	for (uint64_t i = 0; i < BITMAPDELAY; i++) {
		waitATick();
	}
	interruptEnd();
	return generateAddress(frameNo, translatedAddress.first +
		(address & bitMask));
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
	if (mode == VIRTUAL) {
		uint64_t pageSought = address & pageMask;
		uint64_t y = 0;
		for (auto x: tlbs) {
			if (get<2>(x) && ((pageSought) ==
						(get<0>(x) & pageMask))) {
				//entry in TLB - check bitmap
				for (uint64_t i = 0; i < BITMAPDELAY; i++) {
					waitATick();
				}
				if (!isBitmapValid(address, get<1>(x))) {
					return triggerSmallFault(x,
						address, write);
				}
				return generateAddress(y, address);
			}
            		y++;
		}
		//for combopages
		auto comboPage = pair<bool, int>(false, -1);
		//not in TLB - but check if it is in page table
		waitATick(); 
		for (unsigned int i = 0; i < TOTAL_LOCAL_PAGES; i++) {
			waitATick();
            		uint64_t addressInPageTable = PAGESLOCAL +
                        	(i * PAGETABLEENTRY) + 
				(1 << pageShift) * KERNELPAGES;
            		uint32_t flags =
				masterTile->readWord32(addressInPageTable
                        	+ FLAGOFFSET);
            		if (!(flags & _VALID_)) {
                		continue;
            		}
            		waitATick();
            		uint64_t storedPage = masterTile->readLong(
                        	addressInPageTable + VOFFSET);
            		waitATick();
            		if (pageSought == storedPage) {
                		waitATick();
                		flags |= _CLOCK_;
				waitATick();
				if (flags & _COMBO_) {
					//must be low
					waitATick();
					uint32_t nextFlags = masterTile->
						readWord32(addressInPageTable +
						PAGETABLEENTRY + FLAGOFFSET);
					waitATick();
					nextFlags |= _CLOCK_;
					waitATick();
					masterTile->writeWord32(
						addressInPageTable +
						PAGETABLEENTRY + FLAGOFFSET,
						nextFlags);
					waitATick();
					fixTLB(i + 1, pageSought +
						(1 << pageShift));
					waitATick();
				}		
                		masterTile->writeWord32(
					addressInPageTable + FLAGOFFSET,
                    			flags);
                		waitATick();
                		fixTLB(i, address);
                		waitATick();
                		return fetchAddressRead(address);
            		} else if (i < (TOTAL_LOCAL_PAGES - (STACKPAGES + 1)) &&
				!comboPage.first && (pageSought ==
				storedPage + (1 << pageShift))) {
				waitATick();
				comboPage.first = true;
				waitATick();
				comboPage.second = i;
			}	
			waitATick();
        	}
        	waitATick();
		if (comboPage.first) {
			return triggerComboPageCreate(comboPage.second,
				address, readOnly, write);
		} else {
        		return triggerHardFault(address, readOnly, write);
		}
	} else {
		//what do we do if it's physical address?
		return address;
	}
}

uint64_t Processor::fetchAddressWrite(const uint64_t& address)
{
	const bool readOnly = false;
	//implement paging logic
	if (mode == VIRTUAL) {
		uint64_t pageSought = address & pageMask;
		uint64_t y = 0;
		for (auto x: tlbs) {
			if (get<2>(x) && ((pageSought) ==
				(get<0>(x) & pageMask))) {
				//ensure marked as writable page
				uint64_t baseAddress = PAGESLOCAL +
					(y * PAGETABLEENTRY) +
					(1 << pageShift) * KERNELPAGES;
				uint32_t oldFlags = masterTile->
					readWord32(baseAddress + FLAGOFFSET);
				if (!(oldFlags & 0x08)) {
					waitATick();	
					masterTile->writeWord32(baseAddress +
						FLAGOFFSET, oldFlags|0x08);
					waitATick();
				}
				for (uint64_t i = 0; i < BITMAPDELAY; i++) {
					waitATick();
				}
				if (!isBitmapValid(address, get<1>(x))) {
					return triggerSmallFault(x, address,
						true);
				}
				return generateAddress(y, address);
			}
			y++;
		}
		//for combopages
		auto comboPage = pair<bool, int>(false, -1);
		//not in TLB - but check if it is in page table
		waitATick();
		for (unsigned int i = 0; i < TOTAL_LOCAL_PAGES; i++) {
			waitATick();
			uint64_t addressInPageTable = PAGESLOCAL +
				(i * PAGETABLEENTRY) +
				(1 << pageShift) * KERNELPAGES;
			uint32_t flags = masterTile->readWord32(addressInPageTable
				+ FLAGOFFSET);
			if (!(flags & 0x01)) {
				continue;
			}
			waitATick();
			uint64_t storedPage = masterTile->readLong(
				addressInPageTable + VOFFSET);
			waitATick();
            		if (pageSought == storedPage) {
                		waitATick();
                		flags |= _CLOCK_;
				waitATick();
				if (flags & _COMBO_) {
					//must be low
					waitATick();
					uint32_t nextFlags = masterTile->
						readWord32(addressInPageTable +
						PAGETABLEENTRY + FLAGOFFSET);
					waitATick();
					nextFlags |= _CLOCK_;
					waitATick();
					masterTile->writeWord32(
						addressInPageTable +
						PAGETABLEENTRY + FLAGOFFSET,
						nextFlags);
					waitATick();
					fixTLB(i + 1, pageSought +
						(1 << pageShift));
					waitATick();
				}		
                		masterTile->writeWord32(
					addressInPageTable + FLAGOFFSET,
                    			flags);
                		waitATick();
                		fixTLB(i, address);
                		waitATick();
                		return fetchAddressRead(address);
            		} else if (i < (TOTAL_LOCAL_PAGES - (STACKPAGES + 1)) &&
				!comboPage.first && (pageSought ==
				storedPage + (1 << pageShift))) {
				waitATick();
				comboPage.first = true;
				waitATick();
				comboPage.second = i;
			}	
			waitATick();
        	}
        	waitATick();
		if (comboPage.first) {
			return triggerComboPageCreate(comboPage.second,
				address, readOnly, true);
		} else {
        		return triggerHardFault(address, readOnly, true);
		}
	} else {
		//what do we do if it's physical address?
		return address;
	}
}

//function to mimic delay from read of global page tables
void Processor::fetchAddressToRegister()
{
    emit smallFault();
    smallFaultCount++;
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
	if (totalTicks%clockTicks == 0) {
		clockDue = true;
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
	if (inInterrupt) {
		return;
	}
	inClock = true;
	uint64_t pages = TILE_MEM_SIZE >> pageShift;
	interruptBegin();
	int wiped = 0;
	for (uint8_t i = 0; i < pages; i++) {
		waitATick();
		uint64_t flagAddress = (1 << pageShift) * KERNELPAGES 
			+ PAGESLOCAL + FLAGOFFSET +
			((i + currentTLB) % pagesAvailable) * PAGETABLEENTRY;
		uint32_t flags = masterTile->readWord32(flagAddress);
		waitATick();
		if (!(flags & 0x01) || flags & 0x02) {
			continue;
		}
		flags = flags & (~0x04);
		waitATick();
		masterTile->writeWord32(flagAddress, flags);
		waitATick();
		get<2>(tlbs[(i + currentTLB) % pagesAvailable]) = false;
		if (++wiped >= clockWipe){
			break;
		}
	}
	waitATick();
	currentTLB = (currentTLB + clockWipe) % pagesAvailable;
	inClock = false;
	interruptEnd();
}

void Processor::dumpPageFromTLB(const uint64_t& address)
{
	waitATick();
	uint64_t pageAddress = address & pageMask;
	for (auto& x: tlbs) {
		if (get<0>(x) == pageAddress) {
			get<2>(x) = false;
			break;
		}
	}
}
