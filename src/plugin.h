#ifndef CODE_PLUGIN_H
#define CODE_PLUGIN_H

#include <stdlib.h>
#include <stdio.h>
#include <stdexcept>
#include <string.h>
//#include <errors.h>

#include <gcc-plugin.h>
#include <plugin.h>
#include <tree.h>
#include <gimple.h>
#include <basic-block.h>
#include <context.h>
#include <tree-pass.h>
#include <c-family/c-pragma.h>
#include <plugin-version.h>
#include <gimple-iterator.h>
#include <dominance.h>
#include <bitmap.h>
#include <function.h>

/* #include <gcc-plugin.h>
 * #include <gimple.h>
 * #include <gimple-iterator.h>
 * #include <gcc-plugin.h>
 * #include <gimple.h>
 * #include <gimple-iterator.h>
 * #include <basic-block.h>
 * #include <bitmap.h>
 * #include <context.h>
 * #include <dominance.h>
 * #include <plugin.h>
 * #include <tree.h>
 * #include <tree-pass.h>
 */

int plugin_is_GPL_compatible;

#define PLOT

/* Enum to represent the collective operations */

enum mpi_collective_code {
#define DEFMPICOLLECTIVES( CODE, NAME ) CODE,
#include "MPI_collectives.def"
    LAST_AND_UNUSED_MPI_COLLECTIVE_CODE
#undef DEFMPICOLLECTIVES
} ;

/* Name of each MPI collective operations */
#define DEFMPICOLLECTIVES( CODE, NAME ) NAME,
const char *const mpi_collective_name[] = {
#include "MPI_collectives.def"
} ;

#undef DEFMPICOLLECTIVES

typedef struct bb_info{
  mpi_collective_code code;
  int count;
  int rank;
} bb_info;

typedef bb_info * BB_info;

// TD3 
void cfgviz_dump( function * fun, const char * suffix, int mpi );
int is_mpi(gimple *stmt);
mpi_collective_code which_mpi(gimple *stmt);
void mpi_tag_bb(function * fun);
void mpi_free_aux(function * fun);
void mpi_count_bb(function * fun);
void mpi_split_bb(function * fun);
int mpi_two_or_more(function * fun);

// TD4
void print_dominance(function * fun, enum cdi_direction dir);

// TD5
bitmap_head* dominance_frontier(function * fun, enum cdi_direction dir);
bitmap_head* post_dominance_frontier(function * fun, enum cdi_direction dir);
//void bb_rank(function * fun);
bitmap_head subgraph(function* fun);

void set_post_dominance_frontier(bitmap_head* set_pdf, bitmap_head node_set, 
    bitmap_head* pdf, function* fun);

void it_post_dominance_frontier(bitmap_head* ipdf, bitmap_head pdf_set,
    bitmap_head* pdf, function* fun);

// wrapped up functions

/* called first
 *    * split correctly the basic block
 *    * compute the rank and annotates the blocks
 *    * calls calculate dominance info
 */
void yeti_setup(function* fun);

/* Called at the end
 *    * free aux field in all basic blocks
 *    * free dominance info
 */
void yeti_cleanup(function *fun);

void yeti_warning(mpi_collective_code code, bitmap_head ipdf, 
    bitmap_head set, function* fun);

/* Must be called after setup
 *    * iterate over the tuples (code, rank)
 *    * compute the sets defined by these tuples
 *    * compute the it_pdf for each one
 *        * if non empty calls yeti_waring
 **/
void yeti_core(function *fun);

// 0 if the MPI collective is not monitored, 1 if it is. Order is from the MPI collective enum.
int MONITORED_MPI_COLLECTIVES[LAST_AND_UNUSED_MPI_COLLECTIVE_CODE] = {0};

#endif //CODE_PLUGIN_H
