
#include "plugin.h"


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

void set_post_dominance_frontier(bitmap_head* pdf_set, bitmap_head node_set, 
    bitmap_head* pdf, function* fun){

  basic_block bb, bb2;
  bool found;

  FOR_ALL_BB_FN(bb, fun){
    if(bitmap_bit_p( &node_set, bb->index))
      bitmap_ior_into(pdf_set, &pdf[bb->index]);
  }

  FOR_ALL_BB_FN(bb, fun){
    if(bitmap_bit_p(pdf_set, bb->index)){
      found = false;

      FOR_ALL_BB_FN(bb2, fun){
        /* if bb2 is not int the set and bb->index is not in the pdf of bb2 */
        if(!bitmap_bit_p(&node_set, bb2->index) &&
            bitmap_bit_p(&pdf[bb2->index], bb->index))
          found = true;
      }

      if(!found)
        bitmap_clear_bit(pdf_set, bb->index);
    }
  }
}

void it_post_dominance_frontier(bitmap_head* ipdf, bitmap_head pdf_set, 
    bitmap_head* pdf, function* fun){
  
  basic_block bb, b;
  bitmap_head tmp, test;

  bitmap_initialize(&tmp, &bitmap_default_obstack);
  bitmap_initialize(&test, &bitmap_default_obstack);

  FOR_ALL_BB_FN(bb, fun){
    if(bitmap_bit_p(&pdf_set, bb->index)){
      bitmap_copy(&tmp, &test);

      FOR_ALL_BB_FN(b, fun){
        if(bitmap_bit_p(&pdf[bb->index], b->index))
          bitmap_set_bit(&test, b->index);
      }

      bitmap_copy(&test, ipdf);
      bitmap_ior(ipdf, &test, &tmp);
    }
  }

  bitmap_release(&tmp);
  bitmap_release(&test);
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

void yeti_setup(function* fun){
  bitmap_head esg;

  mpi_count_bb(fun);
  mpi_split_bb(fun);
  mpi_tag_bb(fun);
  esg = subgraph(fun);
  calculate_dominance_info(CDI_DOMINATORS);
  calculate_dominance_info(CDI_POST_DOMINATORS);

#ifdef PLOT
  cfgviz_dump(fun, "mpi", 1); 
#endif

  bitmap_release(&esg);
}

void yeti_cleanup(function* fun){
  free_dominance_info(CDI_POST_DOMINATORS);
  free_dominance_info(CDI_DOMINATORS);
  mpi_free_aux(fun);
}

void yeti_warning(mpi_collective_code code, bitmap_head ipdf, 
    bitmap_head set, function* fun){
  basic_block bb; 
  gimple_stmt_iterator gsi;
  gimple* stmt;

  fprintf(stdout, 
      "WARNING(projetCA): in function %s, MPI conflict with %s. Potential forks are at lines [ ", 
      function_name(fun), mpi_collective_name[code]);

  FOR_EACH_BB_FN(bb, fun){
    if(bitmap_bit_p(&ipdf, bb->index)){
      gsi = gsi_start_bb(bb);
      stmt = gsi_stmt(gsi);
      fprintf(stdout, "%d ", gimple_lineno(stmt));
    }
  }

  fprintf(stdout, "].\n");
}

void yeti_core(function* fun){
  char code;
  mpi_collective_code c;
  int rk;
  int max_rank;
  basic_block bb;
  BB_info info;
  bitmap_head* pdf;
  bitmap_head ipdf;
  bitmap_head set;
  bitmap_head pdf_set;

  pdf = post_dominance_frontier(fun);

  max_rank = 0;
  FOR_ALL_BB_FN(bb, fun){
    info = (BB_info) bb->aux;
    if(info->rank > max_rank){
      max_rank = info->rank;
    }
  }

  for(code = 0; 
      code < LAST_AND_UNUSED_MPI_COLLECTIVE_CODE;
      code++){

    c = (mpi_collective_code) code;
    for(rk = 0; rk<max_rank+1; rk++){
      bitmap_initialize(&set, &bitmap_default_obstack);
      bitmap_initialize(&pdf_set, &bitmap_default_obstack);
      bitmap_initialize(&ipdf, &bitmap_default_obstack);

      FOR_EACH_BB_FN(bb, fun){
        info = (BB_info) bb->aux;
        if(info->code == c && info->rank == rk)
          bitmap_set_bit(&set, bb->index);
      }

      set_post_dominance_frontier(&pdf_set, set, pdf, fun);
      it_post_dominance_frontier(&ipdf, pdf_set, pdf, fun);

      /* if pdf_set not empty */
      if(bitmap_count_bits(&pdf_set)){
        yeti_warning(c, pdf_set, set, fun);
      }

      bitmap_release(&set);
      bitmap_release(&pdf_set);
      bitmap_release(&ipdf);
    }
  }

  FOR_ALL_BB_FN(bb, fun){
    bitmap_release(&pdf[bb->index]);
  }

  delete pdf;
}

/* Pragma handling */


void display_monitored_mpi_functions(){
    int i;
    const char* ptr;
    for (i = 0; MPICOLLECTIVES.iterate (i, &ptr); i++){
        printf("%s\n", ptr);
    }
}
void pragma_set_functions_handle_ending_errors(tree x, enum cpp_ttype token, bool close_paren_needed){
    // Check that there is a closing parenthesis
    if (token == CPP_EOF && close_paren_needed){
        fprintf(stderr, "Error: `#pragma ProjetCA mpicoll_check (string[, string]...)` is missing a closing parenthesis\n");
    }
    if (token == CPP_CLOSE_PAREN){
        if(!close_paren_needed){
            // Check that there isn't an unnecessary closing parenthesis
            fprintf(stderr, "Error: `#pragma ProjetCA mpicoll_check string` has an unnecessary closing parenthesis\n");
        }
        else{
            // Check that there is nothing after the closing parenthesis
            token = pragma_lex (&x);
            if(token != CPP_EOF){
                fprintf(stderr, "Error: `#pragma ProjetCA mpicoll_check (string[, string]...)` has extra stuff after the closing parenthesis\n");
            }
        }
    }
}
/* Handle ProjetCA mpicoll_check pragma */
static void handle_pragma_set_functions(cpp_reader *ARG_UNUSED(dummy)){
    if (cfun)
    {
        throw std::logic_error( "#pragma instrument function is not allowed inside functions\n" );
    }

    enum cpp_ttype token;
    tree x;
    bool close_paren_needed = false;
    token = pragma_lex (&x);

    if (token == CPP_OPEN_PAREN)
    {
        close_paren_needed = true;
        token = pragma_lex (&x);
    }
    while (token != CPP_EOF && token != CPP_CLOSE_PAREN){
        if (token == CPP_NAME){
            const char *op = IDENTIFIER_POINTER(x);
            if(MPICOLLECTIVES.contains(op)){
                fprintf(stderr, "Warning: %s is already monitored.\n", op);
            }
            else{
                MPICOLLECTIVES.safe_push(op);
            }
            if(! close_paren_needed){
                token = pragma_lex (&x);
                if(token != CPP_EOF){
                    fprintf(stderr, "Error: `#pragma ProjetCA mpicoll_check string` has extra stuff after the function\n");
                }
                break;
            }
        }
        token = pragma_lex (&x);
    }
    pragma_set_functions_handle_ending_errors(x, token, close_paren_needed);
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
      if(MPICOLLECTIVES.contains(function_name(fun))){
        printf("FUNCTION : %s\n", function_name(fun));
        return true;
      }
      else 
        return false;
    }

    unsigned int execute (function *fun) {
      yeti_setup(fun);

      yeti_core(fun);

      yeti_cleanup(fun);
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

  c_register_pragma("ProjetCA", "mpicoll_check", handle_pragma_set_functions);
 
  return 0;
}

	static char * 
cfgviz_generate_filename( function * fun, const char * suffix )
{
	char * target_filename ; 

	target_filename = (char *)xmalloc( 2048 * sizeof( char ) ) ;

	snprintf( target_filename, 1024, "dot/%s_%s_%d_%s.dot", 
			current_function_name(),
      LOCATION_FILE(fun->function_start_locus)+4, 
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
//    if(bb->aux) printf("N%d - AUX: %d\tcount: %d\n", index,
//        ((BB_info) bb->aux)->code, 
//        ((BB_info) bb->aux)->count);

  FOR_ALL_BB_FN(bb, fun){
    fprintf(out, "\tN%d [label=\"%d :\\n", bb->index, bb->index);
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
