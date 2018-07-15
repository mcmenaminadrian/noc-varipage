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


#ifndef _PROCESSOR_CLASS_
#define _PROCESSOR_CLASS_

//Page table entries - physical addr, virtual addr, frame no, flags

#define PAGETABLEENTRY (8 + 8 + 8 + 4 + 8)
#define VOFFSET 0
#define POFFSET 8
#define FRAMEOFFSET 16
#define FLAGOFFSET 24
#define CLOCKOFFSET 28
#define ENDOFFSET 36


static const uint64_t REGISTER_FILE_SIZE = 32;
//page mappings
static const uint64_t PAGESLOCAL = 0xA000000000000000;
static const uint64_t GLOBALCLOCKSLOW = 1;
static const uint64_t TOTAL_LOCAL_PAGES = TILE_MEM_SIZE >> PAGE_SHIFT;
static const uint64_t BITS_PER_BYTE = 8;

class Tile;

class Processor: public QObject {
    Q_OBJECT

signals:
    void hardFault();

private:
	std::mutex interruptLock;
	std::mutex waitMutex;
	std::vector<uint64_t> registerFile;
	bool carryBit;
	uint64_t programCounter;
	Tile *masterTile;
	enum ProcessorMode { REAL, VIRTUAL };
	ProcessorMode mode;
	Memory *localMemory;
	MainWindow *mainWindow;
	long pageShift;
	uint64_t stackPointer;
	uint64_t stackPointerOver;
	uint64_t stackPointerUnder;
	uint64_t maskAddress;
	uint64_t maskLine;
	uint64_t memoryAvailable;
	uint64_t pagesAvailable;
	uint64_t processorNumber;
	uint64_t randomPage;
	bool inInterrupt;
	bool inClock;
	bool clockDue;
	uint64_t fetchAddressRead(const uint64_t& address,
		const bool& readOnly = true, const bool& write = false);
    	uint64_t fetchAddressWrite(const uint64_t& address);
	bool isBitmapValid(const uint64_t& address,
		const uint64_t& physAddress) const;
	uint64_t generateAddress(const uint64_t& frame,
	const uint64_t& address);
	void interruptBegin();
	void interruptEnd();
	void transferGlobalToLocal(const uint64_t& address,
		const uint64_t& localAddress,
    		const uint64_t& size, const bool& write);
    	uint64_t triggerHardFault(const uint64_t& address, const bool& readOnly,
        	const bool& write, const int& nomineeFrame);
	std::pair<uint64_t, bool> getFreeFrame();
	std::pair<uint64_t, bool> getFrameStatus(int nominatedFrame);
	void fixPageMap(const uint64_t& frameNo,
        	const uint64_t& address, const bool& readOnly);
	const std::vector<uint8_t>
		requestRemoteMemory(
		const uint64_t& size, const uint64_t& remoteAddress,
       		const uint64_t& localAddress, const bool& write);
    	const std::pair<uint64_t, uint8_t>
        	mapToGlobalAddress(const uint64_t& address);
    	void fetchAddressToRegister();
	void activateClock();
	void writeOutBasicPageTableEntries();
	//adjust numbers below to change how CLOCK fuctions
    	const uint8_t clockWipe = 1;
    	const uint16_t clockTicks = 2000;
	uint64_t totalTicks;
	uint64_t uninterruptedTicks;
	uint64_t currentTLB;

public:
	std::bitset<16> statusWord;
    	Processor(Tile* parent, MainWindow *mW, uint64_t numb);
	void loadMem(const long regNo, const uint64_t memAddr);
	void switchModeReal();
	void switchModeVirtual();
	void setMode();
	void createMemoryMap(Memory *local);
	void setPCNull();
	void start();
	void pcAdvance(const long count = sizeof(long));
    	uint64_t getRegister(const uint64_t& regNumber) const;
    	void setRegister(const uint64_t& regNumber,
        	const uint64_t& value);
	uint8_t getAddress(const uint64_t& address);
    	uint64_t multiplyWithCarry(const uint64_t& A,
        	const uint64_t& B);
    	uint64_t subtractWithCarry(const uint64_t& A,
        	const uint64_t& B);
	uint64_t getLongAddress(const uint64_t& address);
	void writeAddress(const uint64_t& addr,
		const uint64_t& value);
    	void writeAddress64(const uint64_t& addr);
    	void writeAddress32(const uint64_t& addr);
    	void writeAddress16(const uint64_t& addr);
    	void writeAddress8(const uint64_t& addr);
	void pushStackPointer();
	void popStackPointer();
    	uint64_t getStackPointer() const;
	void setStackPointer(const uint64_t& address) { 
		stackPointer = address; }
    	uint64_t getProgramCounter() const {
        	return programCounter;
    	}
    	void setProgramCounter(const uint64_t& address) {
        	programCounter = address;
        	fetchAddressRead(address, true);
    	}
    	void checkCarryBit();
    	void writeBackMemory(const uint64_t& frameNo);
    	void transferLocalToGlobal(const uint64_t& address,
        	const uint64_t& frameNo);
	void waitATick();
	void waitGlobalTick();
	Tile* getTile() const { return masterTile; }
   	uint64_t getNumber() { return processorNumber; }
   	void flushPagesStart();
    	void flushPagesEnd();
    	void dropPage(const uint64_t& frameNo);
    	void dumpPageFromTLB(const uint64_t& address);
    	const uint64_t& getTicks() const { return totalTicks; }
	void incrementBlocks();
        void incrementServiceTime();
        void resetCounters();
    	uint64_t hardFaultCount;
    	uint64_t blocks;
    	uint64_t serviceTime;
};
#endif
