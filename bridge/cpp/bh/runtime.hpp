/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium team:
http://bohrium.bitbucket.org

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as 
published by the Free Software Foundation, either version 3 
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the 
GNU Lesser General Public License along with Bohrium. 

If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __BOHRIUM_BRIDGE_CPP_RUNTIME
#define __BOHRIUM_BRIDGE_CPP_RUNTIME
#include <iostream>
#include <boost/ptr_container/ptr_map.hpp>
#include "bh.h"

namespace bh {

typedef boost::ptr_map<int, bh_array> storage_type;
storage_type storage;
int keys = 0;

// Runtime : Definition

Runtime* Runtime::pInstance = 0;

Runtime* Runtime::instance()
{
    if (pInstance == 0) {
        pInstance = new Runtime;
    }
    return pInstance;
}

Runtime::Runtime()
{
    queue_size = 0;

    bh_error err;

    self_component = bh_component_setup(NULL);
    bh_component_children( self_component, &children_count, &components );

    if(children_count != 1 || components[0]->type != BH_VEM) {
        fprintf(stderr, "Error in the configuration: the bridge must "
                        "have exactly one child of type VEM\n");
        exit(-1);
    }
    vem_component   = components[0];

    vem_init        = vem_component->init;
    vem_execute     = vem_component->execute;
    vem_shutdown    = vem_component->shutdown;

    vem_reg_func    = vem_component->reg_func;
    free(components);

    err = vem_init(vem_component);
    if (err) {
        fprintf(stderr, "Error in vem_init()\n");
        exit(-1);
    }

}

Runtime::~Runtime()
{
    flush();
    vem_shutdown();
    bh_component_free(self_component);
    bh_component_free(vem_component);
}

/**
 * Flush the instruction-queue.
 * This will force the runtime system to execute the queued up instructions.
 *
 * @return The number of instructions flushed.
 */
bh_intp Runtime::flush()
{
    char *msg = (char*) malloc(1024 * sizeof(char));

    bh_intp cur_size = queue_size;
    bh_error res = BH_SUCCESS;
    if (queue_size > 0) {
        res = vem_execute( queue_size, queue );

        if (res != BH_SUCCESS) {
            sprintf(msg, "Error in scheduled batch of instructions: %s\n", bh_error_text(res));
            printf("%s", msg);
        }

        queue_size = 0;
    }
    return cur_size;
}

template <typename T>
inline
void Runtime::enqueue( bh_opcode opcode, multi_array<T> & op0, multi_array<T> & op1, multi_array<T> & op2)
{
    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }
    
    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = &storage[op1.getKey()];
    instr->operand[2] = &storage[op2.getKey()];

    if (op1.isTemp()) {
        delete &op1;
    }

    if (op2.isTemp()) {
        delete &op2;
    }
}

template <typename T>
inline
void Runtime::enqueue( bh_opcode opcode, multi_array<T> & op0, multi_array<T> & op1, T const& op2)
{
    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }

    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = &storage[op1.getKey()];
    instr->operand[2] = NULL;
    assign_const_type( &instr->constant, op2 );

    if (op1.isTemp()) {
        delete &op1;
    }
}

template <typename T>
inline
void Runtime::enqueue( bh_opcode opcode, multi_array<T> & op0, T const& op1, multi_array<T> & op2)
{
    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }

    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = NULL;
    instr->operand[2] = &storage[op2.getKey()];
    assign_const_type( &instr->constant, op1 );

    if (op2.isTemp()) {
        delete &op2;
    }
}

template <typename T>
inline
void Runtime::enqueue( bh_opcode opcode, multi_array<T> & op0, multi_array<T> & op1)
{
    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }

    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = &storage[op1.getKey()];
    instr->operand[2] = NULL;

    if (op1.isTemp()) {
        delete &op1;
    }
}

template <typename T>
inline
void Runtime::enqueue( bh_opcode opcode, multi_array<T> & op0, T const& op1)
{
    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }

    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = NULL;
    instr->operand[2] = NULL;
    assign_const_type( &instr->constant, op1 );
}

template <typename T>
inline
void Runtime::enqueue( bh_opcode opcode, multi_array<T> & op0)
{
    bh_instruction* instr;

    if (queue_size >= BH_CPP_QUEUE_MAX) {
        vem_execute( queue_size, queue );
        queue_size = 0;
    }

    instr = &queue[queue_size++];
    instr->opcode = opcode;
    instr->operand[0] = &storage[op0.getKey()];
    instr->operand[1] = NULL;
    instr->operand[2] = NULL;
}

}
#endif
