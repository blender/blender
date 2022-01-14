/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bli
 * \brief An implementation of the A* (AStar) algorithm to solve shortest path problem.
 */

#include "BLI_utildefines.h"

#include "BLI_bitmap.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */

typedef struct BLI_AStarGNLink {
  int nodes[2];
  float cost;

  void *custom_data;
} BLI_AStarGNLink;

typedef struct BLI_AStarGNode {
  struct ListBase neighbor_links;

  void *custom_data;
} BLI_AStarGNode;

typedef struct BLI_AStarSolution {
  /* Final 'most useful' data. */
  /** Number of steps (i.e. walked links) in path
   * (nodes num, including start and end, is steps + 1). */
  int steps;
  /** Store the path, in reversed order (from destination to source node), as indices. */
  int *prev_nodes;
  /** Indices are nodes' ones, as prev_nodes, but they map to relevant link. */
  BLI_AStarGNLink **prev_links;

  void *custom_data;

  /* Mostly runtime data. */
  BLI_bitmap *done_nodes;
  float *g_costs;
  int *g_steps;

  struct MemArena *mem; /* Memory arena. */
} BLI_AStarSolution;

typedef struct BLI_AStarGraph {
  int node_num;
  BLI_AStarGNode *nodes;

  void *custom_data;

  struct MemArena *mem; /* Memory arena. */
} BLI_AStarGraph;

/**
 * Initialize a node in A* graph.
 *
 * \param custom_data: an opaque pointer attached to this link,
 * available e.g. to cost callback function.
 */
void BLI_astar_node_init(BLI_AStarGraph *as_graph, int node_index, void *custom_data);
/**
 * Add a link between two nodes of our A* graph.
 *
 * \param cost: The 'length' of the link
 * (actual distance between two vertices or face centers e.g.).
 * \param custom_data: An opaque pointer attached to this link,
 * available e.g. to cost callback function.
 */
void BLI_astar_node_link_add(
    BLI_AStarGraph *as_graph, int node1_index, int node2_index, float cost, void *custom_data);
/**
 * \return The index of the other node of given link.
 */
int BLI_astar_node_link_other_node(BLI_AStarGNLink *lnk, int idx);

/**
 * Initialize a solution data for given A* graph. Does not compute anything!
 *
 * \param custom_data: an opaque pointer attached to this link, available e.g
 * . to cost callback function.
 *
 * \note BLI_AStarSolution stores nearly all data needed during solution compute.
 */
void BLI_astar_solution_init(BLI_AStarGraph *as_graph,
                             BLI_AStarSolution *as_solution,
                             void *custom_data);
/**
 * Clear given solution's data, but does not release its memory.
 * Avoids having to recreate/allocate a memarena in loops, e.g.
 *
 * \note This *has to be called* between each path solving.
 */
void BLI_astar_solution_clear(BLI_AStarSolution *as_solution);
/**
 * Release the memory allocated for this solution.
 */
void BLI_astar_solution_free(BLI_AStarSolution *as_solution);

/**
 * Callback computing the current cost (distance) to next node,
 * and the estimated overall cost to destination node
 * (A* expects this estimation to always be less or equal than actual shortest path
 * from next node to destination one).
 *
 * \param link: the graph link between current node and next one.
 * \param node_idx_curr: current node index.
 * \param node_idx_next: next node index.
 * \param node_idx_dst: destination node index.
 */
typedef float (*astar_f_cost)(BLI_AStarGraph *as_graph,
                              BLI_AStarSolution *as_solution,
                              BLI_AStarGNLink *link,
                              int node_idx_curr,
                              int node_idx_next,
                              int node_idx_dst);

/**
 * Initialize an A* graph. Total number of nodes must be known.
 *
 * Nodes might be e.g. vertices, faces, ... etc.
 *
 * \param custom_data: an opaque pointer attached to this link,
 * available e.g. to cost callback function.
 */
void BLI_astar_graph_init(BLI_AStarGraph *as_graph, int node_num, void *custom_data);
void BLI_astar_graph_free(BLI_AStarGraph *as_graph);
/**
 * Solve a path in given graph, using given 'cost' callback function.
 *
 * \param max_steps: maximum number of nodes the found path may have.
 * Useful in performance-critical usages.
 * If no path is found within given steps, returns false too.
 * \return true if a path was found, false otherwise.
 */
bool BLI_astar_graph_solve(BLI_AStarGraph *as_graph,
                           int node_index_src,
                           int node_index_dst,
                           astar_f_cost f_cost_cb,
                           BLI_AStarSolution *r_solution,
                           int max_steps);

#ifdef __cplusplus
}
#endif
