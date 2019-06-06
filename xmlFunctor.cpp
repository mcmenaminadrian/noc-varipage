#include <cstdlib>
#include <iostream>
#include <vector>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <bitset>
#include <map>
#include <QFile>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/util/XMLString.hpp>
#include <unistd.h>
#include "mainwindow.h"
#include "ControlThread.hpp"
#include "memorypacket.hpp"
#include "mux.hpp"
#include "noc.hpp"
#include "memory.hpp"
#include "tile.hpp"
#include "processor.hpp"
#include "xmlFunctor.hpp"
#include "SAX2Handler.hpp"

using namespace std;
using namespace xercesc;

const uint64_t XMLFunctor::sumCount = 0x101;


//avoid magic numbers


#define SETSIZE 128

XMLFunctor::XMLFunctor(Tile *tileIn):
	tile{tileIn}, proc{tileIn->tileProcessor}
{ }


void XMLFunctor::operator()()
{
    const uint64_t order = tile->getOrder();
    if (order >= SETSIZE) {
        return;
    }
    proc->start();

    uint64_t pass = 0;

    while (true) {
  	string lackeyml("lackeyml_");
    	lackeyml += to_string(tile->getOrder()%8);

    	SAX2XMLReader *parser = XMLReaderFactory::createXMLReader();
    	parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
    	parser->setFeature(XMLUni::fgSAX2CoreNameSpaces, true);

    	SAX2Handler *lackeyHandler = new SAX2Handler();
    	parser->setContentHandler(lackeyHandler);
    	parser->setErrorHandler(lackeyHandler);
    	lackeyHandler->setMemoryHandler(this);

    	try {
        	parser->parse(lackeyml.c_str());
    	}
    	catch (const SAXParseException& toCatch) {
           char* message = XMLString::transcode(toCatch.getMessage());
           cout << "Exception message is: \n"
                << message << "\n";
           XMLString::release(&message);
           exit(1);
    }
	/*
    	cout << "===========" << endl;
    	cout << "On pass " << pass << endl;
    	cout << "Task on " << order << " completed." << endl;
   	cout << "Hard fault count: " << proc->hardFaultCount << endl;
    	cout << "Small fault count: " << proc->smallFaultCount << endl;
    	cout << "Blocks: " << proc->blocks << endl;
    	cout << "Service time: " << proc->serviceTime << endl;
    	cout << "Ticks: " << proc->getTicks() << endl;
    	cout << "===========" << endl;
	*/
    	proc->resetCounters();
    	pass++;
    	delete lackeyHandler;
	delete parser;
    } //off we go again
    proc->getTile()->getBarrier()->decrementTaskCount();
}
