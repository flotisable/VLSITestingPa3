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

inline int invert_fault_type( int fault_type )
{ return ( fault_type == STR ) ? STF: STR; }

void ATPG::transition_delay_fault_simulation()
{
  int total_detected_fault_num  = 0;

  tdf_generate_fault_list();

  for( size_t i = vectors.size() - 1 ; i >= 0 ; --i )
  {
     vector<fptr> activated_faults    = tdf_simulate_v1(  vectors[i] );
     int          detected_fault_num  = tdf_simulate_v2(  vectors[i],
                                                          activated_faults );

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
vector<ATPG::fptr> ATPG::tdf_simulate_v1( const string &pattern )
{
  assert( pattern.size() == cktin.size() + 1 ); // precondition

  vector<fptr> activated_faults;

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

int ATPG::tdf_simulate_v2(  const string        &pattern,
                            const vector<fptr>  &activated_faults )
{
  assert( pattern.size() == cktin.size() + 1 ); // precondition

  ostringstream input_pattern;
  vector<fptr>  fault_packet;
  size_t        start_wire_index    = sort_wlist.size();
  int           detected_fault_num  = 0;

  input_pattern << pattern.back() << pattern.substr( 0, cktin.size() - 1 );

  fault_packet.reserve( num_of_pattern );

  tdf_setup_pattern( input_pattern.str() );
  sim();
  tdf_init_fault_sim_wire();

  for( size_t i = 0 ; i < activated_faults.size() ; ++i )
  {
     fptr fault         = activated_faults[i];
     wptr faulty_wire;
     int  fault_type;

     if( !tdf_should_be_in_packet( fault, faulty_wire, fault_type ) ) continue;

     start_wire_index = tdf_add_fault_into_packet( fault,
                                                   fault_packet,
                                                   start_wire_index,
                                                   faulty_wire,
                                                   fault_type );

     if(  fault_packet.size() == num_of_pattern ||
          i + 1 == activated_faults.size() )
     {
       tdf_fault_simulation( start_wire_index );

       detected_fault_num +=  tdf_faulty_wire_postprocess( fault_packet );
       start_wire_index   =   sort_wlist.size();
       fault_packet.clear();
     }
  }
  return detected_fault_num;
}

void ATPG::tdf_init_fault_sim_wire()
{
  for( wptr wire : sort_wlist )
  {
    switch( wire->value )
    {
      case TRUE:

        wire->wire_value1 = wire->wire_value2 = ALL_ONE;
        break;

      case FALSE:

        wire->wire_value1 = wire->wire_value2 = ALL_ZERO;
        break;

      case U:
      default:

        wire->wire_value1 = wire->wire_value2 = 0x55555555;
        break;
    }
  }
}

/*!
 *  Test if the **fault** should be added into packet.
 *  **faulty_wire** and **fault_type** will be modified.
 *
 *  \return if the **fault** should be added into packet.
 */
bool ATPG::tdf_should_be_in_packet( const fptr  fault,
                                    wptr        &faulty_wire,
                                    int &fault_type )
{
   faulty_wire = sort_wlist[fault->to_swlist];
   fault_type  = fault->fault_type;

   if( fault_type == faulty_wire->value ) return false;

   if( fault->node->type == OUTPUT )
   {
     fault->detect = TRUE;
     return false;
   }

   if     ( fault->io == GO )
   {
     if( faulty_wire->flag & OUTPUT )
     {
       fault->detect = TRUE;
       return false;
     }
   }
   else if( fault->io == GI )
   {
     faulty_wire = tdf_get_faulty_wire( fault, fault_type );

     if( !faulty_wire ) return false;

     if( faulty_wire->flag & OUTPUT )
     {
       fault->detect = TRUE;
       return false;
     }
   }
   return true;
}

/*!
 *  Add **fault** into **packet**.
 *  **packet** will be modified.
 *
 *  \return the updated **start_wire_index**
 */
size_t ATPG::tdf_add_fault_into_packet(
  const fptr    fault,
  vector<fptr>  &packet,
  const size_t  start_wire_index,
  const wptr    faulty_wire,
  int           fault_type )
{
  if( !( faulty_wire->flag & FAULTY ) )
  {
    faulty_wire->flag |= FAULTY;
    wlist_faulty.push_front( faulty_wire );
  }

  inject_fault_value( faulty_wire, static_cast<int>( packet.size() ),
                      fault_type );
  faulty_wire->flag |= FAULT_INJECTED;

  for( nptr node : faulty_wire->onode )
     node->owire.front()->flag |= SCHEDULED;

  packet.push_back( fault );

  return min( start_wire_index, static_cast<size_t>( fault->to_swlist ) );
}

ATPG::wptr ATPG::tdf_get_faulty_wire( const fptr fault, int &fault_type )
{
  nptr  node = fault->node;
  int   type = node->type;

  if( type == NOT || type == NAND || type == NOR || type == EQV )
    fault_type = invert_fault_type( fault->fault_type );
  else
    fault_type = fault->fault_type;

  for( const wptr wire : node->iwire )
  {
     if( wire == sort_wlist[fault->to_swlist] ) continue;

     if     ( type == AND || type == NAND )
     {
       if( wire->value != TRUE ) return nullptr;
     }
     else if( type == OR || type == NOR )
     {
       if( wire->value != FALSE ) return nullptr;
     }
     else if( type == XOR || type == EQV )
     {
       if( wire->value == TRUE )
         fault_type = invert_fault_type( fault_type );
     }
  }
  return node->owire.front();
}

void ATPG::tdf_fault_simulation( const size_t start_wire_index )
{
  for( size_t i = start_wire_index ; i < sort_wlist.size() ; ++i )
  {
     wptr wire = sort_wlist[i];

     if( wire->flag & SCHEDULED )
     {
       wire->flag &= ~SCHEDULED;
       fault_sim_evaluate( wire );
     }
  }
}

int ATPG::tdf_faulty_wire_postprocess( vector<fptr> &fault_packet )
{
  int detected_fault_num = 0;

  while( !wlist_faulty.empty() )
  {
    wptr wire = wlist_faulty.front();

    wlist_faulty.pop_front();

    if( wire->flag & OUTPUT )
    {
      for( size_t i = 0 ; i < fault_packet.size() ; ++i )
      {
         if( fault_packet[i]->detect ) continue;
         if( ( ( wire->wire_value1 ^ wire->wire_value2  ) & Mask[i] ) &&
             ( ( wire->wire_value1 ^ Unknown[i]         ) & Mask[i] ) &&
             ( ( wire->wire_value2 ^ Unknown[i]         ) & Mask[i] ) )
         {
           fault_packet[i]->detect = TRUE;
           ++detected_fault_num;
         }
      }
    }
    tdf_reset_faulty_wire( wire );
  }
  return detected_fault_num;
}

void ATPG::tdf_reset_faulty_wire( const wptr faulty_wire )
{
  faulty_wire->flag         &=  ~FAULTY;
  faulty_wire->flag         &=  ~FAULT_INJECTED;
  faulty_wire->fault_flag   &=  ALL_ZERO;
  faulty_wire->wire_value2  =   faulty_wire->wire_value1;
}
