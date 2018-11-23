/**********************************************************************/
/*  Parallel-Fault Event-Driven Transition Delay Fault Simulator      */
/*                                                                    */
/**********************************************************************/

#include "atpg.h"

#include <cassert>

/* pack 16 faults into one packet.  simulate 16 faults togeter. 
 * the following variable name is somewhat misleading */
#define num_of_pattern 16

void ATPG::transition_delay_fault_simulation()
{
  tdf_generate_fault_list();

  for( const string &pattern : vectors )
  {
     const vector<fptr> activated_faults = tdf_simulate_v1( pattern );
  }
}

void ATPG::tdf_generate_fault_list()
{
  int fault_num = 0;

  flist.clear();
  flist_undetect.clear();
  num_of_gate_fault = 0;

  for( auto it = sort_wlist.crbegin() ; it != sort_wlist.crend() ; ++it )
  {
     tdf_generate_fault( *it, GO, STR );
     tdf_generate_fault( *it, GO, STF );
     tdf_generate_fault( *it, GI, STR );
     tdf_generate_fault( *it, GI, STF );
  }

  for( fptr fault : flist_undetect )
     fault->fault_no = fault_num++;
}

void ATPG::tdf_generate_fault( const wptr wire, short io, short fault_type )
{
  fptr_s fault{ new FAULT };

  // init fault
  fault->io             = io;
  fault->fault_type     = fault_type;
  fault->to_swlist      = wire->wlist_index;
  fault->eqv_fault_num  = 1;
  // end init fault

  if( io == GO )
  {
    fault->node = wire->inode.front();

    ++num_of_gate_fault;
    flist.push_front( move( fault ) );
    flist_undetect.push_front( fault.get() );
  }
  else // GI
  {
    // preconditions
    assert( io == GI );
    assert( wire->onode.size() > 1 );
    // end preconditions

    for( nptr node : wire->onode )
    {
       fptr_s fault_temp{ new FAULT };

       *fault_temp      = *fault;
       fault_temp->node = node;

       for( size_t i = 0 ; i < node->iwire.size() ; ++i )
          if( node->iwire[i] == wire )
          {
            fault_temp->index = i;
            break;
          }

       flist.push_front( move( fault_temp ) );
       flist_undetect.push_front( fault_temp.get() );
    }
    num_of_gate_fault += wire->onode.size();
  }
}

/*!
 *  Simulate v1 of trandition delay fault.
 *
 *  \return
 *
 *    the activated fault list
 */
vector<ATPG::fptr> ATPG::tdf_simulate_v1( const string &pattern )
{
  vector<fptr> activated_faults;

  // initialize circuit
  for( size_t i = 0 ; i < sort_wlist.size() ; ++i )
  {
     wptr wire = sort_wlist[i];

     if( i < cktin.size() ) // PI
     {
       wire->value  =   ctoi( pattern[i] );
       wire->flag   |=  CHANGED;
     }
     else
       wire->value = U;
  }
  // end initialize circuit

  sim();

  // collect activated faults
  for( fptr fault : flist_undetect )
  {
     wptr wire = sort_wlist[fault->to_swlist];

     switch( fault->fault_type )
     {
       case STR:

         if( wire->value == FALSE )
           activated_faults.push_back( fault );
         break;

       case STF:

         if( wire->value == TRUE )
           activated_faults.push_back( fault );
         break;

       default: break;
     }
  }
  // end collect activated faults

  return activated_faults;
}
