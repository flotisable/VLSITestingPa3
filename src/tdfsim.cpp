/**********************************************************************/
/*  Parallel-Fault Event-Driven Transition Delay Fault Simulator      */
/*                                                                    */
/**********************************************************************/

#include "atpg.h"

/* pack 16 faults into one packet.  simulate 16 faults togeter. 
 * the following variable name is somewhat misleading */
#define num_of_pattern 16

void ATPG::transition_delay_fault_simulation() {
  
  for( const string &pattern : vectors )
  {
     const vector<fptr> activated_faults = tdf_simulate_v1( pattern );
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
