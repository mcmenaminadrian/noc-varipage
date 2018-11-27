#include <QObject>
#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <deque>
#include "mainwindow.h"

#ifndef __CONTROLTHREAD_
#define __CONTROLTHREAD_

class Processor;

class ControlThread: public QObject {
    Q_OBJECT

signals:
    void updateCycles();

private:
	uint64_t ticks;
	volatile uint16_t taskCount;
	volatile uint16_t signedInCount;
	volatile uint16_t blockedInTree;
	volatile uint16_t powerCount;
	std::mutex runLock;
	bool beginnable;
	std::condition_variable go;
	std::mutex taskCountLock;
	std::mutex blockLock;
	std::mutex cheatLock;
	std::mutex powerLock;
	MainWindow *mainWindow;
	std::deque<uint64_t> waitingProcessors;
	void run();

public:
	ControlThread(unsigned long count = 0, MainWindow *pWind = nullptr);
	void incrementTaskCount();
	void decrementTaskCount();
	void incrementBlocks();
	void begin();
	void releaseToRun();
	void waitForBegin();
	bool tryCheatLock();
	void unlockCheatLock();
	void sufficientPower(Processor *p);
	bool checkQueue(const uint64_t number);
	bool powerToProceed(Processor *p, const bool waiting);
	void switchOffCore(Processor *p);

};

#endif
