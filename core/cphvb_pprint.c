/*
 * Copyright 2011 Simon A. F. Lund <safl@safl.dk>
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
 * along with cphVB. If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>
#include <cphvb.h>
#include <cphvb_pprint.h>

#define PPRINT_BUFSIZE 1024

static void operand_to_str( cphvb_array *op, char buf[] ) {

    char    stride[PPRINT_BUFSIZE]  = "",
            shape[PPRINT_BUFSIZE]   = "",
            tmp[PPRINT_BUFSIZE]    = "";

    for(int i=0; i< op->ndim; i++)
    {
        sprintf(tmp, "%d", (int)op->shape[i]);
        strcat(shape, tmp);
        if (i< op->ndim-1)
            strcat(shape, ",");

        sprintf(tmp, "%d", (int)op->stride[i]);
        strcat(stride, tmp);
        if (i< op->ndim-1)
            strcat(stride, ",");

        sprintf(buf, "%p { Base: %p Dims: %d Start: %d Shape: %s Stride: %s Type: %s Data: %p }",
                op, op->base, (int)op->ndim, (int)op->start, shape, stride, 
                cphvb_type_text(op->type), op->data
            );

    }

}

static void instr_to_str( cphvb_instruction *instr, char buf[] ) {

    int op_count = cphvb_operands(instr->opcode);
    char op_str[PPRINT_BUFSIZE];
    char tmp[PPRINT_BUFSIZE];

    sprintf(buf, "%s {\n", cphvb_opcode_text( instr->opcode) );
    for(int i=0; i < op_count; i++) {
        if (!cphvb_is_constant(instr->operand[i]))
            operand_to_str( instr->operand[i], op_str );
        else 
            sprintf(op_str, "CONSTANT");
        sprintf(tmp, "  OP%d %s\n", i, op_str);
        strcat(buf, tmp);
        if (!cphvb_is_constant(instr->operand[i]) && instr->operand[i]->base != NULL) {
            operand_to_str( instr->operand[i]->base, op_str );
            sprintf(tmp, "      %s\n", op_str);
            strcat(buf, tmp);
        }
    }
    if (instr->opcode == CPHVB_USERFUNC)
    {
        cphvb_userfunc* userfunc = instr->userfunc;
        for(int i=0; i < userfunc->nout; i++) {
            operand_to_str( userfunc->operand[i], op_str );
            sprintf(tmp, "  OUT%d %s\n", i, op_str);
            strcat(buf, tmp);
            if (userfunc->operand[i]->base != NULL) {
                operand_to_str( userfunc->operand[i]->base, op_str );
                sprintf(tmp, "      %s\n", op_str);
                strcat(buf, tmp);
            }
        }
        for(int i=userfunc->nout; i < userfunc->nout + userfunc->nin; i++) {
            operand_to_str( userfunc->operand[i], op_str );
            sprintf(tmp, "  IN%d %s\n", i, op_str);
            strcat(buf, tmp);
            if (userfunc->operand[i]->base != NULL) {
                operand_to_str( userfunc->operand[i]->base, op_str );
                sprintf(tmp, "      %s\n", op_str);
                strcat(buf, tmp);
            }
        }
    }
    strcat(buf, "}");

}

/* Pretty print an instruction.
 *
 * @instr  The instruction in question
 */
void cphvb_pprint_instr( cphvb_instruction *instr ) {

    char buf[PPRINT_BUFSIZE];
    instr_to_str( instr, buf );
    puts( buf );
}

/* Pretty print an array.
 *
 * @instr  The array in question
 */
void cphvb_pprint_array( cphvb_array *array ) {

    char buf[PPRINT_BUFSIZE];
    operand_to_str( array, buf );
    puts( buf );
}
