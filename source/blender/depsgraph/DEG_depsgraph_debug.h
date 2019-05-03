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

/** \file
 * \ingroup depsgraph
 *
 * Public API for Querying and Filtering Depsgraph
 */

#ifndef __DEG_DEPSGRAPH_DEBUG_H__
#define __DEG_DEPSGRAPH_DEBUG_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct Scene;
struct ViewLayer;

/* ------------------------------------------------ */

/* NOTE: Those flags are same bitmask as G.debug_flags */

void DEG_debug_flags_set(struct Depsgraph *depsgraph, int flags);
int DEG_debug_flags_get(const struct Depsgraph *depsgraph);

void DEG_debug_name_set(struct Depsgraph *depsgraph, const char *name);
const char *DEG_debug_name_get(struct Depsgraph *depsgraph);

/* ------------------------------------------------ */

void DEG_stats_simple(const struct Depsgraph *graph,
                      size_t *r_outer,
                      size_t *r_operations,
                      size_t *r_relations);

/* ************************************************ */
/* Diagram-Based Graph Debugging */

void DEG_debug_relations_graphviz(const struct Depsgraph *graph, FILE *stream, const char *label);

void DEG_debug_stats_gnuplot(const struct Depsgraph *graph,
                             FILE *stream,
                             const char *label,
                             const char *output_filename);

/* ************************************************ */

/* Compare two dependency graphs. */
bool DEG_debug_compare(const struct Depsgraph *graph1, const struct Depsgraph *graph2);

/* Check that dependencies in the graph are really up to date. */
bool DEG_debug_graph_relations_validate(struct Depsgraph *graph,
                                        struct Main *bmain,
                                        struct Scene *scene,
                                        struct ViewLayer *view_layer);

/* Perform consistency check on the graph. */
bool DEG_debug_consistency_check(struct Depsgraph *graph);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __DEG_DEPSGRAPH_DEBUG_H__ */
