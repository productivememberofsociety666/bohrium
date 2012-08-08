// jit_ast.h


#ifndef __JIT_AST_H
#define __JIT_AST_H

#include "cphvb.h"
#include <utility>
#include <map>
#include <list>

typedef enum  { bin_op = 0, un_op = 1, const_val = 2, array_val = 3} ExprType;

typedef struct Exp {
    ExprType                                        tag;
    cphvb_intp                                      id;  
                                                   
    union { cphvb_constant*                         constant;
            cphvb_array*                            array;
                                
            struct {    cphvb_opcode    opcode;
                        struct Exp*     left;
                        struct Exp*     right; }    expression;
            
    } op;
} ast;


typedef struct Stm {
    cphvb_array* array;    
    Exp* exp;
} stm;




cphvb_error array_to_exp(cphvb_array *array, ast *result);

cphvb_error const_to_exp(cphvb_constant *constant, ast *result);

cphvb_error operand_to_exp(cphvb_instruction *inst, cphvb_intp operand_no, ast *result);

cphvb_error create_ast_from_instruction(cphvb_instruction *inst, ast *result);

char* expr_type_to_string(ExprType enumval);

cphvb_error print_ast_node(ast* node);


bool at_add(std::map<cphvb_array*,ast*>* assignments, cphvb_array* array, ast* ast);
ast* at_lookup(std::map<cphvb_array*,ast*>* assignments, cphvb_array* array);


void test_constant_to_string(void);
void ast_handle_instruction(std::list<ast*>* expression_list, std::map<cphvb_array*,ast*>* nametable, cphvb_instruction* instr);

#endif
