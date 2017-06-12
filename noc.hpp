#ifndef _NOC_CLASS_
#define _NOC_CLASS_

class Tile;
class Tree;
class PageTable;
class Memory;
#include "mainwindow.h"

class Noc {

#define APNUMBERSIZE 8

private:
	const long columnCount;
	const long rowCount;
	const uint64_t blockSize;
	unsigned long ptrBasePageTables;
	std::vector<std::vector<Tile * > > tiles;
	std::vector<long> answers;
	std::vector<std::vector<long> > lines;
	unsigned long createBasicPageTables();
	unsigned long scanLevelFourTable(unsigned long addr);
	ControlThread *pBarrier;
	std::vector<Memory> globalMemory;
    	MainWindow *mainWindow;

public:
	std::vector<Memory>& getGlobal() { return globalMemory;}
	const long memoryBlocks;
	std::vector<Tree *> trees;
	Noc(const long columns, const long rows, const long pageShift,
		const uint64_t bSize, MainWindow *pWind, const long memBlocks);
	~Noc();
	Tile* tileAt(long i);
	long executeInstructions();
	unsigned long getBasePageTables() const { return ptrBasePageTables; }
	long getColumnCount() const { return columnCount;}
	long getRowCount() const { return rowCount; }
	ControlThread *getBarrier();
};

#endif

template <typename T> int sgn(T val) {
    	return (T(0) < val) - (val < T(0));
}
