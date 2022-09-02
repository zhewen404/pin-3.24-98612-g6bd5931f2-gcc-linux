/*
 * Copyright 2002-2019 Intel Corporation.
 * 
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 * 
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include "pin.H"
using std::cerr;
using std::ofstream;
using std::ios;
using std::string;
using std::endl;

ofstream OutFile;
ofstream OutPCFile;
ofstream OutputdebugFile;
typedef UINT64 COUNTER;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount
static UINT64 icount = 0;
static UINT64 bbcount = 0;
static COUNTER bb_num = 1;
static COUNTER length = 0;

class BBLSTATS
{
  public:
    COUNTER _num;
    COUNTER _counter;
    const ADDRINT _addr;
    COUNTER _num_inst;

  public:
    BBLSTATS(ADDRINT addr, COUNTER num, COUNTER num_inst) :
        _num(num), _counter(0),_addr(addr), _num_inst(num_inst) {};

};

typedef std::map<ADDRINT, BBLSTATS*> bb_t;
typedef std::vector<bb_t*> bbl_t;
// bb_t *bb_table = new bb_t;
bbl_t bb_table_list;

typedef std::map<ADDRINT, COUNTER> bb_num_t;
bb_num_t bb_num_map;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "bbv.out", "specify output file name");
KNOB<string> KnobOutputPCFile(KNOB_MODE_WRITEONCE, "pintool",
    "p", "pc.out", "specify output PC file name");
KNOB<UINT64> KnobIntervalSize(KNOB_MODE_WRITEONCE, "pintool", 
    "s","100000000", "interval size");
KNOB<UINT64> KnobBB(KNOB_MODE_WRITEONCE, "pintool", 
    "n","1", "bbnum debug print");
KNOB<string> KnobOutputdebugFile(KNOB_MODE_WRITEONCE, "pintool",
    "f", "debug.out", "specify output debug file name");
KNOB<BOOL>   KnobDebug(KNOB_MODE_WRITEONCE, "pintool", "d", "0", "debug");

// This function is called before every block
// Use the fast linkage for calls
VOID PIN_FAST_ANALYSIS_CALL docount(ADDRINT c, ADDRINT addr, string assemblyCode) { 
    icount += c; 
    bbcount += 1;
    if (icount/KnobIntervalSize.Value() >= length) {
        length += 1;
        bb_t *new_bb_table  = new bb_t;
        bb_table_list.push_back(new_bb_table);
    }
    
    // search bb_num in bb_num_map
    COUNTER mybbnum = 0;
    bb_t * bb_table = bb_table_list.back();

    bb_num_t::const_iterator its = (bb_num_map).find(addr);
    if (its == (bb_num_map).end())
    {
        // not found
        bb_num_map[addr] = bb_num;
        mybbnum = bb_num;
        bb_num += 1;

        // bb_t * bb_table = bb_table_list.back();
        BBLSTATS * bblstats = new BBLSTATS(addr, mybbnum, c);
        (*bb_table)[addr] = bblstats;

    }
    else{
        // found
        mybbnum = bb_num_map[addr];
        // search in current bb table
        bb_t::const_iterator it = (*bb_table).find(addr);
        if (it == (*bb_table).end())
        {
            BBLSTATS * bblstats = new BBLSTATS(addr, mybbnum, c);
            // bb_num += 1;
            (*bb_table)[addr] = bblstats;
        }
    }

    if (KnobDebug) {
        if (length == KnobBB) {
            OutputdebugFile<<std::hex<<addr<<": "<<std::dec<<assemblyCode<< endl;
            OutputdebugFile << "length "<<length << ", count " << icount <<endl;
        }
        
    }
    ((*bb_table)[addr])->_counter += 1;
}

// VOID DisplayInstruction(void *ip, string assemblyCode, COUNTER * counter){
//     OutFile<<std::hex<<ip<<": "<<std::dec<<assemblyCode<< endl;
//     OutFile << "length "<<length << ", count " << icount <<endl;
//     (*counter) += 1;
// }

    
// Pin calls this function every time a new basic block is encountered
// It inserts a call to docount
VOID Trace(TRACE trace, VOID *v)
{
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // Insert a call to docount for every bbl, passing the number of instructions.
        // IPOINT_ANYWHERE allows Pin to schedule the call anywhere in the bbl to obtain best performance.
        // Use a fast linkage for the call.

        BBL_InsertCall(bbl, IPOINT_BEFORE, AFUNPTR(docount), IARG_FAST_ANALYSIS_CALL, \
        IARG_UINT32, BBL_NumIns(bbl), \
        IARG_PTR, INS_Address(BBL_InsHead(bbl)), \
        IARG_PTR, new string(INS_Disassemble(BBL_InsHead(bbl))), \
        IARG_END);

        // BBL_InsertCall(bbl, IPOINT_BEFORE, AFUNPTR(DisplayInstruction), 
        //     IARG_PTR, INS_Address(BBL_InsHead(bbl)), 
        //     IARG_PTR, new string(INS_Disassemble(BBL_InsHead(bbl))), 
        //     IARG_PTR, &((*bb_table)[INS_Address(BBL_InsHead(bbl))]->_counter),
        //     IARG_END);
    }
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    // Write to a file since cout and cerr maybe closed by the application
    OutFile.setf(ios::showbase);
    OutPCFile.setf(ios::showbase);
    OutputdebugFile.setf(ios::showbase);

    // print bb output: bbnum, frequency
    for(bbl_t::iterator its = bb_table_list.begin(); its != bb_table_list.end(); its++) {
        OutFile << "T";
        for(bb_t::iterator it = (*its)->begin(); it != (*its)->end(); it++) {
            OutFile 
            << ":" << it->second->_num 
            // <<std::hex<<":"<<it->second->_addr 
             <<std::dec<<":"<<(it->second->_counter * it->second->_num_inst)
            //  <<":"<<it->second->_num_inst 
             << "   ";
        }
        OutFile << endl;
    }

    OutFile << "===================="<< endl;
    OutFile << "inscount " << icount << endl;
    OutFile << "bbCount " << bbcount << endl;
    OutFile << "bbnum " << bb_num << endl;
    OutFile << "interval " << bb_table_list.size() << endl;
    OutFile.close();

    // print pc output: bbnum, pc
    // OutPCFile <<;
    BOOL ifbreak = false;
    for(UINT64 i = 1; i <= bb_num; i++) {
        ifbreak = false;
        for(bbl_t::iterator its = bb_table_list.begin(); its != bb_table_list.end(); its++) {
            for(bb_t::iterator it = (*its)->begin(); it != (*its)->end(); it++) {
                if (it->second->_num == i) {
                    OutPCFile <<std::dec<< "F:" << i << ":" 
                        <<std::hex<< it->second->_addr << endl;
                    ifbreak = true;
                    break;
                }
                else continue;
            }
            if (ifbreak) break;
        }
    }
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool counts the number of dynamic instructions executed" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());
    OutPCFile.open(KnobOutputPCFile.Value().c_str());
    OutputdebugFile.open(KnobOutputdebugFile.Value().c_str());

    // Register Instruction to be called to instrument instructions
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
