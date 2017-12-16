#!/usr/bin/env make -f

#
#  Makefile by Tobias Pape
#
# Copyright (c) 2007 Michael Haupt, Tobias Pape
# Software Architecture Group, Hasso Plattner Institute, Potsdam, Germany
# http://www.hpi.uni-potsdam.de/swa/
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

## we need c99!

ifeq ($(CC),)
	CC		= gcc
endif

CFLAGS		=-O3 -m32 -Wno-endif-labels -std=gnu99 $(DBG_FLAGS) $(INCLUDES)
LDFLAGS		=-O3 -m32 $(LIBRARIES)

INSTALL		=install

CSOM_LIBS	=-ldl
CORE_LIBS	=-lm

CSOM_NAME	=CSOM
SOM_NAME	=SOM
CORE_NAME	=$(SOM_NAME)Core

############ global stuff -- overridden by ../Makefile

PREFIX		?= /usr/local

ROOT_DIR	?= $(PWD)/..
SRC_DIR		?= $(ROOT_DIR)/src
BUILD_DIR   ?= $(ROOT_DIR)/build

DEST_SHARE	?= $(PREFIX)/share/$(SOM_NAME)
DEST_BIN	?= $(PREFIX)/bin
DEST_LIB	?= $(PREFIX)/lib/$(SOM_NAME)


ST_DIR		?= $(ROOT_DIR)/Smalltalk
EX_DIR		?= $(ROOT_DIR)/Examples
TEST_DIR	?= $(ROOT_DIR)/TestSuite

############# "component" directories


COMPILER_DIR 	= $(SRC_DIR)/compiler
INTERPRETER_DIR = $(SRC_DIR)/interpreter
MEMORY_DIR 		= $(SRC_DIR)/memory
MISC_DIR 		= $(SRC_DIR)/misc
VM_DIR 			= $(SRC_DIR)/vm
VMOBJECTS_DIR 	= $(SRC_DIR)/vmobjects

COMPILER_SRC	= $(wildcard $(COMPILER_DIR)/*.c)
COMPILER_OBJ	= $(COMPILER_SRC:.c=.o)
INTERPRETER_SRC	= $(wildcard $(INTERPRETER_DIR)/*.c)
INTERPRETER_OBJ	= $(INTERPRETER_SRC:.c=.o)
MEMORY_SRC		= $(wildcard $(MEMORY_DIR)/*.c)
MEMORY_OBJ		= $(MEMORY_SRC:.c=.o)
MISC_SRC		= $(wildcard $(MISC_DIR)/*.c)
MISC_OBJ		= $(MISC_SRC:.c=.o)
VM_SRC			= $(wildcard $(VM_DIR)/*.c) $(SRC_DIR)/main.c
VM_OBJ			= $(VM_SRC:.c=.o)
VMOBJECTS_SRC	= $(wildcard $(VMOBJECTS_DIR)/*.c)
VMOBJECTS_OBJ	= $(VMOBJECTS_SRC:.c=.o)

############# primitives location etc.

PRIMITIVES_DIR	= $(SRC_DIR)/primitives
PRIMITIVES_SRC	= $(wildcard $(PRIMITIVES_DIR)/*.c)
PRIMITIVES_OBJ	= $(PRIMITIVES_SRC:.c=.pic.o)

############# include path

INCLUDES		=-I$(SRC_DIR)
LIBRARIES       =

##############
############## Collections.

CSOM_OBJ		=  $(MEMORY_OBJ) $(MISC_OBJ) $(VMOBJECTS_OBJ) \
					$(COMPILER_OBJ) $(INTERPRETER_OBJ) $(VM_OBJ) 
OBJECTS			= $(CSOM_OBJ) $(PRIMITIVES_OBJ)

SOURCES			=  $(COMPILER_SRC) $(INTERPRETER_SRC) $(MEMORY_SRC) \
					$(MISC_SRC) $(VM_SRC) $(VMOBJECTS_SRC)  \
					$(PRIMITIVES_SRC)

############# Things to clean

CLEAN			= $(OBJECTS) CORE

############# Tools

OSTOOL			= $(BUILD_DIR)/ostool.exe

#
#
#
#  metarules
#

.SUFFIXES: .pic.o

.PHONY: clean clobber test

all: $(OSTOOL) $(SRC_DIR)/platform.h $(SRC_DIR)/CSOM CORE


debug : DBG_FLAGS=-DDEBUG -g
debug: all

profiling : DBG_FLAGS=-g -pg
profiling : LDFLAGS+=-pg
profiling: all


.c.pic.o:
	$(CC) $(CFLAGS) -fPIC -g -c $< -o $*.pic.o


clean:
	rm -Rf $(CLEAN)


clobber: $(OSTOOL) clean
	rm -f `$(OSTOOL) x "$(CSOM_NAME)"` $(ST_DIR)/`$(OSTOOL) s "$(CORE_NAME)"`
	rm -f $(OSTOOL)
	rm -f $(SRC_DIR)/platform.h

$(OSTOOL): $(BUILD_DIR)/ostool.c
	cc -g -Wno-endif-labels -o $(OSTOOL) $(BUILD_DIR)/ostool.c

$(SRC_DIR)/platform.h: $(OSTOOL)
	@($(OSTOOL) i >$(SRC_DIR)/platform.h)

#
#
#
# product rules
#


$(SRC_DIR)/CSOM: $(CSOM_OBJ)
	@echo Linking CSOM
	$(CC) -s MAIN_MODULE=1 -s WASM=1 --pre-js pre-benchmarks.js $(LDFLAGS) `$(OSTOOL) l`\
		-o `$(OSTOOL) x "$(CSOM_NAME)"` \
		$(CSOM_OBJ) $(CSOM_LIBS) 
	@echo CSOM done.

CORE: $(SRC_DIR)/CSOM $(PRIMITIVES_OBJ)
	@echo Linking SOMCore lib
	$(CC) -s SIDE_MODULE=1 -s WASM=1 $(LDFLAGS) `$(OSTOOL) l "$(CORE_NAME)"` \
		-o `$(OSTOOL) s "$(CORE_NAME)"`\
		$(PRIMITIVES_OBJ) $(CORE_LIBS)
	mv "$(CORE_NAME).wasm" $(ST_DIR)
	@touch CORE
	@echo SOMCore done.

install: all
	@echo installing CSOM into build
	$(INSTALL) -d $(DEST_SHARE) $(DEST_BIN) $(DEST_LIB)
	$(INSTALL) `$(OSTOOL) x "$(CSOM_NAME)"` $(DEST_BIN)
	@echo CSOM.
	cp -R $(ST_DIR) $(DEST_LIB)
	@echo Library.
	cp -R $(EX_DIR) $(TEST_DIR) $(DEST_SHARE)
	@echo shared components.
	@echo done.
	
uninstall:
	@echo removing Library and shared Components
	rm -Rf $(DEST_SHARE) $(DEST_LIB)
	@echo removing CSOM
	rm -Rf $(DEST_BIN)/`$(OSTOOL) x "$(CSOM_NAME)"`
	@echo done.

#
# test: run the standard test suite
#
test: all
	@(./CSOM -cp Smalltalk TestSuite/TestHarness.som;)

#
# bench: run the benchmarks
#
bench: all
	@(./CSOM -cp Smalltalk Examples/Benchmarks/All.som;)

bench-gc: all
	@(./CSOM -g -cp Smalltalk Examples/Benchmarks/All.som;)

ddbench: all
	@(./CSOM -d -d -cp Smalltalk Examples/Benchmarks/All.som;)
