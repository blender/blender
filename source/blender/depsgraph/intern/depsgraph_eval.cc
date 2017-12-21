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
#include "BKE_scene.h"

#include "DNA_scene_types.h"
} /* extern "C" */

#include "DEG_depsgraph.h"

#include "intern/eval/deg_eval.h"
#include "intern/eval/deg_eval_flush.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/nodes/deg_node_time.h"

#include "intern/depsgraph.h"

/* ****************** */
/* Evaluation Context */

/* Create new evaluation context. */
EvaluationContext *DEG_evaluation_context_new(eEvaluationMode mode)
{
	EvaluationContext *eval_ctx =
		(EvaluationContext *)MEM_callocN(sizeof(EvaluationContext),
		                                 "EvaluationContext");
	DEG_evaluation_context_init(eval_ctx, mode);
	return eval_ctx;
}

/**
 * Initialize evaluation context.
 * Used by the areas which currently overrides the context or doesn't have
 * access to a proper one.
 */
void DEG_evaluation_context_init(EvaluationContext *eval_ctx,
                                 eEvaluationMode mode)
{
	eval_ctx->mode = mode;
}

void DEG_evaluation_context_init_from_scene(EvaluationContext *eval_ctx,
                                            Scene *scene,
                                            ViewLayer *view_layer,
                                            RenderEngineType *engine_type,
                                            eEvaluationMode mode)
{
	DEG_evaluation_context_init(eval_ctx, mode);
	eval_ctx->depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
	eval_ctx->view_layer = view_layer;
	eval_ctx->engine_type = engine_type;
	eval_ctx->ctime = BKE_scene_frame_get(scene);
}

/* Free evaluation context. */
void DEG_evaluation_context_free(EvaluationContext *eval_ctx)
{
	MEM_freeN(eval_ctx);
}

/* Evaluate all nodes tagged for updating. */
void DEG_evaluate_on_refresh(EvaluationContext *eval_ctx,
                             Depsgraph *graph)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	/* Update time on primary timesource. */
	DEG::TimeSourceDepsNode *tsrc = deg_graph->find_time_source();
	tsrc->cfra = BKE_scene_frame_get(deg_graph->scene);
	DEG::deg_evaluate_on_refresh(eval_ctx, deg_graph);
}

/* Frame-change happened for root scene that graph belongs to. */
void DEG_evaluate_on_framechange(EvaluationContext *eval_ctx,
                                 Main *bmain,
                                 Depsgraph *graph,
                                 float ctime)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	/* Update time on primary timesource. */
	DEG::TimeSourceDepsNode *tsrc = deg_graph->find_time_source();
	tsrc->cfra = ctime;
	tsrc->tag_update(deg_graph);
	DEG::deg_graph_flush_updates(bmain, deg_graph);
	/* Perform recalculation updates. */
	DEG::deg_evaluate_on_refresh(eval_ctx, deg_graph);
}

bool DEG_needs_eval(Depsgraph *graph)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	return BLI_gset_size(deg_graph->entry_tags) != 0;
}
