#include <iostream>
#include <vector>
#include <map>
#include <mutex>
#include <bitset>
#include <condition_variable>
#include "mainwindow.h"
#include "ControlThread.hpp"
#include "memorypacket.hpp"
#include "mux.hpp"
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

	//create the nodes
	while (busCount > 1) {
		nodesTree.push_back(bus(levels, &globalMemory));
		muxCount /= 2;
		levels++;
	}
	//number the leaves
	//root bus - connects to global memory
	nodesTree.push_back(bus(levels, &globalMemory);
	nodesTree[levels][0].assignGlobalMemory(&globalMemory);
	nodesTree[levels][0].upstreamBus = -1;
	nodesTree[levels][0].addMMUMutex();
	//initialise the mutexes
	for (unsigned int i = 0; i < nodesTree.size(); i++) {
			nodesTree[i].initialiseMutex();
	}

	//attach root to global memory
	globalMemory.attachTree(&(nodesTree.at(nodesTree.size() - 1)[0]));
}
