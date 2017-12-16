/*
 * $Id: Interpreter.c 792 2009-04-06 08:07:33Z michael.haupt $
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
 
#include "Interpreter.h"
#include "bytecodes.h"

#include <memory/gc.h>

#include <vm/Universe.h>

#include <vmobjects/Signature.h>
#include <vmobjects/VMInvokable.h>

#include <compiler/Disassembler.h>

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


// class variable.
static pVMFrame frame;

// convenience macros for frequently used function invocations
#define _FRAME Interpreter_get_frame()
#define _SETFRAME(f) Interpreter_set_frame(f)
#define _METHOD Interpreter_get_method()
#define _SELF Interpreter_get_self()

#pragma mark private bytecode handlers.

pVMFrame pop_frame(void) {
    // Save a reference to the top frame
    pVMFrame result = _FRAME;
    
    // Pop the top frame from the frame stack
    _SETFRAME(SEND(_FRAME, get_previous_frame));

    // Destroy the previous pointer on the old top frame
    SEND(result, clear_previous_frame);

    // Return the popped frame
    return result;
}


void pop_frame_and_push_result(pVMObject result) {
    // Pop the top frame from the interpreter frame stack and compute the number
    // of arguments
    pVMFrame prev_frame = pop_frame();
    pVMMethod method = SEND(prev_frame, get_method);
    int number_of_arguments = SEND(method, get_number_of_arguments);
        
    // Pop the arguments
    for(int i = 0; i < number_of_arguments; i++)
        SEND(_FRAME, pop);
    
    // Push the result
    SEND(_FRAME, push, result);
}


void sendXXX(pVMSymbol signature, pVMClass receiver_class) {
    // Lookup the invokable with the given signature
    pVMObject invokable = (pVMObject)SEND(receiver_class,
                                          lookup_invokable, signature);

    if(invokable != NULL)
        // Invoke the invokable in the current frame
        TSEND(VMInvokable, invokable, invoke, _FRAME);           
    else { 
        // doesNotUnderstand
        // Compute the number of arguments
        int number_of_arguments = Signature_get_number_of_arguments(signature);
        
        // Compute the receiver
        pVMObject receiver = 
            SEND(_FRAME, get_stack_element, number_of_arguments - 1);
        
        // Allocate an array with enough room to hold all arguments
        pVMArray arguments_array = Universe_new_array(number_of_arguments);
      
        // Remove all arguments and put them in the freshly allocated array
        for(int i = number_of_arguments - 1; i >= 0; i--) {
            pVMObject o = SEND(_FRAME, pop);
            SEND(arguments_array, set_indexable_field, i, o);
        }

        // Send 'doesNotUnderstand:arguments:' to the receiver object
        pVMObject arguments[] =
            { (pVMObject)signature, (pVMObject)arguments_array };
        SEND(receiver, send, doesNotUnderstand_sym, arguments, 2);
    }
} 

#pragma mark  Bytecode handling functions

void do_dup(void) {
    // Handle the dup bytecode
    pVMObject elem = SEND(_FRAME, get_stack_element, 0);
    SEND(_FRAME, push, elem);
}


void do_push_local(int bytecode_index) {
    // Handle the push local bytecode
    pVMMethod method = _METHOD;
    uint8_t bc1 = SEND(method, get_bytecode, bytecode_index + 1);
    uint8_t bc2 = SEND(method, get_bytecode, bytecode_index + 2);
    
    pVMObject local = SEND(_FRAME, get_local, bc1, bc2);
    
    SEND(_FRAME, push, local);
}


void do_push_argument(int bytecode_index) {
    // Handle the push argument bytecode
    pVMMethod method = _METHOD;
    uint8_t bc1 = SEND(method, get_bytecode, bytecode_index + 1);
    uint8_t bc2 = SEND(method, get_bytecode, bytecode_index + 2);
    
    pVMObject argument = SEND(_FRAME, get_argument, bc1, bc2);
    
    SEND(_FRAME, push, argument);
}


void do_push_field(int bytecode_index) {
    pVMMethod method = _METHOD;
    // Handle the push field bytecode
    pVMSymbol field_name =
        (pVMSymbol)SEND(method, get_constant, bytecode_index);
    
    // Get the field index from the field name
    pVMObject self = _SELF;
    int field_index = SEND(self, get_field_index, field_name);
    
    pVMObject o = SEND(self, get_field, field_index);
    // Push the field with the computed index onto the stack
    SEND(_FRAME, push, o);
}


void do_push_block(int bytecode_index) {
    pVMMethod method = _METHOD;
    // Handle the push block bytecode
    pVMMethod block_method = (pVMMethod)SEND(method, 
                                              get_constant, bytecode_index);
        
    int number_of_arguments = SEND(block_method, get_number_of_arguments);
    // Push a new block with the current get_frame() as context onto the stack
    SEND(_FRAME, push,
         (pVMObject) Universe_new_block(block_method,
                                        _FRAME,
                                        number_of_arguments));
}


void do_push_constant(int bytecode_index) {
    pVMMethod method = _METHOD;
    
    // Handle the push constant bytecode
    pVMObject constant = SEND(method, get_constant, bytecode_index);
    SEND(_FRAME, push, constant);
}


void do_push_global(int bytecode_index) {
    pVMMethod method = _METHOD;
    // Handle the push global bytecode
    pVMSymbol global_name = (pVMSymbol)SEND(method,
                                            get_constant, bytecode_index);
        
    // Get the global from the Universe
    pVMObject global = Universe_get_global(global_name);
        
    if(global != NULL)
        // Push the global onto the stack
        SEND(_FRAME, push, global);
    else {
        // Send 'unknownGlobal:' to self
        pVMObject arguments[] = { (pVMObject)global_name };
        pVMObject self =_SELF;
        SEND(self, send, unknownGlobal_sym, arguments, 1);
    }
}


void do_pop(void) {
    // Handle the pop bytecode
    SEND(_FRAME, pop);
}


void do_pop_local(int bytecode_index) {
    pVMMethod method = _METHOD;
    // Handle the pop local bytecode
    uint8_t bc1 = SEND(method, get_bytecode, bytecode_index + 1);
    uint8_t bc2 = SEND(method, get_bytecode, bytecode_index + 2);
   
    pVMObject o =  SEND(_FRAME, pop);
    
    SEND(_FRAME, set_local, bc1, bc2, o);
}


void do_pop_argument(int bytecode_index) {
    pVMMethod method = _METHOD;
    // Handle the pop argument bytecode
    uint8_t bc1 = SEND(method, get_bytecode, bytecode_index + 1);
    uint8_t bc2 = SEND(method, get_bytecode, bytecode_index + 2);

    pVMObject o =  SEND(_FRAME, pop);
    SEND(_FRAME, set_argument, bc1, bc2, o);
}


void do_pop_field(int bytecode_index) {
    pVMMethod method = _METHOD;
    // Handle the pop field bytecode
    pVMSymbol field_name = (pVMSymbol)SEND(method, 
                                           get_constant, bytecode_index);
                
    // Get the field index from the field name
    pVMObject self = _SELF;
    int field_index = SEND(self, get_field_index, field_name);
    
    // Set the field with the computed index to the value popped from the stack
    pVMObject o = SEND(_FRAME, pop);
    SEND(self, set_field, field_index, o);
}


void do_send(int bytecode_index) {
    pVMMethod method = _METHOD;
    // Handle the send bytecode
    pVMSymbol signature = (pVMSymbol)SEND(method, 
                                          get_constant, bytecode_index);
    
    // Get the number of arguments from the signature
    int number_of_arguments = Signature_get_number_of_arguments(signature);
    
    // Get the receiver from the stack
    pVMObject receiver =
        SEND(_FRAME, get_stack_element, number_of_arguments - 1);

    // Send the message
    sendXXX(signature, SEND(receiver, get_class));
}


void do_super_send(int bytecode_index) {
    pVMMethod method = _METHOD;
    // Handle the super send bytecode
    pVMSymbol signature = (pVMSymbol)SEND(method, 
                                           get_constant, bytecode_index);
    
    // Send the message
    // Lookup the invokable with the given signature
    // (take care of blocks: block methods are not members of the surrounding
    // class, so their context must be resolved first)    
    pVMFrame ctxt = SEND(_FRAME, get_outer_context);
    pVMMethod real_method = SEND(ctxt, get_method);
    pVMClass holder = TSEND(VMInvokable, real_method, get_holder);
    pVMClass super = SEND(holder, get_super_class);
    pVMObject invokable = (pVMObject)SEND(super, lookup_invokable, signature); 
    
    if(invokable != NULL)
      // Invoke the invokable in the current frame
      TSEND(VMInvokable, invokable, invoke, _FRAME);
    else {
      // Compute the number of arguments
      int number_of_arguments = Signature_get_number_of_arguments(signature);
    
      // Compute the receiver
      pVMObject receiver = SEND(_FRAME, 
                                get_stack_element, number_of_arguments - 1);
    
      // Allocate an array with enough room to hold all arguments
      pVMArray arguments_array = Universe_new_array(number_of_arguments);
    
      // Remove all arguments and put them in the freshly allocated array
      for(int i = number_of_arguments - 1; i >= 0; i--) {
          pVMObject o = SEND(_FRAME, pop);
          SEND(arguments_array, set_indexable_field, i, o);
      }
    
      // Send 'doesNotUnderstand:arguments:' to the receiver object
      pVMObject arguments[] =
        { (pVMObject)signature, (pVMObject)arguments_array };
      SEND(receiver, send,  doesNotUnderstand_sym, arguments, 2);
    }
}


void do_return_local() {
    // Handle the return local bytecode
    pVMObject result = SEND(_FRAME, pop);
                    
    // Pop the top frame and push the result
    pop_frame_and_push_result(result);
}


void do_return_non_local() {
    // Handle the return non local bytecode
    pVMObject result = SEND(_FRAME, pop);
                    
    // Compute the context for the non-local return
    pVMFrame context = SEND(_FRAME, get_outer_context);

    // Make sure the block context is still on the stack
    if(!SEND(context, has_previous_frame)) {
        // Try to recover by sending 'escapedBlock:' to the sending object
        // this can get a bit nasty when using nested blocks. In this case
        // the "sender" will be the surrounding block and not the object that
        // acutally sent the 'value' message.
        pVMBlock block = (pVMBlock)SEND(_FRAME, get_argument, 0, 0);
        pVMFrame prev_frame = SEND(_FRAME, get_previous_frame);
        pVMFrame outer_context =SEND(prev_frame, get_outer_context);
        pVMObject sender = SEND(outer_context, get_argument, 0, 0);
        pVMObject arguments[] = { (pVMObject)block };

        // pop the frame of the currently executing block...
        pop_frame();

        // ... and execute the escapedBlock message instead
       SEND(sender, send, escapedBlock_sym, arguments, 1);

       return;
    }

    // Unwind the frames
    while(_FRAME != context) {
        pop_frame();
    }

    // Pop the top frame and push the result
    pop_frame_and_push_result(result);
}


#pragma mark extern callable Interpreter funtions

void Interpreter_initialize(pVMObject nilObject) {
    _SETFRAME((pVMFrame) nilObject);
}

void Interpreter_start(void) {
    // iterate over the bytecodes
    while(true) {
        // get the current bytecode index
        int bytecode_index = SEND(_FRAME, get_bytecode_index);
        // get the current bytecode
        pVMMethod method = Interpreter_get_method();
        uint8_t bytecode = SEND(method, get_bytecode, bytecode_index);
        // get the length of the current bytecode
        int bytecode_length = bytecodes_get_bytecode_length(bytecode);
        
        // trace, if wanted (means dump_bytecodes is at least 2)
        if(dump_bytecodes >1)
            Disassembler_dump_bytecode(_FRAME, method, bytecode_index);
        
        // compute the next bytecode index
        int next_bytecode_index = bytecode_index + bytecode_length;
        // update the bytecode index of the frame
        SEND(_FRAME, set_bytecode_index, next_bytecode_index);
        
        // Handle the current bytecode
        switch(bytecode) {
            case BC_HALT:             return; // handle the halt bytecode
            case BC_DUP:              do_dup();  break;
            case BC_PUSH_LOCAL:       do_push_local(bytecode_index); break;
            case BC_PUSH_ARGUMENT:    do_push_argument(bytecode_index); break;
            case BC_PUSH_FIELD:       do_push_field(bytecode_index); break;
            case BC_PUSH_BLOCK:       do_push_block(bytecode_index); break;
            case BC_PUSH_CONSTANT:    do_push_constant(bytecode_index); break;
            case BC_PUSH_GLOBAL:      do_push_global(bytecode_index); break;
            case BC_POP:              do_pop(); break;
            case BC_POP_LOCAL:        do_pop_local(bytecode_index); break;
            case BC_POP_ARGUMENT:     do_pop_argument(bytecode_index); break;
            case BC_POP_FIELD:        do_pop_field(bytecode_index); break;
            case BC_SEND:             do_send(bytecode_index); break;
            case BC_SUPER_SEND:       do_super_send(bytecode_index); break;
            case BC_RETURN_LOCAL:     do_return_local(); break;
            case BC_RETURN_NON_LOCAL: do_return_non_local(); break;
            default:                  Universe_error_exit(
                                            "Interpreter: Unexpected bytecode");      
        } // switch
    } // while
}


pVMFrame Interpreter_push_new_frame(pVMMethod method, pVMFrame context) {
    _SETFRAME(Universe_new_frame(_FRAME, method, context));
    return _FRAME;
}


void Interpreter_set_frame(pVMFrame _frame) {
    frame = _frame;
}


pVMFrame Interpreter_get_frame(void) {
    return frame;
}


pVMMethod Interpreter_get_method(void) {
    return SEND(_FRAME, get_method);
}


pVMObject Interpreter_get_self(void) {
    pVMFrame context = SEND(_FRAME, get_outer_context);
    return SEND(context, get_argument, 0, 0);
}
