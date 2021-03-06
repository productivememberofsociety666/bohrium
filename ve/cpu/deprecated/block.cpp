#include "block.hpp"

using namespace std;
namespace bohrium{
namespace core{

const char Block::TAG[] = "Block";

Block::Block(SymbolTable& symbol_table, const bh_ir& ir, size_t dag_idx)
: instr_(NULL), operands_(NULL), noperands_(0), omask_(0), tacs(NULL), ntacs_(0), ir(ir), dag(ir.dag_list[dag_idx]), symbol_table(symbol_table)
{
    if (ps<1) {
        fprintf(stderr, "This block is the empty program! You should not have called this!");
    }

    
}

Block::~Block()
{
}

string Block::scope_text(string prefix) const
{
    stringstream ss;
    ss << prefix << "scope {" << endl;
    for(size_t i=1; i<=noperands(); ++i) {
        ss << prefix;
        ss << "[" << i << "] {";
        ss << core::operand_text(scope(i));
        ss << "}";

        ss << endl;
    }
    ss << prefix << "}" << endl;

    return ss.str();
}

string Block::scope_text() const
{
    return scope_text("");
}

string Block::text(std::string prefix) const
{
    stringstream ss;
    ss << prefix;
    ss << "block(";
    ss << "length="       << ntacs_;
    ss << ", noperands="  << noperands();
    ss << ", omask="      << omask_;
    ss << ") {"           << endl;
    ss << prefix << "  symbol(" << symbol() << ")" << endl;
    ss << prefix << "  symbol_text(" << symbol_text() << ")" << endl;

    ss << prefix << "  tacs {" << endl;
    for(size_t i=0; i<ntacs_; ++i) {
        ss << prefix << "    [" << i << "]" << core::tac_text(tacs[i]) << endl;
    }
    ss << prefix << "  }" << endl;

    ss << scope_text(prefix+"  ");
    ss << prefix << "}";
    
    return ss.str();
}

string Block::text() const
{
    return text("");
}

bool Block::symbolize()
{   
    bool symbolize_res = symbolize(0, ntacs_-1);
    return symbolize_res;
}

bool Block::symbolize(size_t tac_start, size_t tac_end)
{
    stringstream tacs,
                 operands;

    //
    // Scope
    for(size_t i=1; i<=noperands(); ++i) {
        const operand_t& operand = scope(i);

        operands << "~" << i;
        operands << core::layout_text_shand(operand.layout);
        operands << core::etype_text_shand(operand.etype);
    }

    //
    // Program
    bool first = true;
    for (size_t i=tac_start; i<=tac_end; ++i) {
        tac_t& tac = this->tacs[i];
       
        // Do not include system opcodes in the kernel symbol.
        if ((tac.op == SYSTEM) || (tac.op == EXTENSION)) {
            continue;
        }
        if (!first) {   // Separate op+oper with "_"
            tacs  << "_";
        }
        first = false;

        tacs << core::operation_text(tac.op);
        tacs << "-" << core::operator_text(tac.oper);
        tacs << "-";
        size_t ndim = (tac.op == REDUCE) ? symbol_table.table[tac.in1].ndim : symbol_table.table[tac.out].ndim;
        if (ndim <= 3) {
            tacs << ndim;
        } else {
            tacs << "N";
        }
        tacs << "D";
        
        switch(core::tac_noperands(tac)) {
            case 3:
                tacs << "_" << resolve(tac.out);
                tacs << "_" << resolve(tac.in1);
                tacs << "_" << resolve(tac.in2);
                break;

            case 2:
                tacs << "_" << resolve(tac.out);
                tacs << "_" << resolve(tac.in1);
                break;

            case 1:
                tacs << "_" << resolve(tac.out);
                break;

            case 0:
                break;

            default:
                fprintf(stderr, "Something horrible happened...\n");
        }
    }

    symbol_text_    = tacs.str() +"_"+ operands.str();
    symbol_         = core::hash_text(symbol_text_);

    return true;
}

uint32_t Block::omask(void) const
{
    return omask_;
}

size_t Block::noperands(void) const
{
    return noperands_;
}

size_t Block::add_operand(bh_instruction& instr, size_t operand_idx)
{
    //
    // Map operands through the SymbolTable
    size_t arg_symbol = symbol_table.map_operand(instr, operand_idx);

    //
    // Map operands into block-scope.
    size_t arg_idx = ++(noperands_);

    //
    // Reuse operand identifiers: Detect if we have seen it before and reuse the name.
    // This is done by comparing the currently investigated operand (arg_idx)
    // with all other operands in the current scope [1,arg_idx[
    // Do remember that 0 is is not a valid operand and we therefore index from 1.
    // Also we do not want to compare with selv, that is when i == arg_idx.
    for(size_t i=1; i<arg_idx; ++i) {
        if (!core::equivalent(scope(i), symbol_table.table[arg_symbol])) {
            continue; // Not equivalent, continue search.
        }
        // Found one! Use it instead of the incremented identifier.
        --noperands_;
        arg_idx = i;
        break;
    }

    //
    // Point to the operand in the symbol_table
    operands_[arg_idx] = &symbol_table.table[arg_symbol];

    //
    // Insert entry such that tac operands can be resolved in block-scope.
    operand_map.insert(pair<size_t,size_t>(arg_symbol, arg_idx));

    return arg_symbol;
}

const operand_t& Block::scope(size_t operand_idx) const
{
    return *operands_[operand_idx];
}

size_t Block::resolve(size_t symbol_idx) const
{
    return operand_map.find(symbol_idx)->second;
}

tac_t& Block::program(size_t pc) const
{
    return tacs[pc];
}

size_t Block::size(void) const
{
    return ntacs_;
}

string Block::symbol(void) const
{
    return symbol_;
}

string Block::symbol_text(void) const
{
    return symbol_text_;
}

operand_t** Block::operands(void) const
{
    return operands_;
}

}}
