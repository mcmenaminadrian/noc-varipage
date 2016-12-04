#include <iostream>
#include <vector>
#include <xercesc/sax2/Attributes.hpp>
#include "memorypacket.hpp"
#include "mux.hpp"
#include "ControlThread.hpp"
#include "tile.hpp"
#include "processor.hpp"
#include "xmlFunctor.hpp"
#include "SAX2Handler.hpp"


using namespace std;
using namespace xercesc;


#define instruction 0
#define load 1
#define store 2
#define modify 3
#define nothing 4

static const int parseType(string memType)
{
    if (memType == "instruction") {
           return instruction;
    }
    if (memType == "load") {
        return load;
    }
    if (memType == "store") {
        return store;
    }
    if (memType == "modify") {
        return modify;
    }
    return nothing;
}


SAX2Handler::SAX2Handler()
{ 
	memoryHandler = nullptr;
}

void SAX2Handler::setMemoryHandler(XMLFunctor *handler)
{
    memoryHandler = handler;
}

void SAX2Handler::startElement(const XMLCh* const uri,
	const XMLCh* const localname, const XMLCh* const qname,
	const Attributes& attrs)
{

	//test code
 //   char *message = XMLString::transcode(localname);
  //  cout << "Element: " << message << endl;
  //  XMLString::release(&message);


    XMLCh *addressStr = XMLString::transcode("address");
    XMLCh *sizeStr = XMLString::transcode("size");

    char *memAccess = XMLString::transcode(localname);
    auto typeXML = parseType(memAccess);
    char *address = XMLString::transcode(attrs.getValue(addressStr));
    char *size = XMLString::transcode(attrs.getValue(sizeStr));
    if (address) {
        string addrStr(address);
        switch (typeXML) {
            case instruction:
                memoryHandler->proc->setProgramCounter(
                            stol(addrStr, nullptr, 16));
                memoryHandler->proc->pcAdvance(atoi(size));
                XMLString::release(&address);
                XMLString::release(&size);
                break;
            case store:
                XMLString::release(&address);
                XMLString::release(&size);
                break;
            case load:
                XMLString::release(&address);
                XMLString::release(&size);
                break;
        default:
            break;
        }
    }

}

void SAX2Handler::fatalError(const SAXParseException& exception)
{
    char *message = XMLString::transcode(exception.getMessage());
	cout << "Fatal error: " << message << " at line ";
	cout << exception.getLineNumber() << endl;
	XMLString::release(&message);
}

