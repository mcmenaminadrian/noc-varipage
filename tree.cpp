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

	//root bus - connects to global memory
	nodesTree.push_back(Bus(levels, &globalMemory));
	nodesTree[levels].assignGlobalMemory(&globalMemory);
	nodesTree[levels].upstreamBus = nullptr;
	nodesTree[levels].initialiseMutex();

	for (int i = 0; i < 128; i++)
	{
		noc.tileAt(i)->addTreeLeaf(&nodesTree[0]);
	}

	//attach root to global memory
	globalMemory.attachTree(&(nodesTree.at(nodesTree.size() - 1)));
}
