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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_debug.h
 *  \ingroup depsgraph
 */

#ifndef __DEPSGRAPH_DEBUG_H__
#define __DEPSGRAPH_DEBUG_H__

#include "depsgraph_types.h"

extern "C" {
#include "BKE_global.h"
}

struct DepsgraphStats;
struct DepsgraphStatsID;
struct DepsgraphStatsComponent;
struct DepsgraphSettings;
struct EvaluationContext;
struct OperationDepsNode;

struct Depsgraph;

struct DepsgraphDebug {
	static DepsgraphStats *stats;

	static void stats_init();
	static void stats_free();

	static void verify_stats();
	static void reset_stats();

	static void eval_begin(const EvaluationContext *eval_ctx);
	static void eval_end(const EvaluationContext *eval_ctx);
	static void eval_step(const EvaluationContext *eval_ctx,
	                      const char *message);

	static void task_started(Depsgraph *graph, const OperationDepsNode *node);
	static void task_completed(Depsgraph *graph,
	                           const OperationDepsNode *node,
	                           double time);

	static DepsgraphStatsID *get_id_stats(ID *id, bool create);
	static DepsgraphStatsComponent *get_component_stats(DepsgraphStatsID *id_stats,
	                                                    const string &name,
	                                                    bool create);
	static DepsgraphStatsComponent *get_component_stats(ID *id,
	                                                    const string &name,
	                                                    bool create)
	{
		return get_component_stats(get_id_stats(id, create), name, create);
	}
};

#define DEG_DEBUG_PRINTF(...) \
	{ \
		if (G.debug & G_DEBUG_DEPSGRAPH) { \
			fprintf(stderr, __VA_ARGS__); \
		} \
	} \

#endif  /* __DEPSGRAPH_DEBUG_H__ */
