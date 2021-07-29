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

/** \file blender/depsgraph/intern/eval/deg_eval_debug.cc
 *  \ingroup depsgraph
 *
 * Implementation of tools for debugging the depsgraph
 */

#include "intern/eval/deg_eval_debug.h"

#include <cstring>  /* required for STREQ later on. */

#include "BLI_listbase.h"
#include "BLI_ghash.h"

extern "C" {
#include "WM_api.h"
#include "WM_types.h"
}  /* extern "C" */

#include "DEG_depsgraph_debug.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/depsgraph_intern.h"

namespace DEG {

DepsgraphStats *DepsgraphDebug::stats = NULL;

static string get_component_name(eDepsNode_Type type, const char *name = "")
{
	DepsNodeFactory *factory = deg_get_node_factory(type);
	if (name[0] != '\0') {
		return string(factory->tname());
	}
	else {
		return string(factory->tname()) + " | " + name;
	}
}

static void times_clear(DepsgraphStatsTimes &times)
{
	times.duration_last = 0.0f;
}

static void times_add(DepsgraphStatsTimes &times, float time)
{
	times.duration_last += time;
}

void DepsgraphDebug::eval_begin(const EvaluationContext *UNUSED(eval_ctx))
{
	/* TODO(sergey): Stats are currently globally disabled. */
	/* verify_stats(); */
	reset_stats();
}

void DepsgraphDebug::eval_end(const EvaluationContext *UNUSED(eval_ctx))
{
	WM_main_add_notifier(NC_SPACE | ND_SPACE_INFO_REPORT, NULL);
}

void DepsgraphDebug::eval_step(const EvaluationContext *UNUSED(eval_ctx),
                               const char *message)
{
#ifdef DEG_DEBUG_BUILD
	if (deg_debug_eval_cb)
		deg_debug_eval_cb(deg_debug_eval_userdata, message);
#else
	(void)message;  /* Ignored. */
#endif
}

void DepsgraphDebug::task_started(Depsgraph *graph,
                                  const OperationDepsNode *node)
{
	if (stats) {
		BLI_spin_lock(&graph->lock);

		ComponentDepsNode *comp = node->owner;
		ID *id = comp->owner->id;

		DepsgraphStatsID *id_stats = get_id_stats(id, true);
		times_clear(id_stats->times);

		/* XXX TODO use something like: if (id->flag & ID_DEG_DETAILS) {...} */
		if (0) {
			/* XXX component name usage needs cleanup! currently mixes identifier
			 * and description strings!
			 */
			DepsgraphStatsComponent *comp_stats =
			        get_component_stats(id, get_component_name(comp->type,
			                                                   comp->name).c_str(),
			                            true);
			times_clear(comp_stats->times);
		}

		BLI_spin_unlock(&graph->lock);
	}
}

void DepsgraphDebug::task_completed(Depsgraph *graph,
                                    const OperationDepsNode *node,
                                    double time)
{
	if (stats) {
		BLI_spin_lock(&graph->lock);

		ComponentDepsNode *comp = node->owner;
		ID *id = comp->owner->id;

		DepsgraphStatsID *id_stats = get_id_stats(id, true);
		times_add(id_stats->times, time);

		/* XXX TODO use something like: if (id->flag & ID_DEG_DETAILS) {...} */
		if (0) {
			/* XXX component name usage needs cleanup! currently mixes identifier
			 * and description strings!
			 */
			DepsgraphStatsComponent *comp_stats =
			        get_component_stats(id,
			                            get_component_name(comp->type,
			                                               comp->name).c_str(),
			                            true);
			times_add(comp_stats->times, time);
		}

		BLI_spin_unlock(&graph->lock);
	}
}

/* ********** */
/* Statistics */


/* GHash callback */
static void deg_id_stats_free(void *val)
{
	DepsgraphStatsID *id_stats = (DepsgraphStatsID *)val;

	if (id_stats) {
		BLI_freelistN(&id_stats->components);
		MEM_freeN(id_stats);
	}
}

void DepsgraphDebug::stats_init()
{
	if (!stats) {
		stats = (DepsgraphStats *)MEM_callocN(sizeof(DepsgraphStats),
		                                      "Depsgraph Stats");
		stats->id_stats = BLI_ghash_new(BLI_ghashutil_ptrhash,
		                                BLI_ghashutil_ptrcmp,
		                                "Depsgraph ID Stats Hash");
	}
}

void DepsgraphDebug::stats_free()
{
	if (stats) {
		BLI_ghash_free(stats->id_stats, NULL, deg_id_stats_free);
		MEM_freeN(stats);
		stats = NULL;
	}
}

void DepsgraphDebug::verify_stats()
{
	stats_init();
}

void DepsgraphDebug::reset_stats()
{
	if (!stats) {
		return;
	}

	/* XXX this doesn't work, will immediately clear all info,
	 * since most depsgraph updates have none or very few updates to handle.
	 *
	 * Could consider clearing only zero-user ID blocks here
	 */
//	BLI_ghash_clear(stats->id_stats, NULL, deg_id_stats_free);
}

DepsgraphStatsID *DepsgraphDebug::get_id_stats(ID *id, bool create)
{
	DepsgraphStatsID *id_stats = (DepsgraphStatsID *)BLI_ghash_lookup(stats->id_stats, id);

	if (!id_stats && create) {
		id_stats = (DepsgraphStatsID *)MEM_callocN(sizeof(DepsgraphStatsID),
		                                           "Depsgraph ID Stats");
		id_stats->id = id;

		BLI_ghash_insert(stats->id_stats, id, id_stats);
	}

	return id_stats;
}

DepsgraphStatsComponent *DepsgraphDebug::get_component_stats(
        DepsgraphStatsID *id_stats,
        const char *name,
        bool create)
{
	DepsgraphStatsComponent *comp_stats;
	for (comp_stats = (DepsgraphStatsComponent *)id_stats->components.first;
	     comp_stats != NULL;
	     comp_stats = comp_stats->next)
	{
		if (STREQ(comp_stats->name, name)) {
			break;
		}
	}
	if (!comp_stats && create) {
		comp_stats = (DepsgraphStatsComponent *)MEM_callocN(sizeof(DepsgraphStatsComponent),
		                                                    "Depsgraph Component Stats");
		BLI_strncpy(comp_stats->name, name, sizeof(comp_stats->name));
		BLI_addtail(&id_stats->components, comp_stats);
	}
	return comp_stats;
}

}  // namespace DEG
