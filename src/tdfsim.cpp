/**********************************************************************/
/*  Parallel-Fault Event-Driven Transition Delay Fault Simulator      */
/*                                                                    */
/**********************************************************************/

#include "atpg.h"

#include <cassert>
#include <sstream>

/* pack 16 faults into one packet.  simulate 16 faults togeter. 
 * the following variable name is somewhat misleading */
#define num_of_pattern 16

void ATPG::transition_delay_fault_simulation()
{
  int total_detected_fault_num  = 0;

  tdf_generate_fault_list();

  for( size_t i = vectors.size() - 1 ; i >= 0 ; --i )
  {
     forward_list<fptr> activated_faults    = tdf_simulate_v1( vectors[i] );
     int                detected_fault_num;

     flist_undetect.swap( activated_faults );

     tdf_simulate_v2( vectors[i], detected_fault_num );

     flist_undetect = move( activated_faults );

     flist_undetect.remove_if(  []( const fptr fault )
                                { return ( fault->detect == TRUE ); } );

     total_detected_fault_num += detected_fault_num;

     fprintf( stdout, "vector[%lu] detects %d faults (%d)\n",
              i, detected_fault_num, total_detected_fault_num );

     if( i == 0 ) break;
  }
  fprintf(  stdout, "# Result:\n" );
  fprintf(  stdout, "-----------------------\n" );
  fprintf(  stdout, "# total transition delay faults: %d\n", num_of_gate_fault );
  fprintf(  stdout, "# total detected faults: %d\n", total_detected_fault_num );
  fprintf(  stdout, "# fault coverage: %f %%\n",
            static_cast<double>( total_detected_fault_num ) * 100 / num_of_gate_fault );
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
    flist_undetect.push_front( fault.get() );
    flist.push_front( move( fault ) );
  }
  else if( io == GI && wire->onode.size() > 1 )
  {
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

       flist_undetect.push_front( fault_temp.get() );
       flist.push_front( move( fault_temp ) );
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
forward_list<ATPG::fptr> ATPG::tdf_simulate_v1( const string &pattern )
{
  assert( pattern.size() == cktin.size() + 1 ); // precondition

  forward_list<fptr> activated_faults;

  tdf_setup_pattern( pattern.substr( 0, cktin.size() ) );
  sim();

  // collect activated faults
  for( fptr fault : flist_undetect )
  {
     wptr wire = sort_wlist[fault->to_swlist];

     switch( fault->fault_type )
     {
       case STR:

         if( wire->value == FALSE )
           activated_faults.push_front( fault );
         break;

       case STF:

         if( wire->value == TRUE )
           activated_faults.push_front( fault );
         break;

       default: break;
     }
  }
  // end collect activated faults

  return activated_faults;
}

void ATPG::tdf_setup_pattern( const string &pattern )
{
  assert( pattern.size() == cktin.size() ); // precondition

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
}

void ATPG::tdf_simulate_v2( const string &pattern, int &detected_fault_num  )
{
  assert( pattern.size() == cktin.size() + 1 ); // precondition

  ostringstream input_pattern;

  input_pattern << pattern.back() << pattern.substr( 0, cktin.size() - 1 );

  fault_sim_a_vector( input_pattern.str(), detected_fault_num );
}
