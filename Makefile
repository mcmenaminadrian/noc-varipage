#############################################################################
# Makefile for building: noc-qt
# Generated by qmake (2.01a) (Qt 4.8.2) on: Sun Dec 18 21:47:32 2016
# Project:  noc-qt.pro
# Template: app
# Command: /usr/bin/qmake -o Makefile noc-qt.pro
#############################################################################

####### Compiler, tools and options

CC            = gcc
CXX           = g++
DEFINES       = -DQT_WEBKIT -DQT_NO_DEBUG -DQT_GUI_LIB -DQT_CORE_LIB -DQT_SHARED
CFLAGS        = -m64 -pipe -O2 -Wall -W -D_REENTRANT $(DEFINES)
CXXFLAGS      = -m64 -pipe -std=c++11 -g -O0 -Wall -W -D_REENTRANT $(DEFINES)
INCPATH       = -I/usr/share/qt4/mkspecs/linux-g++-64 -I. -I/home/adrian/local/include -I/usr/include/qt4/QtCore -I/usr/include/qt4/QtGui -I/usr/include/qt4 -I/usr/local/include 
LINK          = g++
LFLAGS        = -m64 -Wl,-O1
LIBS          = $(SUBLIBS)  -L/usr/lib/x86_64-linux-gnu -lQtGui -lQtCore -lpthread -L/home/adrian/local/lib -lxerces-c
AR            = ar cqs
RANLIB        = 
QMAKE         = /usr/bin/qmake
TAR           = tar -cf
COMPRESS      = gzip -9f
COPY          = cp -f
SED           = sed
COPY_FILE     = $(COPY)
COPY_DIR      = $(COPY) -r
STRIP         = strip
INSTALL_FILE  = install -m 644 -p
INSTALL_DIR   = $(COPY_DIR)
INSTALL_PROGRAM = install -m 755 -p
DEL_FILE      = rm -f
SYMLINK       = ln -f -s
DEL_DIR       = rmdir
MOVE          = mv -f
CHK_DIR_EXISTS= test -d
MKDIR         = mkdir -p

####### Output directory

OBJECTS_DIR   = ./

####### Files

SOURCES       = main.cpp \
		mainwindow.cpp \
		ControlThread.cpp \
		memory.cpp \
		memorypacket.cpp \
		mux.cpp \
		noc.cpp \
		numberpage.cpp \
		paging.cpp \
		processor.cpp \
		SAX2Handler.cpp \
		xmlFunctor.cpp \
		tile.cpp \
		tree.cpp moc_mainwindow.cpp \
		moc_ControlThread.cpp \
		moc_processor.cpp
OBJECTS       = main.o \
		mainwindow.o \
		ControlThread.o \
		memory.o \
		memorypacket.o \
		mux.o \
		noc.o \
		numberpage.o \
		paging.o \
		processor.o \
		SAX2Handler.o \
		xmlFunctor.o \
		tile.o \
		tree.o \
		moc_mainwindow.o \
		moc_ControlThread.o \
		moc_processor.o
DIST          = /usr/share/qt4/mkspecs/common/unix.conf \
		/usr/share/qt4/mkspecs/common/linux.conf \
		/usr/share/qt4/mkspecs/common/gcc-base.conf \
		/usr/share/qt4/mkspecs/common/gcc-base-unix.conf \
		/usr/share/qt4/mkspecs/common/g++-base.conf \
		/usr/share/qt4/mkspecs/common/g++-unix.conf \
		/usr/share/qt4/mkspecs/qconfig.pri \
		/usr/share/qt4/mkspecs/modules/qt_webkit_version.pri \
		/usr/share/qt4/mkspecs/features/qt_functions.prf \
		/usr/share/qt4/mkspecs/features/qt_config.prf \
		/usr/share/qt4/mkspecs/features/exclusive_builds.prf \
		/usr/share/qt4/mkspecs/features/default_pre.prf \
		/usr/share/qt4/mkspecs/features/release.prf \
		/usr/share/qt4/mkspecs/features/default_post.prf \
		/usr/share/qt4/mkspecs/features/unix/gdb_dwarf_index.prf \
		/usr/share/qt4/mkspecs/features/warn_on.prf \
		/usr/share/qt4/mkspecs/features/qt.prf \
		/usr/share/qt4/mkspecs/features/unix/thread.prf \
		/usr/share/qt4/mkspecs/features/moc.prf \
		/usr/share/qt4/mkspecs/features/resources.prf \
		/usr/share/qt4/mkspecs/features/uic.prf \
		/usr/share/qt4/mkspecs/features/yacc.prf \
		/usr/share/qt4/mkspecs/features/lex.prf \
		/usr/share/qt4/mkspecs/features/include_source_dir.prf \
		noc-qt.pro
QMAKE_TARGET  = noc-qt
DESTDIR       = 
TARGET        = noc-qt

first: all
####### Implicit rules

.SUFFIXES: .o .c .cpp .cc .cxx .C

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.cc.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.cxx.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.C.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o "$@" "$<"

.c.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o "$@" "$<"

####### Build rules

all: Makefile $(TARGET)

$(TARGET): ui_mainwindow.h $(OBJECTS)  
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(OBJCOMP) $(LIBS)

Makefile: noc-qt.pro  /usr/share/qt4/mkspecs/linux-g++-64/qmake.conf /usr/share/qt4/mkspecs/common/unix.conf \
		/usr/share/qt4/mkspecs/common/linux.conf \
		/usr/share/qt4/mkspecs/common/gcc-base.conf \
		/usr/share/qt4/mkspecs/common/gcc-base-unix.conf \
		/usr/share/qt4/mkspecs/common/g++-base.conf \
		/usr/share/qt4/mkspecs/common/g++-unix.conf \
		/usr/share/qt4/mkspecs/qconfig.pri \
		/usr/share/qt4/mkspecs/modules/qt_webkit_version.pri \
		/usr/share/qt4/mkspecs/features/qt_functions.prf \
		/usr/share/qt4/mkspecs/features/qt_config.prf \
		/usr/share/qt4/mkspecs/features/exclusive_builds.prf \
		/usr/share/qt4/mkspecs/features/default_pre.prf \
		/usr/share/qt4/mkspecs/features/release.prf \
		/usr/share/qt4/mkspecs/features/default_post.prf \
		/usr/share/qt4/mkspecs/features/unix/gdb_dwarf_index.prf \
		/usr/share/qt4/mkspecs/features/warn_on.prf \
		/usr/share/qt4/mkspecs/features/qt.prf \
		/usr/share/qt4/mkspecs/features/unix/thread.prf \
		/usr/share/qt4/mkspecs/features/moc.prf \
		/usr/share/qt4/mkspecs/features/resources.prf \
		/usr/share/qt4/mkspecs/features/uic.prf \
		/usr/share/qt4/mkspecs/features/yacc.prf \
		/usr/share/qt4/mkspecs/features/lex.prf \
		/usr/share/qt4/mkspecs/features/include_source_dir.prf \
		/usr/lib/x86_64-linux-gnu/libQtGui.prl \
		/usr/lib/x86_64-linux-gnu/libQtCore.prl
	$(QMAKE) -o Makefile noc-qt.pro
/usr/share/qt4/mkspecs/common/unix.conf:
/usr/share/qt4/mkspecs/common/linux.conf:
/usr/share/qt4/mkspecs/common/gcc-base.conf:
/usr/share/qt4/mkspecs/common/gcc-base-unix.conf:
/usr/share/qt4/mkspecs/common/g++-base.conf:
/usr/share/qt4/mkspecs/common/g++-unix.conf:
/usr/share/qt4/mkspecs/qconfig.pri:
/usr/share/qt4/mkspecs/modules/qt_webkit_version.pri:
/usr/share/qt4/mkspecs/features/qt_functions.prf:
/usr/share/qt4/mkspecs/features/qt_config.prf:
/usr/share/qt4/mkspecs/features/exclusive_builds.prf:
/usr/share/qt4/mkspecs/features/default_pre.prf:
/usr/share/qt4/mkspecs/features/release.prf:
/usr/share/qt4/mkspecs/features/default_post.prf:
/usr/share/qt4/mkspecs/features/unix/gdb_dwarf_index.prf:
/usr/share/qt4/mkspecs/features/warn_on.prf:
/usr/share/qt4/mkspecs/features/qt.prf:
/usr/share/qt4/mkspecs/features/unix/thread.prf:
/usr/share/qt4/mkspecs/features/moc.prf:
/usr/share/qt4/mkspecs/features/resources.prf:
/usr/share/qt4/mkspecs/features/uic.prf:
/usr/share/qt4/mkspecs/features/yacc.prf:
/usr/share/qt4/mkspecs/features/lex.prf:
/usr/share/qt4/mkspecs/features/include_source_dir.prf:
/usr/lib/x86_64-linux-gnu/libQtGui.prl:
/usr/lib/x86_64-linux-gnu/libQtCore.prl:
qmake:  FORCE
	@$(QMAKE) -o Makefile noc-qt.pro

dist: 
	@$(CHK_DIR_EXISTS) .tmp/noc-qt1.0.0 || $(MKDIR) .tmp/noc-qt1.0.0 
	$(COPY_FILE) --parents $(SOURCES) $(DIST) .tmp/noc-qt1.0.0/ && $(COPY_FILE) --parents mainwindow.h ControlThread.hpp memory.hpp memorypacket.hpp mux.hpp noc.hpp paging.hpp processor.hpp SAX2Handler.hpp xmlFunctor.hpp tile.hpp tree.hpp .tmp/noc-qt1.0.0/ && $(COPY_FILE) --parents main.cpp mainwindow.cpp ControlThread.cpp memory.cpp memorypacket.cpp mux.cpp noc.cpp numberpage.cpp paging.cpp processor.cpp SAX2Handler.cpp xmlFunctor.cpp tile.cpp tree.cpp .tmp/noc-qt1.0.0/ && $(COPY_FILE) --parents mainwindow.ui .tmp/noc-qt1.0.0/ && (cd `dirname .tmp/noc-qt1.0.0` && $(TAR) noc-qt1.0.0.tar noc-qt1.0.0 && $(COMPRESS) noc-qt1.0.0.tar) && $(MOVE) `dirname .tmp/noc-qt1.0.0`/noc-qt1.0.0.tar.gz . && $(DEL_FILE) -r .tmp/noc-qt1.0.0


clean:compiler_clean 
	-$(DEL_FILE) $(OBJECTS)
	-$(DEL_FILE) *~ core *.core


####### Sub-libraries

distclean: clean
	-$(DEL_FILE) $(TARGET) 
	-$(DEL_FILE) Makefile


check: first

mocclean: compiler_moc_header_clean compiler_moc_source_clean

mocables: compiler_moc_header_make_all compiler_moc_source_make_all

compiler_moc_header_make_all: moc_mainwindow.cpp moc_ControlThread.cpp moc_processor.cpp
compiler_moc_header_clean:
	-$(DEL_FILE) moc_mainwindow.cpp moc_ControlThread.cpp moc_processor.cpp
moc_mainwindow.cpp: mainwindow.h
	/usr/bin/moc-qt4 $(DEFINES) $(INCPATH) mainwindow.h -o moc_mainwindow.cpp

moc_ControlThread.cpp: mainwindow.h \
		ControlThread.hpp
	/usr/bin/moc-qt4 $(DEFINES) $(INCPATH) ControlThread.hpp -o moc_ControlThread.cpp

moc_processor.cpp: mainwindow.h \
		ControlThread.hpp \
		memorypacket.hpp \
		mux.hpp \
		tile.hpp \
		memory.hpp \
		processor.hpp
	/usr/bin/moc-qt4 $(DEFINES) $(INCPATH) processor.hpp -o moc_processor.cpp

compiler_rcc_make_all:
compiler_rcc_clean:
compiler_image_collection_make_all: qmake_image_collection.cpp
compiler_image_collection_clean:
	-$(DEL_FILE) qmake_image_collection.cpp
compiler_moc_source_make_all:
compiler_moc_source_clean:
compiler_uic_make_all: ui_mainwindow.h
compiler_uic_clean:
	-$(DEL_FILE) ui_mainwindow.h
ui_mainwindow.h: mainwindow.ui
	/usr/bin/uic-qt4 mainwindow.ui -o ui_mainwindow.h

compiler_yacc_decl_make_all:
compiler_yacc_decl_clean:
compiler_yacc_impl_make_all:
compiler_yacc_impl_clean:
compiler_lex_make_all:
compiler_lex_clean:
compiler_clean: compiler_moc_header_clean compiler_uic_clean 

####### Compile

main.o: main.cpp mainwindow.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o main.o main.cpp

mainwindow.o: mainwindow.cpp mainwindow.h \
		ui_mainwindow.h \
		ControlThread.hpp \
		memorypacket.hpp \
		mux.hpp \
		noc.hpp \
		tile.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o mainwindow.o mainwindow.cpp

ControlThread.o: ControlThread.cpp mainwindow.h \
		ControlThread.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o ControlThread.o ControlThread.cpp

memory.o: memory.cpp tree.hpp \
		memorypacket.hpp \
		mux.hpp \
		memory.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o memory.o memory.cpp

memorypacket.o: memorypacket.cpp memorypacket.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o memorypacket.o memorypacket.cpp

mux.o: mux.cpp mainwindow.h \
		memorypacket.hpp \
		memory.hpp \
		ControlThread.hpp \
		tile.hpp \
		processor.hpp \
		mux.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o mux.o mux.cpp

noc.o: noc.cpp mainwindow.h \
		memory.hpp \
		ControlThread.hpp \
		memorypacket.hpp \
		mux.hpp \
		noc.hpp \
		tile.hpp \
		tree.hpp \
		processor.hpp \
		paging.hpp \
		xmlFunctor.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o noc.o noc.cpp

numberpage.o: numberpage.cpp 
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o numberpage.o numberpage.cpp

paging.o: paging.cpp memory.hpp \
		paging.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o paging.o paging.cpp

processor.o: processor.cpp mainwindow.h \
		ControlThread.hpp \
		memorypacket.hpp \
		mux.hpp \
		tile.hpp \
		memory.hpp \
		processor.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o processor.o processor.cpp

SAX2Handler.o: SAX2Handler.cpp memorypacket.hpp \
		mux.hpp \
		ControlThread.hpp \
		mainwindow.h \
		tile.hpp \
		processor.hpp \
		memory.hpp \
		xmlFunctor.hpp \
		SAX2Handler.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o SAX2Handler.o SAX2Handler.cpp

xmlFunctor.o: xmlFunctor.cpp mainwindow.h \
		ControlThread.hpp \
		memorypacket.hpp \
		mux.hpp \
		noc.hpp \
		memory.hpp \
		tile.hpp \
		processor.hpp \
		xmlFunctor.hpp \
		SAX2Handler.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o xmlFunctor.o xmlFunctor.cpp

tile.o: tile.cpp mainwindow.h \
		ControlThread.hpp \
		memorypacket.hpp \
		mux.hpp \
		memory.hpp \
		tile.hpp \
		processor.hpp \
		noc.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o tile.o tile.cpp

tree.o: tree.cpp mainwindow.h \
		ControlThread.hpp \
		memorypacket.hpp \
		mux.hpp \
		memory.hpp \
		tree.hpp \
		noc.hpp \
		tile.hpp \
		processor.hpp
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o tree.o tree.cpp

moc_mainwindow.o: moc_mainwindow.cpp 
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o moc_mainwindow.o moc_mainwindow.cpp

moc_ControlThread.o: moc_ControlThread.cpp 
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o moc_ControlThread.o moc_ControlThread.cpp

moc_processor.o: moc_processor.cpp 
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o moc_processor.o moc_processor.cpp

####### Install

install:   FORCE

uninstall:   FORCE

FORCE:

