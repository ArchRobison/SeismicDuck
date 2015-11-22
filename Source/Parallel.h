/* Copyright 1996-2014 Arch D. Robison 

   Licensed under the Apache License, Version 2.0 (the "License"); 
   you may not use this file except in compliance with the License. 
   You may obtain a copy of the License at 
   
       http://www.apache.org/licenses/LICENSE-2.0 
       
   Unless required by applicable law or agreed to in writing, software 
   distributed under the License is distributed on an "AS IS" BASIS, 
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
   See the License for the specific language governing permissions and 
   limitations under the License.
 */

/******************************************************************************
 Parallel control structures for Seismic Duck.
*******************************************************************************/

#if __INTEL_COMPILER>=1200
#define USE_CILK 1
#define USE_ARRAY_NOTATION 1
#define USE_TBB 0
#else
#define USE_TBB 1
#endif

#if USE_CILK

#include <cilk/cilk.h>

#if 1
//! Cilk implementation of one-dimensional ghost cell pattern using cilk_for.
template<typename Op>
void parallel_ghost_cell( size_t n, const Op& op ) {
    cilk_for( size_t i=1; i<n; ++i ) 
        op.exchangeBorders(i);
    cilk_for( size_t i=0; i<n; ++i )
        op.updateInterior(i);
}
#else
//! parallel_ghost_cell implemented with cilk_spawn and divide-and-conquer.
/** It's harder to understand than the implementation using cilk_for,
    but has slightly better cache locality. */
template<typename Op>
void parallel_ghost_cell_task( size_t lower, size_t upper, const Op& op ) {
    while( lower+1!=upper ) {
        size_t mid=lower+(upper-lower)/2u;
        op.exchangeBorders(mid);
        cilk_spawn parallel_ghost_cell_task( mid, upper, op );
        upper=mid;
    }
    op.updateInterior(lower);
}

//! Cilk divide and conquer implementation of one-dimensional ghost cell pattern.
/** n is the number of chunks, numbered 0..n-1.
    Class op must have two methods:
        op.exchangeBorders(i): exchange border information between chunks i-1 and i.
        op.updateInterior(i): update chunk i
*/
template<typename Op>
void parallel_ghost_cell( size_t n, const Op& op ) {
    if( n>0 )
        parallel_ghost_cell_task( 0, n, op );
};
#endif

#elif USE_TBB

#include "tbb/parallel_invoke.h"

//! TBB task for parallel_ghost_cell
template<typename Op>
class ghost_cell_task: public tbb::task {
    size_t lower, upper;
    const Op op;
    /*override*/ tbb::task* execute() {
        if( lower+1==upper ) {
            op.updateInterior(lower);
            return nullptr;
        } else {
            // Find place to split group of chunks into two subgroups
            size_t mid=lower+(upper-lower)/2u;
            // Exchange border information along the split
            op.exchangeBorders(mid);
            // Evaluate the two subgroups in parallel.  
            task& c = *new( allocate_continuation() ) tbb::empty_task();
            // Continuation implicitly waits on 2 children.
            c.set_ref_count(2);
            // Left child
            spawn(*new( c.allocate_child() ) ghost_cell_task(lower,mid,op));
            // Recycle self as right child and use TBB idiom for "tail call" of a task.
            recycle_as_child_of(c);
            lower=mid;
            return this;
        }
    }
public:
    //! Construct task that evaluates group of chunks [l..u) using op_.
    ghost_cell_task( size_t l, size_t u, const Op& op_ ) : lower(l), upper(u), op(op_) {}
};

//! TBB divide and conquer implementation of one-dimensional ghost cell pattern.
template<typename Op>
void parallel_ghost_cell( size_t n, const Op& op ) {
    if( n>0 ) {
        tbb::task* t = new( tbb::task::allocate_root() ) ghost_cell_task<Op>(0,n,op);
        tbb::task::spawn_root_and_wait(*t);
    }
};

#else

//! Serial implementation of parallel_ghost_cell
/** n is the number of chunks, numbered 0..n-1.
    Class op must have two methods:
        op.exchangeBorders(i): exchange border information between chunks i-1 and i.
        op.updateInterior(i): update chunk i
*/
template<typename Op>
void parallel_ghost_cell( size_t n, const Op& op ) {
    for( size_t i=n; i-->0; ) {
        if( i )
            op.exchangeBorders(i);
        op.updateInterior(i);
    }
}
#endif /* serial */
