/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Original Author: Lukas Toenne
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/DEG_depsgraph_debug.h
 *  \ingroup depsgraph
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

/* ------------------------------------------------ */

void DEG_stats_simple(const struct Depsgraph *graph,
                      size_t *r_outer,
                      size_t *r_operations,
                      size_t *r_relations);

/* ************************************************ */
/* Diagram-Based Graph Debugging */

void DEG_debug_relations_graphviz(const struct Depsgraph *graph,
                                  FILE *stream,
                                  const char *label);

void DEG_debug_stats_gnuplot(const struct Depsgraph *graph,
                             FILE *stream,
                             const char *label,
                             const char *output_filename);

/* ************************************************ */

/* Compare two dependency graphs. */
bool DEG_debug_compare(const struct Depsgraph *graph1,
                       const struct Depsgraph *graph2);

/* Check that dependnecies in the graph are really up to date. */
bool DEG_debug_scene_relations_validate(struct Main *bmain,
                                        struct Scene *scene);


/* Perform consistency check on the graph. */
bool DEG_debug_consistency_check(struct Depsgraph *graph);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* __DEG_DEPSGRAPH_DEBUG_H__ */
