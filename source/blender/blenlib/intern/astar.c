/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation */

/** \file
 * \ingroup bli
 * \brief An implementation of the A* (AStar) algorithm to solve shortest path problem.
 *
 * This library implements the simple A* (AStar) algorithm, an optimized version of
 * classical dijkstra shortest path solver. The difference is that each future possible
 * path is weighted from its 'shortest' (smallest) possible distance to destination,
 * in addition to distance already walked. This heuristic allows more efficiency
 * in finding optimal path.
 *
 * Implementation based on Wikipedia A* page:
 * https://en.wikipedia.org/wiki/A*_search_algorithm
 *
 * Note that most memory handling here is done through two different MemArena's.
 * Those should also be used to allocate
 * custom data needed to a specific use of A*.
 * The first one, owned by BLI_AStarGraph,
 * is for 'static' data that will live as long as the graph.
 * The second one, owned by BLI_AStarSolution, is for data used during a single path solve.
 * It will be cleared much more often than graph's one.
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#include "BLI_heap_simple.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"

#include "BLI_astar.h"

void BLI_astar_node_init(BLI_AStarGraph *as_graph, const int node_index, void *custom_data)
{
  as_graph->nodes[node_index].custom_data = custom_data;
}

void BLI_astar_node_link_add(BLI_AStarGraph *as_graph,
                             const int node1_index,
                             const int node2_index,
                             const float cost,
                             void *custom_data)
{
  MemArena *mem = as_graph->mem;
  BLI_AStarGNLink *link = BLI_memarena_alloc(mem, sizeof(*link));
  LinkData *ld = BLI_memarena_alloc(mem, sizeof(*ld) * 2);

  link->nodes[0] = node1_index;
  link->nodes[1] = node2_index;
  link->cost = cost;
  link->custom_data = custom_data;

  ld[0].data = ld[1].data = link;

  BLI_addtail(&(as_graph->nodes[node1_index].neighbor_links), &ld[0]);
  BLI_addtail(&(as_graph->nodes[node2_index].neighbor_links), &ld[1]);
}

int BLI_astar_node_link_other_node(BLI_AStarGNLink *lnk, const int idx)
{
  return (lnk->nodes[0] == idx) ? lnk->nodes[1] : lnk->nodes[0];
}

void BLI_astar_solution_init(BLI_AStarGraph *as_graph,
                             BLI_AStarSolution *as_solution,
                             void *custom_data)
{
  MemArena *mem = as_solution->mem;
  size_t node_num = (size_t)as_graph->node_num;

  if (mem == NULL) {
    mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    as_solution->mem = mem;
  }
  /* else memarena should be cleared */

  as_solution->steps = 0;
  as_solution->prev_nodes = BLI_memarena_alloc(mem, sizeof(*as_solution->prev_nodes) * node_num);
  as_solution->prev_links = BLI_memarena_alloc(mem, sizeof(*as_solution->prev_links) * node_num);

  as_solution->custom_data = custom_data;

  as_solution->done_nodes = BLI_BITMAP_NEW_MEMARENA(mem, node_num);
  as_solution->g_costs = BLI_memarena_alloc(mem, sizeof(*as_solution->g_costs) * node_num);
  as_solution->g_steps = BLI_memarena_alloc(mem, sizeof(*as_solution->g_steps) * node_num);
}

void BLI_astar_solution_clear(BLI_AStarSolution *as_solution)
{
  if (as_solution->mem) {
    BLI_memarena_clear(as_solution->mem);
  }

  as_solution->steps = 0;
  as_solution->prev_nodes = NULL;
  as_solution->prev_links = NULL;

  as_solution->custom_data = NULL;

  as_solution->done_nodes = NULL;
  as_solution->g_costs = NULL;
  as_solution->g_steps = NULL;
}

void BLI_astar_solution_free(BLI_AStarSolution *as_solution)
{
  if (as_solution->mem) {
    BLI_memarena_free(as_solution->mem);
    as_solution->mem = NULL;
  }
}

void BLI_astar_graph_init(BLI_AStarGraph *as_graph, const int node_num, void *custom_data)
{
  MemArena *mem = as_graph->mem;

  if (mem == NULL) {
    mem = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    as_graph->mem = mem;
  }
  /* else memarena should be cleared */

  as_graph->node_num = node_num;
  as_graph->nodes = BLI_memarena_calloc(mem, sizeof(*as_graph->nodes) * (size_t)node_num);

  as_graph->custom_data = custom_data;
}

void BLI_astar_graph_free(BLI_AStarGraph *as_graph)
{
  if (as_graph->mem) {
    BLI_memarena_free(as_graph->mem);
    as_graph->mem = NULL;
  }
}

bool BLI_astar_graph_solve(BLI_AStarGraph *as_graph,
                           const int node_index_src,
                           const int node_index_dst,
                           astar_f_cost f_cost_cb,
                           BLI_AStarSolution *r_solution,
                           const int max_steps)
{
  HeapSimple *todo_nodes;

  BLI_bitmap *done_nodes = r_solution->done_nodes;
  int *prev_nodes = r_solution->prev_nodes;
  BLI_AStarGNLink **prev_links = r_solution->prev_links;
  float *g_costs = r_solution->g_costs;
  int *g_steps = r_solution->g_steps;

  r_solution->steps = 0;
  prev_nodes[node_index_src] = -1;
  BLI_bitmap_set_all(done_nodes, false, as_graph->node_num);
  copy_vn_fl(g_costs, as_graph->node_num, FLT_MAX);
  g_costs[node_index_src] = 0.0f;
  g_steps[node_index_src] = 0;

  if (node_index_src == node_index_dst) {
    return true;
  }

  todo_nodes = BLI_heapsimple_new();
  BLI_heapsimple_insert(todo_nodes,
                        f_cost_cb(as_graph, r_solution, NULL, -1, node_index_src, node_index_dst),
                        POINTER_FROM_INT(node_index_src));

  while (!BLI_heapsimple_is_empty(todo_nodes)) {
    const int node_curr_idx = POINTER_AS_INT(BLI_heapsimple_pop_min(todo_nodes));
    BLI_AStarGNode *node_curr = &as_graph->nodes[node_curr_idx];
    LinkData *ld;

    if (BLI_BITMAP_TEST(done_nodes, node_curr_idx)) {
      /* Might happen, because we always add nodes to heap when evaluating them,
       * without ever removing them. */
      continue;
    }

    /* If we are limited in amount of steps to find a path, skip if we reached limit. */
    if (max_steps && g_steps[node_curr_idx] > max_steps) {
      continue;
    }

    if (node_curr_idx == node_index_dst) {
      /* Success! Path found... */
      r_solution->steps = g_steps[node_curr_idx] + 1;

      BLI_heapsimple_free(todo_nodes, NULL);
      return true;
    }

    BLI_BITMAP_ENABLE(done_nodes, node_curr_idx);

    for (ld = node_curr->neighbor_links.first; ld; ld = ld->next) {
      BLI_AStarGNLink *link = ld->data;
      const int node_next_idx = BLI_astar_node_link_other_node(link, node_curr_idx);

      if (!BLI_BITMAP_TEST(done_nodes, node_next_idx)) {
        float g_cst = g_costs[node_curr_idx] + link->cost;

        if (g_cst < g_costs[node_next_idx]) {
          prev_nodes[node_next_idx] = node_curr_idx;
          prev_links[node_next_idx] = link;
          g_costs[node_next_idx] = g_cst;
          g_steps[node_next_idx] = g_steps[node_curr_idx] + 1;
          /* We might have this node already in heap, but since this 'instance'
           * will be evaluated first, no problem. */
          BLI_heapsimple_insert(
              todo_nodes,
              f_cost_cb(as_graph, r_solution, link, node_curr_idx, node_next_idx, node_index_dst),
              POINTER_FROM_INT(node_next_idx));
        }
      }
    }
  }

  BLI_heapsimple_free(todo_nodes, NULL);
  return false;
}
