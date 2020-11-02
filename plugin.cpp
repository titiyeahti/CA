/*
 * =====================================================================================
 *
 *       Filename:  plugin.cpp
 *
 *    Description:  First attempt to write a plugin for GCC.
 *
 *        Version:  1.0
 *        Created:  22/09/2020 14:19:32
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Thibaut Milhaud (tm), thibaut.milhaud@ensiie.fr
 *   Organization:  ensiie, univ-tln/Imath
 *
 * =====================================================================================
 */

#include <cstdio>
#include <stdlib.h>
#include <gcc-plugin.h>
#include <plugin.h>
#include <tree.h>
#include <gimple.h>
#include <basic-block.h>
#include <context.h>
#include <tree-pass.h>
#include <plugin-version.h>
#include <gimple-iterator.h>
#include <dominance.h>
#include <bitmap.h>

int plugin_is_GPL_compatible;

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


// CODE TD3
struct pass_data my_pass_data = {
  GIMPLE_PASS, /* type */
  "MY_PASS", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  TV_NONE, /* tv_id */
  PROP_gimple_any, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  0, /* todo_flags_finish */
};

typedef struct bb_info{
  mpi_collective_code code;
  int count;
} bb_info;

typedef bb_info * BB_info;

int is_mpi(gimple *stmt){
  if(is_gimple_call(stmt)){
    tree node = gimple_call_fndecl(stmt); 
    const char* string = IDENTIFIER_POINTER( DECL_NAME(node) );
    int i;
    for(i=0; i<LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
      if(!strcmp(mpi_collective_name[i], string))
        return 1;
    }
  }
  return 0;
}

mpi_collective_code which_mpi(gimple *stmt){
  if(is_gimple_call(stmt)){
    tree node = gimple_call_fndecl(stmt); 
    const char* string = IDENTIFIER_POINTER( DECL_NAME(node) );
    char i;
    for(i=0; i<LAST_AND_UNUSED_MPI_COLLECTIVE_CODE; i++) {
      if(!strcmp(mpi_collective_name[i], string))
        return (mpi_collective_code) i;
    }
  }
  return LAST_AND_UNUSED_MPI_COLLECTIVE_CODE;
}

void mpi_count_bb(function* fun){
  int count;
  mpi_collective_code code = LAST_AND_UNUSED_MPI_COLLECTIVE_CODE;
  mpi_collective_code res_code;
  basic_block bb;
  gimple* stmt;
  gimple_stmt_iterator gsi;
  FOR_ALL_BB_FN(bb, fun) {
    count = 0;
    res_code = LAST_AND_UNUSED_MPI_COLLECTIVE_CODE;
    BB_info bbi =  new bb_info[1];
    for(gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)){
      stmt = gsi_stmt(gsi);
      code = which_mpi(stmt);
      if(code != LAST_AND_UNUSED_MPI_COLLECTIVE_CODE){
        res_code = code;
        count++;
      }
    }
    if(res_code != LAST_AND_UNUSED_MPI_COLLECTIVE_CODE)
      bbi->code = res_code;
    else 
      bbi->code = code;

    bbi->count = count;
    bb->aux = (void*) bbi;
  }
}

int mpi_two_or_more(function * fun){
  basic_block bb;
  int count;
  FOR_ALL_BB_FN(bb, fun){
    count = ((BB_info)bb->aux)->count;
    if(count>1){
      return 1;
    }
  }
  return 0;
}

void mpi_split_bb(function * fun){
  basic_block bb;
  int count;
  FOR_ALL_BB_FN(bb, fun){
    count = ((BB_info)bb->aux)->count;
    //printf("count : %d\n", count);
    gimple* stmt;
    gimple_stmt_iterator gsi;
    gsi = gsi_start_bb(bb);
// TODO modifier cet endroit
    while(count > 1){
      stmt = gsi_stmt(gsi);
      if(is_mpi(stmt)){
        split_block(bb, stmt);
        BB_info info = new bb_info[1];
        count --;
        info->count = count;
        bb = bb->next_bb;
        bb->aux = (void*) info;
        gsi = gsi_start_bb(bb);
      }
      gsi_next(&gsi);
    }
  }
}

void
mpi_tag_bb(function * fun)
{
  basic_block bb;
  gimple* stmt;
  gimple_stmt_iterator gsi;
  FOR_ALL_BB_FN(bb, fun) {
    for(gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)){
      stmt = gsi_stmt(gsi);
      if(is_mpi(stmt)){
        BB_info bbi =  new bb_info[1];
        bbi->code = which_mpi(stmt);
        bb->aux = (void*) bbi;
      }
    }
  }
}

void
mpi_free_aux(function * fun)
{
  basic_block bb;
  gimple* stmt;
  gimple_stmt_iterator gsi;
  auto_bitmap bm;
  FOR_ALL_BB_FN(bb, fun) {
    if(bb->aux){
      delete ((BB_info) bb->aux);
      bb->aux = NULL;
    }
  }
}

// CODE TD4 

void print_dominance(function * fun, enum cdi_direction dir){
  basic_block bb;
  basic_block ib;
  vec<basic_block> v;
  unsigned id, iid;
  unsigned i;
  calculate_dominance_info(dir);
  
  FOR_ALL_BB_FN(bb, fun){
    v = get_all_dominated_blocks(dir, bb);
    id = bb->index;
    printf("%d : ", id);
    FOR_EACH_VEC_ELT(v, i, ib){
      iid = ib->index;
      if(id != iid)
        printf("%d ", ib->index);
    }

    printf("\n");
  }

  free_dominance_info(fun, dir);
}

// CODE TD5
bitmap_head* dominance_frontier(function * fun){
  int n = n_basic_blocks_for_fn(fun);
  basic_block bb;
  basic_block runner;
  basic_block doms;
  edge_iterator ie;
  edge p;
  bitmap_head* map;
  
  map = new bitmap_head[n];
  
  FOR_EACH_BB_FN (bb, fun)
    bitmap_initialize (&map[bb->index], &bitmap_default_obstack);

  FOR_ALL_BB_FN(bb, fun){
    if(EDGE_COUNT(bb->preds) > 1){
      FOR_EACH_EDGE(p, ie, bb->preds){
        runner = p->src;
        doms = get_immediate_dominator(CDI_DOMINATORS, bb);
        while (runner != doms){
          /* ajout de b dans la frontière de dominance de p */
          bitmap_set_bit((bitmap) &map[runner->index], bb->index);
          runner = get_immediate_dominator(CDI_DOMINATORS, runner);
        }
      }
    }
  }

  return map;
}

bitmap_head* post_dominance_frontier(function * fun){
  int n = n_basic_blocks_for_fn(fun);
  basic_block bb;
  basic_block runner;
  basic_block doms;
  edge_iterator ie;
  edge p;
  bitmap_head* map;
  
  map = new bitmap_head[n];
  
  FOR_ALL_BB_FN (bb, fun)
    bitmap_initialize (&map[bb->index], &bitmap_default_obstack);

  FOR_ALL_BB_FN(bb, fun){
    if(EDGE_COUNT(bb->succs) > 1){
      FOR_EACH_EDGE(p, ie, bb->succs){
        
        runner = p->dest;
        if(runner == EXIT_BLOCK_PTR_FOR_FN(fun)) /* puits */
          continue;

        doms = get_immediate_dominator(CDI_POST_DOMINATORS, bb);
        while (runner != doms){
          /* ajout de b dans la frontière de dominance de p */
          bitmap_set_bit((bitmap) &map[runner->index], bb->index);
          runner = get_immediate_dominator(CDI_POST_DOMINATORS, runner);
        }
      }
    }
  }

  return map;
}

class my_pass : public gimple_opt_pass
{
  public :
    my_pass(gcc::context *ctxt)
      : gimple_opt_pass(my_pass_data, ctxt) {}

  my_pass *clone() {
      return new my_pass(g);
    }

    bool gate (function *fun) {
      return true;
    }

    unsigned int execute (function *fun) {
      bitmap_head* df;
      bitmap_head* pdf;
      int b;
      basic_block bb;
      mpi_count_bb(fun);
      mpi_split_bb(fun);
      mpi_free_aux(fun);
      mpi_tag_bb(fun);
      mpi_free_aux(fun);
      char str[10];

      printf("DF:\n");
      calculate_dominance_info(CDI_DOMINATORS);
      df = dominance_frontier(fun);
      FOR_ALL_BB_FN(bb, fun){
        sprintf(str, "%d :", bb->index); 
        bitmap_print(stdout, &df[bb->index], str, "\n");
        bitmap_release(&df[bb->index]);
      }
      delete df;
      
      printf("\n\nPDF:\n");
      calculate_dominance_info(CDI_POST_DOMINATORS);
      pdf = post_dominance_frontier(fun);
      FOR_ALL_BB_FN(bb, fun){
        sprintf(str, "%d :", bb->index); 
        bitmap_print(stdout, &pdf[bb->index], str, "\n");
        bitmap_release(&pdf[bb->index]);
      }
      delete pdf;

      free_dominance_info(CDI_POST_DOMINATORS);
      free_dominance_info(CDI_DOMINATORS);
      return 0;
    }
};

  int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
  my_pass p(g);
  if (!plugin_default_version_check (version, &gcc_version))
    return 1;

  struct register_pass_info pass_info;
  pass_info.pass = &p;
  pass_info.reference_pass_name = "cfg";
  pass_info.ref_pass_instance_number = 0;
  pass_info.pos_op = PASS_POS_INSERT_AFTER;

  register_callback("my_pass",
      PLUGIN_PASS_MANAGER_SETUP,
      NULL,
      &pass_info);

  return 0;
}

	static char * 
cfgviz_generate_filename( function * fun, const char * suffix )
{
	char * target_filename ; 

	target_filename = (char *)xmalloc( 2048 * sizeof( char ) ) ;

	snprintf( target_filename, 1024, "%s_%s_%d_%s.dot", 
			current_function_name(),
			LOCATION_FILE( fun->function_start_locus ),
			LOCATION_LINE( fun->function_start_locus ),
			suffix ) ;

	return target_filename ;
}

/* Dump the graphviz representation of function 'fun' in file 'out' */
	static void 
cfgviz_internal_dump( function * fun, FILE * out, int mpi ) 
{

	// Print the header line and open the main graph
	fprintf(out, "Digraph G{\n");

  basic_block bb;
  gimple* stmt;
  gimple_stmt_iterator gsi;
  edge e;
  edge_iterator ei;

  tree node;
  int index, next;

  FOR_ALL_BB_FN(bb, fun){
    index = bb->index;
//    if(bb->aux) printf("N%d - AUX: %d\tcount: %d\n", index,
//        ((BB_info) bb->aux)->code, 
//        ((BB_info) bb->aux)->count);

    fprintf(out, "\tN%d [label=\"%d :\\n", index, index);
    for(gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)){
      stmt = gsi_stmt(gsi);
      if(is_gimple_call(stmt)&&(!mpi) || is_mpi(stmt)){
        node = gimple_call_fndecl(stmt);
        fprintf(out, "%s\\n", IDENTIFIER_POINTER( DECL_NAME(node) ));
      }
    }
    fprintf(out, "\" shape=ellipse]\n");
  }

  FOR_ALL_BB_FN(bb, fun){
    index = bb->index;
    FOR_EACH_EDGE(e, ei, bb->succs){
      next = e->dest->index;
      fprintf(out, "\tN%d -> N%d [color=red]\n", index, next);
    }
  }
	fprintf(out, "}\n");
}

	void 
cfgviz_dump( function * fun, const char * suffix, int mpi)
{
	char * target_filename ; 
	FILE * out ;

	target_filename = cfgviz_generate_filename( fun, suffix) ;

	printf( "[GRAPHVIZ] Generating CFG of function %s in file <%s>\n",
			current_function_name(), target_filename ) ;

	out = fopen( target_filename, "w" );
  if(!out)
    printf("filename : %s\n", target_filename);
  
  else{ 
    cfgviz_internal_dump( fun, out, mpi) ;

    fclose( out ) ;
    free( target_filename ) ;
  }
}
