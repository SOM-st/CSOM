/*
 * $Id: main.c 792 2009-04-06 08:07:33Z michael.haupt $
 *
Copyright (c) 2007 Michael Haupt, Tobias Pape
Software Architecture Group, Hasso Plattner Institute, Potsdam, Germany
http://www.hpi.uni-potsdam.de/swa/

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
  */

#include <emscripten.h>
#include <memory/gc.h>

#include <vm/Universe.h>

int main(int argc, char** argv) {
    printf("This is CSOM.\n");
    EM_ASM(
        FS.mkdir('/Smalltalk');
        FS.mount(NODEFS, {
			root: './core-lib/Smalltalk'
		}, '/Smalltalk');
        FS.mkdir('/TestSuite');
        FS.mount(NODEFS, {
			root: './core-lib/TestSuite'
		}, '/TestSuite');
    );
    
    int vm_argc = 0;    
    const char** vm_argv =
        Universe_handle_arguments(&vm_argc, argc, (const char**)argv);
    Universe_initialize(vm_argc, (const char**)vm_argv);
    
    FREE_ARRAY_ELEMENTS((char**)vm_argv, vm_argc);  // the array itself is still part of argv, i.e., on the c-stack
    
    Universe_exit(ERR_SUCCESS);
}


