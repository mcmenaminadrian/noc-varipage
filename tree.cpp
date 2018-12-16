#include <iostream>
#include <vector>
#include <map>
#include <mutex>
#include <bitset>
#include <condition_variable>
#include "mainwindow.h"
#include "ControlThread.hpp"
#include "memorypacket.hpp"
#include "bus.hpp"
#include "memory.hpp"
#include "tree.hpp"
#include "noc.hpp"
#include "tile.hpp"
#include "processor.hpp"


using namespace std;

Tree::Tree(Memory& globalMemory, Noc& noc, const long columns, const long rows)
{
	long totalLeaves = columns * rows;
	levels = 0;
	long busCount = totalLeaves / 2;
 
	//create the nodes - eight buses at the base
	for (int i = 0; i < 8; i++){
		nodesTree.push_back(Bus(levels, &globalMemory));
	}
	levels++;
	busCount = busCount / 2;
	//the rest of the buses
	while (busCount > 1) {
		nodesTree.push_back(Bus(levels, &globalMemory));
		levels++;
		busCount = busCount/2;
	}
	//number the leaves
	//root bus - connects to global memory
	nodesTree.push_back(Bus(levels, &globalMemory));
	int topBus = nodesTree.size() - 1;
	nodesTree[topBus].assignGlobalMemory(&globalMemory);
	nodesTree[topBus].upstreamBus = nullptr;
	nodesTree[topBus].initialiseMutex();
	//initialise the mutexes
	for (unsigned int i = 0; i < topBus; i++) {
		nodesTree[i].initialiseMutex();
		if (i > 7) {
			nodesTree[i].upstreamBus = & nodesTree[i + 1];
		} else {
			nodesTree[i].upstreamBus = & nodesTree[8];
		} 
	}

	for (int i = 0; i < 128; i++)
	{
		noc.tileAt(i)->addTreeLeaf(&nodesTree[i / 16]);
	}

	//attach root to global memory
	globalMemory.attachTree(&(nodesTree.at(nodesTree.size() - 1)));
}
