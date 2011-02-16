/*
 * Copyright 2011 Mads R. B. Kristensen <madsbk@gmail.com>
 *
 * This file is part of cphVB.
 *
 * cphVB is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cphVB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cphVB.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <cphvb_vem.h>
#include <cphvb.h>
#include "private.h"

/* Initialize the VEM
 *
 * @return Error codes (CPHVB_SUCCESS)
 */
cphvb_error cphvb_vem_init(void)
{
    return CPHVB_SUCCESS;
}


/* Shutdown the VEM, which include a instruction flush
 *
 * @return Error codes (CPHVB_SUCCESS)
 */
cphvb_error cphvb_vem_shutdown(void)
{
    return CPHVB_SUCCESS;
}


/* Create an array, which are handled by the VEM.
 *
 * @base Pointer to the base array. If NULL this is a base array
 * @type The type of data in the array
 * @ndim Number of dimentions
 * @start Index of the start element (always 0 for base-array)
 * @shape[CPHVB_MAXDIM] Number of elements in each dimention
 * @stride[CPHVB_MAXDIM] The stride for each dimention
 * @has_init_value Does the array have an initial value
 * @init_value The initial value
 * @new_array The handler for the newly created array
 * @return Error code (CPHVB_SUCCESS, CPHVB_OUT_OF_MEMORY)
 */
cphvb_error cphvb_vem_create_array(cphvb_array*   base,
                                   cphvb_type     type,
                                   cphvb_intp     ndim,
                                   cphvb_index    start,
                                   cphvb_index    shape[CPHVB_MAXDIM],
                                   cphvb_index    stride[CPHVB_MAXDIM],
                                   cphvb_intp     has_init_value,
                                   cphvb_constant init_value,
                                   cphvb_array**  new_array)
{
    cphvb_array *array    = malloc(sizeof(cphvb_array));
    if(array == NULL)
        return CPHVB_OUT_OF_MEMORY;

    array->owner          = CPHVB_BRIDGE;
    array->base           = base;
    array->type           = type;
    array->ndim           = ndim;
    array->start          = start;
    array->has_init_value = has_init_value;
    array->init_value     = init_value;
    array->data           = NULL;
    array->ref_count      = 1;
    memcpy(array->shape, shape, ndim * sizeof(cphvb_index));
    memcpy(array->stride, stride, ndim * sizeof(cphvb_index));

    if(array->base != NULL)
    {
        assert(!has_init_value);
        ++array->base->ref_count;
        array->data = array->base->data;
    }

    *new_array = array;
    return CPHVB_SUCCESS;
}


/* Check whether the instruction is supported by the VEM or not
 *
 * @return non-zero when true and zero when false
 */
cphvb_intp cphvb_vem_instruction_check(cphvb_instruction *inst)
{
    switch(inst->opcode)
    {
    case CPHVB_DESTORY:
        return 1;
    default:
        return 0;
    }
}


/* Execute a list of instructions (blocking, for the time being).
 * It is required that the VEM supports all instructions in the list.
 *
 * @instruction A list of instructions to execute
 * @return Error codes (CPHVB_SUCCESS)
 */
cphvb_error cphvb_vem_execute(cphvb_intp count,
                              cphvb_instruction inst_list[])
{
    cphvb_intp i;
    for(i=0; i<count; ++i)
    {
        cphvb_instruction *inst = &inst_list[i];
        switch(inst->opcode)
        {
        case CPHVB_DESTORY:
            printf("EXEC: CPHVB_DESTORY\n");
            if(inst->operand[0]->base == NULL)
            {
                if(--inst->operand[0]->ref_count <= 0)
                {
                    if(inst->operand[0]->data != NULL)
                        free(inst->operand[0]->data);
                    free(inst->operand[0]);
                }
            }
            else
            {
                if(--inst->operand[0]->base->ref_count <= 0)
                {
                    if(inst->operand[0]->base->data != NULL)
                        free(inst->operand[0]->base->data);
                    free(inst->operand[0]->base);
                    free(inst->operand[0]);
                }
            }
            break;
        default:
            fprintf(stderr, "cphvb_vem_execute() encountered an not "
                            "supported instruction opcode\n");
            exit(CPHVB_INST_NOT_SUPPORTED);
        }

    }

    return CPHVB_SUCCESS;
}


