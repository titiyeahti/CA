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
//void bb_rank(function * fun);
bitmap_head subgraph(function* fun);

bitmap_head set_post_dominance_frontier(bitmap_head node_set, 
    bitmap_head* pdf, function* fun);

bitmap_head it_post_dominance_frontier(bitmap_head pdf_set, bitmap_head* pdf,
    function* fun);

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
  int rank;
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
    int i;
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
    BB_info bbi = new bb_info;
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
    bbi->rank = 0;
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
//Should be ok
    while(count > 1){
      stmt = gsi_stmt(gsi);
      if(is_mpi(stmt)){
        split_block(bb, stmt);
        BB_info info = new bb_info;
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
        BB_info bbi =  new bb_info;
        bbi->code = which_mpi(stmt);
        bbi->rank = 0;
        bb->aux = (void*) bbi;
      }
    }
  }
}

void
mpi_free_aux(function * fun)
{
  basic_block bb;
  //auto_bitmap bm;
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
          if(!(bitmap_set_bit((bitmap) &map[runner->index], bb->index)))
              break;

          runner = get_immediate_dominator(CDI_POST_DOMINATORS, runner);
        }
      }
    }
  }

  return map;
}

bitmap_head set_post_dominance_frontier(bitmap_head node_set, 
    bitmap_head* pdf, function* fun){

  bitmap_head pdf_set;
  bitmap_initialize(&pdf_set, &bitmap_default_obstack);

  basic_block bb, bb2;
  bool found;

  FOR_ALL_BB_FN(bb, fun){
    if(bitmap_bit_p( &node_set, bb->index))
      bitmap_ior_into(&pdf_set, &pdf[bb->index]);
  }

  FOR_ALL_BB_FN(bb, fun){
    if(bitmap_bit_p(&pdf_set, bb->index)){
      found = false;

      FOR_ALL_BB_FN(bb2, fun){
        /* if bb2 is not int the set and bb->index is not in the pdf of bb2 */
        if(!bitmap_bit_p(&node_set, bb2->index) &&
            bitmap_bit_p(&pdf[bb2->index], bb->index))
          found = true;
      }

      if(!found)
        bitmap_clear_bit(&pdf_set, bb->index);
    }
  }

  return pdf_set;
}

bitmap_head it_post_dominance_frontier(bitmap_head pdf_set, bitmap_head* pdf,
    function* fun){
  
  basic_block bb, b;
  bitmap_head tmp, test, ipdf;

  bitmap_initialize(&tmp, &bitmap_default_obstack);
  bitmap_initialize(&test, &bitmap_default_obstack);
  bitmap_initialize(&ipdf, &bitmap_default_obstack);

  FOR_ALL_BB_FN(bb, fun){
    if(bitmap_bit_p(&pdf_set, bb->index)){
      bitmap_copy(&tmp, &test);

      FOR_ALL_BB_FN(b, fun){
        if(bitmap_bit_p(&pdf[bb->index], b->index))
          bitmap_set_bit(&test, b->index);
      }

      bitmap_copy(&test, &ipdf);
      bitmap_ior(&ipdf, &test, &tmp);
    }
  }

  bitmap_release(&tmp);
  bitmap_release(&test);
  return ipdf;
}


/* Compute the subgraph (subset of the edges and the rank of each vertex*/
bitmap_head subgraph(function * fun){
  int n, bot, top, rk;
  BB_info info,  info2;
  edge_iterator ie;
  edge p;
  n = n_basic_blocks_for_fn(fun);
  
  bitmap_head in_queue, emap, set;
  bitmap_initialize(&in_queue, &bitmap_default_obstack);
  bitmap_initialize(&emap, &bitmap_default_obstack);
  bitmap_initialize(&set, &bitmap_default_obstack);
  basic_block cur;
  basic_block next;
  basic_block queue[n];

  bot = 0;
  top = 0;

  queue[top] = ENTRY_BLOCK_PTR_FOR_FN(fun);
  info = (BB_info) queue[top]->aux;
  info->rank = 0;
  top++;

  while(bot<top){
    cur = queue[bot];
    bot ++;
    bitmap_set_bit(&set, cur->index);
    FOR_EACH_EDGE(p, ie, cur->succs){
      /* the edges is cur -> next 
       * since cur is unique*/
      next = p->dest;
      
      if(!bitmap_bit_p(&set, next->index))
        bitmap_set_bit(&emap, p->dest_idx);

      if(!bitmap_bit_p(&in_queue, next->index)){
        bitmap_set_bit(&in_queue, next->index);
        queue[top] = next;
        top ++;
      }
    }

    /* Rank computation */
    rk = 0;
    info = (BB_info) cur->aux;
    FOR_EACH_EDGE(p, ie, cur->preds){
      if(bitmap_bit_p(&emap, p->dest_idx)){
        next = p->src;
        info2 = (BB_info) next->aux;
        
        if(info2->rank > rk)
          rk = info2->rank;
      }
    }

    if(info->code < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE)
      info->rank = rk+1;
    else
      info->rank = rk;
  }

  bitmap_release(&in_queue);
  bitmap_release(&set);
  return emap;
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
      printf("FUNCTION : %s\n", function_name(fun));
      return true;
    }

    unsigned int execute (function *fun) {
      BB_info info;
      bitmap_head* df;
      bitmap_head* pdf;
      bitmap_head esg;
      basic_block bb;
      mpi_count_bb(fun);

      FOR_ALL_BB_FN(bb, fun){
        info = (BB_info) bb->aux;
        if(info->code < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE)
          printf("%d : %s (%d)\n", bb->index, mpi_collective_name[info->code],
              info->rank);
        else
          printf("%d : not mpi call (%d)\n", bb->index, info->rank);
      }
      mpi_split_bb(fun);
      mpi_tag_bb(fun);
      esg = subgraph(fun);
      cfgviz_dump(fun, "", 0); 
      cfgviz_dump(fun, "mpi", 1); 
      char str[10];

      FOR_ALL_BB_FN(bb, fun){
        info = (BB_info) bb->aux;
        if(info->code < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE)
          printf("%d : %s (%d)\n", bb->index, mpi_collective_name[info->code],
              info->rank);
        else
          printf("%d : not mpi call (%d)\n", bb->index, info->rank);
      }

      //print_dominance(fun, CDI_DOMINATORS);

      printf("DF:\n");
      calculate_dominance_info(CDI_DOMINATORS);
      df = dominance_frontier(fun);
      FOR_ALL_BB_FN(bb, fun){
        sprintf(str, "%d :", bb->index); 
        bitmap_print(stdout, &df[bb->index], str, "\n");
        bitmap_release(&df[bb->index]);
      }
      delete df;
      
      //print_dominance(fun, CDI_POST_DOMINATORS);

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
      mpi_free_aux(fun);
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

	snprintf( target_filename, 1024, "dot/%s_file_%d_%s.dot", 
			current_function_name(),
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
      if((is_gimple_call(stmt)&&(!mpi)) || is_mpi(stmt)){
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
