/**
 * @file   cc_light_scheduler.h
 * @date   08/2017
 * @author Imran Ashraf
 * @brief  resource-constraint scheduler and code generator for cc light
 */

#ifndef CC_LIGHT_SCHEDULER_H
#define CC_LIGHT_SCHEDULER_H

#include <lemon/list_graph.h>
#include <lemon/lgf_reader.h>
#include <lemon/lgf_writer.h>
#include <lemon/dijkstra.h>
#include <lemon/connectivity.h>

#include <ql/utils.h>
#include <ql/gate.h>
#include <ql/circuit.h>
#include <ql/scheduler.h>
#include <ql/arch/cc_light_resource_manager.h>

#include <iomanip>
#include <map>
#include <sstream>

using namespace std;
using namespace lemon;

namespace ql
{
namespace arch
{

const size_t MAX_S_REG =32;
const size_t MAX_T_REG =64;

// TODO systematic
// typedef size_t                     qubit_t;
typedef std::vector<size_t>        qubit_set_t;
typedef std::pair<size_t,size_t>   qubit_pair_t;
typedef std::vector<qubit_pair_t>  qubit_pair_set_t;

size_t CurrSRegCount=0;
size_t CurrTRegCount=0;

qubit_set_t squbits;
qubit_pair_set_t dqubits;

class Mask
{
public:
    size_t regNo;
    std::string regName;
    qubit_set_t squbits;
    qubit_pair_set_t dqubits;

    Mask() {}

    Mask(qubit_set_t & qs) : squbits(qs)
    {
        
        if(CurrSRegCount < MAX_S_REG)
        {
            regNo = CurrSRegCount++;
            regName = "s" + std::to_string(regNo);
        }
        else
        {
            println(" !!!! Handle cases requiring more registers");
        }
    }

    Mask(std::string rn, qubit_set_t & qs ) : regName(rn), squbits(qs)
    { 
        if(CurrSRegCount < MAX_S_REG)
        {
            regNo = CurrSRegCount++;
        }
        else
        {
            println(" !!!! Handle cases requiring more registers");
        }
    }

    Mask(qubit_pair_set_t & qps) : dqubits(qps) 
    { 
        if(CurrTRegCount < MAX_T_REG)
        {
            regNo = CurrTRegCount++;
            regName = "t" + std::to_string(regNo);
        }
        else
        {
            println(" !!!! Handle cases requiring more registers");
        }
    }

};

std::map<size_t,Mask> SReg2Mask;
std::map<qubit_set_t,Mask> QS2Mask;

std::map<size_t,Mask> TReg2Mask;
std::map<qubit_pair_set_t,Mask> QPS2Mask;

static class MaskManager
{
public:
    MaskManager()
    {
        // add pre-defined smis
        for(size_t i=0; i<7; ++i)
        {
            qubit_set_t qs;
            qs.push_back(i);
            Mask m(qs);
            QS2Mask[qs] = m;
            SReg2Mask[m.regNo] = m;
        }

        // add some common single qubit masks
        {
            qubit_set_t qs;
            for(auto i=0; i<7; i++) qs.push_back(i);
            Mask m(qs); // TODO add proper support for:  Mask m(qs, "all_qubits");
            QS2Mask[qs] = m;
            SReg2Mask[m.regNo] = m;
        }

        {
            qubit_set_t qs;
            qs.push_back(0); qs.push_back(1); qs.push_back(5); qs.push_back(6);
            Mask m(qs); // TODO add proper support for:  Mask m(qs, "data_qubits");
            QS2Mask[qs] = m;
            SReg2Mask[m.regNo] = m;
        }

        {
            qubit_set_t qs;
            qs.push_back(2); qs.push_back(3); qs.push_back(4);
            Mask m(qs); // TODO add proper support for:  Mask m(qs, "ancilla_qubits");
            QS2Mask[qs] = m;
            SReg2Mask[m.regNo] = m;
        }


        // qubit_pair_set_t pre_defined_edges = { {2,0}, {0,3}, {3,1}, {1,4}, {2,5}, {5,3}, {3,6}, {6,4},
        //                                {0,2}, {3,0}, {1,3}, {4,1}, {5,2}, {3,5}, {6,3}, {4,6} };
        // // add smit
        // for(auto & p : pre_defined_edges)
        // {
        //     qubit_pair_set_t qps;
        //     qps.push_back(p);
        //     Mask m(qps);
        //     QPS2Mask[qps] = m;
        //     TReg2Mask[m.regNo] = m;
        // }

    }

    size_t getRegNo( qubit_set_t & qs )
    {
        auto it = QS2Mask.find(qs);
        if( it == QS2Mask.end() )
        {
            Mask m(qs);
            QS2Mask[qs] = m;
            SReg2Mask[m.regNo] = m;
        }
        return QS2Mask[qs].regNo;
    }

    size_t getRegNo( qubit_pair_set_t & qps )
    {
        auto it = QPS2Mask.find(qps);
        if( it == QPS2Mask.end() )
        {
            Mask m(qps);
            QPS2Mask[qps] = m;
            TReg2Mask[m.regNo] = m;
        }
        return QPS2Mask[qps].regNo;
    }

    std::string getRegName( qubit_set_t & qs )
    {
        auto it = QS2Mask.find(qs);
        if( it == QS2Mask.end() )
        {
            Mask m(qs);
            QS2Mask[qs] = m;
            SReg2Mask[m.regNo] = m;
        }
        return QS2Mask[qs].regName;
    }

    std::string getRegName( qubit_pair_set_t & qps )
    {
        auto it = QPS2Mask.find(qps);
        if( it == QPS2Mask.end() )
        {
            Mask m(qps);
            QPS2Mask[qps] = m;
            TReg2Mask[m.regNo] = m;
        }
        return QPS2Mask[qps].regName;
    }

    std::string getMaskInstructions()
    {
        std::stringstream ssmasks;
        for(size_t r=0; r<CurrSRegCount; ++r)
        {
            auto & m = SReg2Mask[r];
            ssmasks << "smis " << m.regName << ", {";
            for(auto it = m.squbits.begin(); it != m.squbits.end(); ++it)
            {
                ssmasks << *it;
                if( std::next(it) != m.squbits.end() )
                    ssmasks << ", ";
            }
            ssmasks << "} \n";
        }

        for(size_t r=0; r<CurrTRegCount; ++r)
        {
            auto & m = TReg2Mask[r];
            ssmasks << "smit " << m.regName << ", {";
            for(auto it = m.dqubits.begin(); it != m.dqubits.end(); ++it)
            {
                ssmasks << "(" << it->first << ", " << it->second << ")";
                if( std::next(it) != m.dqubits.end() )
                    ssmasks << ", ";
            }
            ssmasks << "} \n";
        }

        return ssmasks.str();
    }

} gMaskManager;


void PrintBundles(Bundles & bundles, bool verbose=false)
{
    if(verbose) 
    {
        println("Printing simplified CC-Light QISA");

        for (Bundle & abundle : bundles)
        {
            std::cout << abundle.start_cycle << "  ";

            for( auto secIt = abundle.ParallelSections.begin(); secIt != abundle.ParallelSections.end(); ++secIt )
            {
                for(auto insIt = secIt->begin(); insIt != secIt->end(); ++insIt )
                {
                    std::cout << (*insIt)->qasm();
                    if( std::next(insIt) != secIt->end() )
                    {
                        std::cout << " , ";
                    }
                }
                if( std::next(secIt) != abundle.ParallelSections.end() )
                {
                    std::cout << " | ";
                }
            }
            std::cout << "\n";
        }
    }
}

void PrintCCLighQasm(std::string prog_name, ql::quantum_platform & platform, Bundles & bundles, bool verbose=false)
{
    if(verbose) println("Printing CC-Light QISA");

    ofstream fout;
    string qisafname( ql::utils::get_output_dir() + "/" + prog_name + ".qisa");
    fout.open( qisafname, ios::binary);
    if ( fout.fail() )
    {
        println("Error opening file " << qisafname << std::endl
                 << "Make sure the output directory ("<< ql::utils::get_output_dir() << ") exists");
        return;
    }


    std::stringstream ssbundles;
    size_t curr_cycle=0; // first instruction should be with pre-interval 1, 'bs 1'

    ssbundles << "start:" << "\n";
    for (Bundle & abundle : bundles)
    {
        auto bcycle = abundle.start_cycle;
        auto delta = bcycle - curr_cycle;

        if(delta < 8)
            ssbundles << "    bs " << delta << "    ";
        else
            ssbundles << "    qwait " << delta-1 << "\n"
                      << "    bs 1    ";

        for( auto secIt = abundle.ParallelSections.begin(); secIt != abundle.ParallelSections.end(); ++secIt )
        {
            qubit_set_t squbits;
            qubit_pair_set_t dqubits;
            auto firstInsIt = secIt->begin();

            auto id = (*(firstInsIt))->name;
            auto iname = id;
            if ( !platform.instruction_settings[id]["cc_light_instr"].is_null() )
                iname = platform.instruction_settings[id]["cc_light_instr"];

            auto itype = (*(firstInsIt))->type();
            auto nOperands = ((*firstInsIt)->operands).size();
            if( itype == __nop_gate__ )
            {
                ssbundles << iname;
            }
            else
            {
                for(auto insIt = secIt->begin(); insIt != secIt->end(); ++insIt )
                {
                    if( 1 == nOperands )
                    {
                        auto & op = (*insIt)->operands[0];
                        squbits.push_back(op);
                    }
                    else if( 2 == nOperands )
                    {
                        auto & op1 = (*insIt)->operands[0];
                        auto & op2 = (*insIt)->operands[1];
                        dqubits.push_back( qubit_pair_t(op1,op2) );
                    }
                    else
                    {
                        throw ql::exception("Error : only 1 and 2 operand instructions are supported by cc light masks !",false);
                    }
                }
                std::string rname;
                if( 1 == nOperands )
                {
                    rname = gMaskManager.getRegName(squbits);
                }
                else if( 2 == nOperands )
                {
                    rname = gMaskManager.getRegName(dqubits);
                }
                else
                {
                    throw ql::exception("Error : only 1 and 2 operand instructions are supported by cc light masks !",false);
                }

                ssbundles << iname << " " << rname;
            }

            if( std::next(secIt) != abundle.ParallelSections.end() )
            {
                ssbundles << " | ";
            }
        }
        curr_cycle+=delta;
        ssbundles << "\n";
    }

    auto & lastBundle = bundles.back();
    auto lbduration = lastBundle.duration_in_cycles;
    ssbundles << "    qwait " << lbduration << "\n";
    ssbundles << "    br always, start" << "\n"
              << "    nop \n"
              << "    nop" << endl;

    if(verbose)
    {
        println("Printing CC-Light QISA");
        std::cout << gMaskManager.getMaskInstructions() << endl << ssbundles.str() << endl;
    }

    println("Writing CC-Light QISA to " << qisafname);
    fout << gMaskManager.getMaskInstructions() << endl << ssbundles.str() << endl;
    fout.close();
    if(verbose) println("Printing CC-Light QISA [Done]");
}


void PrintCCLighQasmTimeStamped(std::string prog_name, ql::quantum_platform & platform, Bundles & bundles, bool verbose=false)
{
    if(verbose) println("Printing Time-stamped CC-Light QISA");
    ofstream fout;
    string qisafname( ql::utils::get_output_dir() + "/" + prog_name + ".tqisa");
    fout.open( qisafname, ios::binary);
    if ( fout.fail() )
    {
        println("Error opening file " << qisafname << std::endl
                 << "Make sure the output directory ("<< ql::utils::get_output_dir() << ") exists");
        return;
    }


    std::stringstream ssbundles;
    size_t curr_cycle=0; // first instruction should be with pre-interval 1, 'bs 1'
    ssbundles << "start:" << "\n";
    for (Bundle & abundle : bundles)
    {
        auto bcycle = abundle.start_cycle;
        auto delta = bcycle - curr_cycle;

        if(delta < 8)
            ssbundles << std::setw(8) << curr_cycle << ":    bs " << delta << "    ";
        else
            ssbundles << std::setw(8) << curr_cycle << ":    qwait " << delta-1 << "\n"
                      << std::setw(8) << curr_cycle + (delta-1) << ":    bs 1    ";

        for( auto secIt = abundle.ParallelSections.begin(); secIt != abundle.ParallelSections.end(); ++secIt )
        {
            qubit_set_t squbits;
            qubit_pair_set_t dqubits;
            auto firstInsIt = secIt->begin();

            auto id = (*(firstInsIt))->name;
            auto iname = id;
            if ( !platform.instruction_settings[id]["cc_light_instr"].is_null() )
                iname = platform.instruction_settings[id]["cc_light_instr"];

            auto itype = (*(firstInsIt))->type();
            auto nOperands = ((*firstInsIt)->operands).size();
            if( itype == __nop_gate__ )
            {
                ssbundles << iname;
            }
            else
            {
                for(auto insIt = secIt->begin(); insIt != secIt->end(); ++insIt )
                {
                    if( 1 == nOperands )
                    {
                        auto & op = (*insIt)->operands[0];
                        squbits.push_back(op);
                    }
                    else if( 2 == nOperands )
                    {
                        auto & op1 = (*insIt)->operands[0];
                        auto & op2 = (*insIt)->operands[1];
                        dqubits.push_back( qubit_pair_t(op1,op2) );
                    }
                    else
                    {
                        throw ql::exception("Error : only 1 and 2 operand instructions are supported by cc light masks !",false);
                    }
                }
                std::string rname;
                if( 1 == nOperands )
                {
                    rname = gMaskManager.getRegName(squbits);
                }
                else if( 2 == nOperands )
                {
                    rname = gMaskManager.getRegName(dqubits);
                }
                else
                {
                    throw ql::exception("Error : only 1 and 2 operand instructions are supported by cc light masks !",false);
                }

                ssbundles << iname << " " << rname;
            }

            if( std::next(secIt) != abundle.ParallelSections.end() )
            {
                ssbundles << " | ";
            }
        }
        curr_cycle+=delta;
        ssbundles << "\n";
    }

    auto & lastBundle = bundles.back();
    auto lbduration = lastBundle.duration_in_cycles;
    ssbundles << std::setw(8) << curr_cycle   << ":    qwait " << lbduration << "\n";
    curr_cycle+=lbduration;
    ssbundles << std::setw(8) << curr_cycle++ << ":    br always, start" << "\n";
    ssbundles << std::setw(8) << curr_cycle++ << ":    nop \n";
    ssbundles << std::setw(8) << curr_cycle++ << ":    nop" << endl;


    if(verbose)
    {
        println("Printing Time-stamped CC-Light QISA");
        std::cout << gMaskManager.getMaskInstructions() << endl << ssbundles.str() << endl;
    }

    println("Writing Time-stamped CC-Light QISA to " << qisafname);
    fout << gMaskManager.getMaskInstructions() << endl << ssbundles.str() << endl;
    fout.close();

    if(verbose) println("Printing Time-stamped CC-Light QISA [Done]");
}

void cc_light_schedule(std::string prog_name, size_t nqubits, ql::circuit & ckt, ql::quantum_platform & platform, bool verbose=false)
{
    Bundles bundles1;

    if(verbose) println("Scheduling CC-Light instructions ...");
    Scheduler sched;
    sched.Init(nqubits, ckt, platform, verbose);
    bundles1 = sched.GetBundlesScheduleALAP(verbose);

    // combine parallel instrcutions of same type from different sections
    // into a single section
    for (Bundle & abundle : bundles1)
    {
        for( auto secIt1 = abundle.ParallelSections.begin(); secIt1 != abundle.ParallelSections.end(); ++secIt1 )
        {
            for( auto secIt2 = std::next(secIt1); secIt2 != abundle.ParallelSections.end(); ++secIt2)
            {
                auto insIt1 = secIt1->begin();
                auto insIt2 = secIt2->begin();
                if(insIt1 != secIt1->end() && insIt2 != secIt2->end() )
                {
                    auto n1 = (*insIt1)->name;
                    auto n2 = (*insIt2)->name;
                    if( n1 == n2 )
                    {
                        // std::cout << "splicing " << n1 << " and " << n2  << std::endl;
                        (*secIt1).splice(insIt1, (*secIt2) );
                    }
                }
            }
        }
    }

    // remove empty sections
    Bundles bundles2;
    for (Bundle & abundle1 : bundles1)
    {
        Bundle abundle2;
        abundle2.start_cycle = abundle1.start_cycle;
        abundle2.duration_in_cycles = abundle1.duration_in_cycles;
        for(auto & sec : abundle1.ParallelSections)
        {
            if( !sec.empty() )
            {
                abundle2.ParallelSections.push_back(sec);
            }
        }
        bundles2.push_back(abundle2);
    }

    // print scheduled bundles with parallelism
    // PrintBundles(bundles2, verbose);

    // print scheduled bundles with parallelism in cc-light syntax
    PrintCCLighQasm(prog_name, platform, bundles2, verbose);

    // print scheduled bundles with parallelism in cc-light syntax with time-stamps
    PrintCCLighQasmTimeStamped(prog_name, platform, bundles2, verbose);

    if(verbose) println("Scheduling CC-Light instructions [Done].");
}


void cc_light_schedule_rc( std::string prog_name, size_t nqubits, ql::circuit & ckt,
                           ql::quantum_platform & platform, bool verbose=false)
{
    Bundles bundles1;

    if(verbose) println("Resource constraint scheduling of CC-Light instructions ...");
    resource_manager_t rm(platform);
    Scheduler sched;
    sched.Init(nqubits, ckt, platform, verbose);

    bundles1 = sched.GetBundlesScheduleALAP(rm, verbose);

    // combine parallel instrcutions of same type from different sections
    // into a single section
    for (Bundle & abundle : bundles1)
    {
        for( auto secIt1 = abundle.ParallelSections.begin(); secIt1 != abundle.ParallelSections.end(); ++secIt1 )
        {
            for( auto secIt2 = std::next(secIt1); secIt2 != abundle.ParallelSections.end(); ++secIt2)
            {
                auto insIt1 = secIt1->begin();
                auto insIt2 = secIt2->begin();
                if(insIt1 != secIt1->end() && insIt2 != secIt2->end() )
                {
                    auto n1 = (*insIt1)->name;
                    auto n2 = (*insIt2)->name;
                    if( n1 == n2 )
                    {
                        // std::cout << "splicing " << n1 << " and " << n2  << std::endl;
                        (*secIt1).splice(insIt1, (*secIt2) );
                    }
                }
            }
        }
    }

    // remove empty sections
    Bundles bundles2;
    for (Bundle & abundle1 : bundles1)
    {
        Bundle abundle2;
        abundle2.start_cycle = abundle1.start_cycle;
        abundle2.duration_in_cycles = abundle1.duration_in_cycles;
        for(auto & sec : abundle1.ParallelSections)
        {
            if( !sec.empty() )
            {
                abundle2.ParallelSections.push_back(sec);
            }
        }
        bundles2.push_back(abundle2);
    }

    // print scheduled bundles with parallelism
    // PrintBundles(bundles2, verbose);

    // print scheduled bundles with parallelism in cc-light syntax
    PrintCCLighQasm(prog_name, platform, bundles2, verbose);

    // print scheduled bundles with parallelism in cc-light syntax with time-stamps
    PrintCCLighQasmTimeStamped(prog_name, platform, bundles2, verbose);

    if(verbose) println("Resource constraint scheduling of CC-Light instructions [Done].");
}

} // end of namespace arch
} // end of namespace ql

#endif
