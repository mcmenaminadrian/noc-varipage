#include <QObject>
#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "mainwindow.h"
#include "processor.hpp"
#include "ControlThread.hpp"

using namespace std;

// 30% of 'full' power is used by network
// which we cannot turn off
// hence 35% gives us 5/70 * 128 cores etc
// so 8 cores is 34.375% power
// 12 cores is 36.5625% power
// 16 cores is 38.75%
// 32 cores is 47.5%
// 50% is 36.57 cores ie 36
// 75% is 82.28 cores ie 82 cores
static uint POWER_MAX = 36; //maximum number of active cores

ControlThread::ControlThread(unsigned long tcks, MainWindow *pWind):
    ticks(tcks), taskCount(0), beginnable(false), mainWindow(pWind),
	waitingProcessors()
{
	QObject::connect(this, SIGNAL(updateCycles()),
		pWind, SLOT(updateLCD()));
	blockedInTree = 0;
}

void ControlThread::releaseToRun()
{
	unique_lock<mutex> lck(runLock);
	taskCountLock.lock();
	signedInCount++;
	if (signedInCount >= taskCount) {
		taskCountLock.unlock();
		lck.unlock();
		powerLock.lock();
		powerCount = 0;
		powerLock.unlock();
		run();
		return;
	}
	taskCountLock.unlock();
	go.wait(lck);
}

void ControlThread::sufficientPower(Processor *pActive)
{
       unique_lock<mutex> lck(powerLock);
       bool statePower = pActive->getTile()->getPowerState();
       if (!statePower) {
               lck.unlock();
               return; //already dark
       }
       powerCount++;
       if (powerCount > POWER_MAX) {
               lck.unlock();
               if (!pActive->isInInterrupt()) {
                       pActive->getTile()->setPowerStateOff();
               }
               return; 
       }
       lck.unlock();
       return;
}

bool ControlThread::checkQueue(const uint64_t procNumber)
{
	deque<uint64_t>::iterator it = waitingProcessors.begin();
	while (it != waitingProcessors.end()) {
		if (*it++ == procNumber) {
			return true;
		}
	}
	return false;
}

bool ControlThread::powerToProceed(Processor *pActive, const bool waiting)
{
	unique_lock<mutex> lck(powerLock);
	bool status = checkQueue(pActive->getNumber());
	lck.unlock();
	if (status == false) {
		return true;
	} else {
		if (!waiting) {
			pActive->powerCycleTick();
		}
		pActive->waitATick();
	}
	return false;
}


void ControlThread::incrementTaskCount()
{
	unique_lock<mutex> lock(taskCountLock);
	taskCount++;
}

void ControlThread::decrementTaskCount()
{
	unique_lock<mutex> lck(runLock);
	unique_lock<mutex> lock(taskCountLock);
	taskCount--;
    lock.unlock();
    lck.unlock();
	if (signedInCount >= taskCount) {
		run();
	}
}

void ControlThread::incrementBlocks()
{
	unique_lock<mutex> lck(runLock);
	unique_lock<mutex> lckBlock(blockLock);
	blockedInTree++;
	lckBlock.unlock();
}

void ControlThread::run()
{
	unique_lock<mutex> lck(runLock);
	unique_lock<mutex> lckBlock(blockLock);
	if (blockedInTree > 0) {
		cout << "On tick " << ticks << " total blocks ";
		cout << blockedInTree << endl;
		blockedInTree = 0;
	}
	lckBlock.unlock();
	signedInCount = 0;
	ticks++;
	go.notify_all();
	//update LCD display
	++(mainWindow->currentCycles);
	emit updateCycles();
}

void ControlThread::waitForBegin()
{
	unique_lock<mutex> lck(runLock);
	go.wait(lck, [&]() { return this->beginnable;});
	for (int i = 0; i < POWER_MAX; i++) {
		waitingProcessors.pop_front();
	}
}

void ControlThread::begin()
{
	runLock.lock();
	for (uint64_t i = 0; i < taskCount; i++) {
		waitingProcessors.push_back(i);
	}
	beginnable = true;
	go.notify_all();
	runLock.unlock();
}

bool ControlThread::tryCheatLock()
{
	return cheatLock.try_lock();
}

void ControlThread::unlockCheatLock()
{
	cheatLock.unlock();
}

void ControlThread::switchOffCore(Processor *p)
{
	unique_lock<mutex> lck(powerLock);
	waitingProcessors.push_back(p->getNumber());
	waitingProcessors.pop_front();
	lck.unlock();
}
