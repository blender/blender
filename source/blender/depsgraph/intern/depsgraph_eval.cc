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

/** \file blender/depsgraph/intern/depsgraph_eval.cc
 *  \ingroup depsgraph
 *
 * Evaluation engine entrypoints for Depsgraph Engine.
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

extern "C" {
#include "DNA_scene_types.h"

#include "BKE_depsgraph.h"
#include "BKE_scene.h"
} /* extern "C" */

#include "DEG_depsgraph.h"

#include "intern/eval/deg_eval.h"
#include "intern/eval/deg_eval_flush.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph.h"

#ifdef WITH_LEGACY_DEPSGRAPH
static bool use_legacy_depsgraph = true;
#endif

/* Unfinished and unused, and takes quite some pre-processing time. */
#undef USE_EVAL_PRIORITY

bool DEG_depsgraph_use_legacy(void)
{
#ifdef DISABLE_NEW_DEPSGRAPH
	return true;
#elif defined(WITH_LEGACY_DEPSGRAPH)
	return use_legacy_depsgraph;
#else
	BLI_assert(!"Should not be used with new depsgraph");
	return false;
#endif
}

void DEG_depsgraph_switch_to_legacy(void)
{
#ifdef WITH_LEGACY_DEPSGRAPH
	use_legacy_depsgraph = true;
#else
	BLI_assert(!"Should not be used with new depsgraph");
#endif
}

void DEG_depsgraph_switch_to_new(void)
{
#ifdef WITH_LEGACY_DEPSGRAPH
	use_legacy_depsgraph = false;
#else
	BLI_assert(!"Should not be used with new depsgraph");
#endif
}

/* ****************** */
/* Evaluation Context */

/* Create new evaluation context. */
EvaluationContext *DEG_evaluation_context_new(int mode)
{
	EvaluationContext *eval_ctx =
		(EvaluationContext *)MEM_callocN(sizeof(EvaluationContext),
		                                 "EvaluationContext");
	eval_ctx->mode = mode;
	return eval_ctx;
}

/**
 * Initialize evaluation context.
 * Used by the areas which currently overrides the context or doesn't have
 * access to a proper one.
 */
void DEG_evaluation_context_init(EvaluationContext *eval_ctx, int mode)
{
	eval_ctx->mode = mode;
}

/* Free evaluation context. */
void DEG_evaluation_context_free(EvaluationContext *eval_ctx)
{
	MEM_freeN(eval_ctx);
}

/* Evaluate all nodes tagged for updating. */
void DEG_evaluate_on_refresh(EvaluationContext *eval_ctx,
                             Depsgraph *graph,
                             Scene *scene)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	/* Update time on primary timesource. */
	DEG::TimeSourceDepsNode *tsrc = deg_graph->find_time_source();
	tsrc->cfra = BKE_scene_frame_get(scene);
	unsigned int layers = deg_graph->layers;
	/* XXX(sergey): This works around missing updates in temp scenes used
	 * by various scripts, but is weak and needs closer investigation.
	 */
	if (layers == 0) {
		layers = scene->lay;
	}
	DEG::deg_evaluate_on_refresh(eval_ctx, deg_graph, layers);
}

/* Frame-change happened for root scene that graph belongs to. */
void DEG_evaluate_on_framechange(EvaluationContext *eval_ctx,
                                 Main *bmain,
                                 Depsgraph *graph,
                                 float ctime,
                                 const unsigned int layers)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	/* Update time on primary timesource. */
	DEG::TimeSourceDepsNode *tsrc = deg_graph->find_time_source();
	tsrc->cfra = ctime;
	tsrc->tag_update(deg_graph);
	DEG::deg_graph_flush_updates(bmain, deg_graph);
	/* Perform recalculation updates. */
	DEG::deg_evaluate_on_refresh(eval_ctx, deg_graph, layers);
}

bool DEG_needs_eval(Depsgraph *graph)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	return BLI_gset_size(deg_graph->entry_tags) != 0;
}
