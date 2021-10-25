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

/** \file blender/depsgraph/intern/depsgraph_tag.cc
 *  \ingroup depsgraph
 *
 * Core routines for how the Depsgraph works.
 */

#include <stdio.h>
#include <cstring>  /* required for memset */
#include <queue>

#include "BLI_utildefines.h"
#include "BLI_task.h"
#include "BLI_listbase.h"

extern "C" {
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"


#include "BKE_idcode.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"

#define new new_
#include "BKE_screen.h"
#undef new
} /* extern "C" */

#include "DEG_depsgraph.h"

#include "intern/builder/deg_builder.h"
#include "intern/eval/deg_eval_flush.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

/* *********************** */
/* Update Tagging/Flushing */

/* Legacy depsgraph did some special trickery for things like particle systems
 * when tagging ID for an update. Ideally that tagging needs to become obsolete
 * in favor of havng dedicated node for that which gets tagged, but for until
 * design of those areas is more clear we'll do the same legacy code here.
 *                                                                  - sergey -
 */
#define DEPSGRAPH_USE_LEGACY_TAGGING

namespace {

/* Data-Based Tagging ------------------------------- */

void lib_id_recalc_tag(Main *bmain, ID *id)
{
	id->tag |= LIB_TAG_ID_RECALC;
	DEG_id_type_tag(bmain, GS(id->name));
}

void lib_id_recalc_data_tag(Main *bmain, ID *id)
{
	id->tag |= LIB_TAG_ID_RECALC_DATA;
	DEG_id_type_tag(bmain, GS(id->name));
}

void lib_id_recalc_tag_flag(Main *bmain, ID *id, int flag)
{
	if (flag) {
		/* This bit of code ensures legacy object->recalc flags
		 * are still filled in the same way as it was expected
		 * with the old dependency graph.
		 *
		 * This is because some areas like motion paths and likely
		 * some other physics baking process are doing manual scene
		 * update on all the frames, trying to minimize number of
		 * updates.
		 *
		 * But this flag will also let us to re-construct entry
		 * nodes for update after relations update and after layer
		 * visibility changes.
		 */
		short idtype = GS(id->name);
		if (idtype == ID_OB) {
			Object *object = (Object *)id;
			object->recalc |= (flag & OB_RECALC_ALL);
		}

		if (flag & OB_RECALC_OB)
			lib_id_recalc_tag(bmain, id);
		if (flag & (OB_RECALC_DATA | PSYS_RECALC))
			lib_id_recalc_data_tag(bmain, id);
	}
	else {
		lib_id_recalc_tag(bmain, id);
	}
}

#ifdef DEPSGRAPH_USE_LEGACY_TAGGING
void depsgraph_legacy_handle_update_tag(Main *bmain, ID *id, short flag)
{
	if (flag) {
		Object *object;
		short idtype = GS(id->name);
		if (idtype == ID_PA) {
			ParticleSystem *psys;
			for (object = (Object *)bmain->object.first;
			     object != NULL;
			     object = (Object *)object->id.next)
			{
				for (psys = (ParticleSystem *)object->particlesystem.first;
				     psys != NULL;
				     psys = (ParticleSystem *)psys->next)
				{
					if (&psys->part->id == id) {
						DEG_id_tag_update_ex(bmain, &object->id, flag & OB_RECALC_ALL);
						psys->recalc |= (flag & PSYS_RECALC);
					}
				}
			}
		}
	}
}
#endif

}  /* namespace */

/* Tag all nodes in ID-block for update.
 * This is a crude measure, but is most convenient for old code.
 */
void DEG_graph_id_tag_update(Main *bmain, Depsgraph *graph, ID *id)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	DEG::IDDepsNode *node = deg_graph->find_id_node(id);
	lib_id_recalc_tag(bmain, id);
	if (node != NULL) {
		node->tag_update(deg_graph);
	}
}

/* Tag nodes related to a specific piece of data */
void DEG_graph_data_tag_update(Depsgraph *graph, const PointerRNA *ptr)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	DEG::DepsNode *node = deg_graph->find_node_from_pointer(ptr, NULL);
	if (node != NULL) {
		node->tag_update(deg_graph);
	}
	else {
		printf("Missing node in %s\n", __func__);
		BLI_assert(!"Shouldn't happens since it'll miss crucial update.");
	}
}

/* Tag nodes related to a specific property. */
void DEG_graph_property_tag_update(Depsgraph *graph,
                                   const PointerRNA *ptr,
                                   const PropertyRNA *prop)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	DEG::DepsNode *node = deg_graph->find_node_from_pointer(ptr, prop);
	if (node != NULL) {
		node->tag_update(deg_graph);
	}
	else {
		printf("Missing node in %s\n", __func__);
		BLI_assert(!"Shouldn't happens since it'll miss crucial update.");
	}
}

/* Tag given ID for an update in all the dependency graphs. */
void DEG_id_tag_update(ID *id, short flag)
{
	DEG_id_tag_update_ex(G.main, id, flag);
}

void DEG_id_tag_update_ex(Main *bmain, ID *id, short flag)
{
	if (id == NULL) {
		/* Ideally should not happen, but old depsgraph allowed this. */
		return;
	}
	DEG_DEBUG_PRINTF("%s: id=%s flag=%d\n", __func__, id->name, flag);
	lib_id_recalc_tag_flag(bmain, id, flag);
	for (Scene *scene = (Scene *)bmain->scene.first;
	     scene != NULL;
	     scene = (Scene *)scene->id.next)
	{
		if (scene->depsgraph) {
			Depsgraph *graph = scene->depsgraph;
			if (flag == 0) {
				/* TODO(sergey): Currently blender is still tagging IDs
				 * for recalc just using flag=0. This isn't totally correct
				 * but we'd better deal with such cases and don't fail.
				 */
				DEG_graph_id_tag_update(bmain, graph, id);
				continue;
			}
			if (flag & OB_RECALC_DATA && GS(id->name) == ID_OB) {
				Object *object = (Object *)id;
				if (object->data != NULL) {
					DEG_graph_id_tag_update(bmain,
					                        graph,
					                        (ID *)object->data);
				}
			}
			if (flag & (OB_RECALC_OB | OB_RECALC_DATA)) {
				DEG_graph_id_tag_update(bmain, graph, id);
			}
			else if (flag & OB_RECALC_TIME) {
				DEG_graph_id_tag_update(bmain, graph, id);
			}
		}
	}

#ifdef DEPSGRAPH_USE_LEGACY_TAGGING
	/* Special handling from the legacy depsgraph.
	 * TODO(sergey): Need to get rid of those once all the areas
	 * are re-formulated in terms of franular nodes.
	 */
	depsgraph_legacy_handle_update_tag(bmain, id, flag);
#endif
}

/* Tag given ID type for update. */
void DEG_id_type_tag(Main *bmain, short idtype)
{
	if (idtype == ID_NT) {
		/* Stupid workaround so parent datablocks of nested nodetree get looped
		 * over when we loop over tagged datablock types.
		 */
		DEG_id_type_tag(bmain, ID_MA);
		DEG_id_type_tag(bmain, ID_TE);
		DEG_id_type_tag(bmain, ID_LA);
		DEG_id_type_tag(bmain, ID_WO);
		DEG_id_type_tag(bmain, ID_SCE);
	}

	bmain->id_tag_update[BKE_idcode_to_index(idtype)] = 1;
}

/* Recursively push updates out to all nodes dependent on this,
 * until all affected are tagged and/or scheduled up for eval
 */
void DEG_ids_flush_tagged(Main *bmain)
{
	for (Scene *scene = (Scene *)bmain->scene.first;
	     scene != NULL;
	     scene = (Scene *)scene->id.next)
	{
		DEG_scene_flush_update(bmain, scene);
	}
}

void DEG_scene_flush_update(Main *bmain, Scene *scene)
{
	if (scene->depsgraph == NULL) {
		return;
	}
	DEG::deg_graph_flush_updates(
	        bmain,
	        reinterpret_cast<DEG::Depsgraph *>(scene->depsgraph));
}

/* Update dependency graph when visible scenes/layers changes. */
void DEG_graph_on_visible_update(Main *bmain, Scene *scene)
{
	DEG::Depsgraph *graph = reinterpret_cast<DEG::Depsgraph *>(scene->depsgraph);
	wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
	int old_layers = graph->layers;
	if (wm != NULL) {
		BKE_main_id_tag_listbase(&bmain->scene, LIB_TAG_DOIT, true);
		graph->layers = 0;
		for (wmWindow *win = (wmWindow *)wm->windows.first;
		     win != NULL;
		     win = (wmWindow *)win->next)
		{
			Scene *scene = win->screen->scene;
			if (scene->id.tag & LIB_TAG_DOIT) {
				graph->layers |= BKE_screen_visible_layers(win->screen, scene);
				scene->id.tag &= ~LIB_TAG_DOIT;
			}
		}
	}
	else {
		/* All the layers for background render for now. */
		graph->layers = (1 << 20) - 1;
	}
	if (old_layers != graph->layers) {
		/* Tag all objects which becomes visible (or which becomes needed for dependencies)
		 * for recalc.
		 *
		 * This is mainly needed on file load only, after that updates of invisible objects
		 * will be stored in the pending list.
		 */
		GHASH_FOREACH_BEGIN(DEG::IDDepsNode *, id_node, graph->id_hash)
		{
			ID *id = id_node->id;
			if ((id->tag & LIB_TAG_ID_RECALC_ALL) != 0 ||
			    (id_node->layers & scene->lay_updated) == 0)
			{
				id_node->tag_update(graph);
			}
			/* A bit of magic: if object->recalc is set it means somebody tagged
			 * it for update. If corresponding ID recalc flags are zero it means
			 * graph has been evaluated after that and the recalc was skipped
			 * because of visibility check.
			 */
			if (GS(id->name) == ID_OB) {
				Object *object = (Object *)id;
				if ((id->tag & LIB_TAG_ID_RECALC_ALL) == 0 &&
				    (object->recalc & OB_RECALC_ALL) != 0)
				{
					id_node->tag_update(graph);
					DEG::ComponentDepsNode *anim_comp =
					        id_node->find_component(DEG::DEG_NODE_TYPE_ANIMATION);
					if (anim_comp != NULL && object->recalc & OB_RECALC_TIME) {
						anim_comp->tag_update(graph);
					}
				}
			}
		}
		GHASH_FOREACH_END();
	}
	scene->lay_updated |= graph->layers;
	/* If graph is tagged for update, we don't need to bother with updates here,
	 * nodes will be re-created.
	 */
	if (graph->need_update) {
		return;
	}
	/* Special trick to get local view to work.  */
	LINKLIST_FOREACH (Base *, base, &scene->base) {
		Object *object = base->object;
		DEG::IDDepsNode *id_node = graph->find_id_node(&object->id);
		id_node->layers = 0;
	}
	LINKLIST_FOREACH (Base *, base, &scene->base) {
		Object *object = base->object;
		DEG::IDDepsNode *id_node = graph->find_id_node(&object->id);
		id_node->layers |= base->lay;
		if (object == scene->camera) {
			/* Camera should always be updated, it used directly by viewport. */
			id_node->layers |= (unsigned int)(-1);
		}
	}
	DEG::deg_graph_build_flush_layers(graph);
	LINKLIST_FOREACH (Base *, base, &scene->base) {
		Object *object = base->object;
		DEG::IDDepsNode *id_node = graph->find_id_node(&object->id);
		GHASH_FOREACH_BEGIN(DEG::ComponentDepsNode *, comp, id_node->components)
		{
			id_node->layers |= comp->layers;
		}
		GHASH_FOREACH_END();
	}
}

void DEG_on_visible_update(Main *bmain, const bool UNUSED(do_time))
{
	for (Scene *scene = (Scene *)bmain->scene.first;
	     scene != NULL;
	     scene = (Scene *)scene->id.next)
	{
		if (scene->depsgraph != NULL) {
			DEG_graph_on_visible_update(bmain, scene);
		}
	}
}

/* Check if something was changed in the database and inform
 * editors about this.
 */
void DEG_ids_check_recalc(Main *bmain, Scene *scene, bool time)
{
	ListBase *lbarray[MAX_LIBARRAY];
	int a;
	bool updated = false;

	/* Loop over all ID types. */
	a  = set_listbasepointers(bmain, lbarray);
	while (a--) {
		ListBase *lb = lbarray[a];
		ID *id = (ID *)lb->first;

		if (id && bmain->id_tag_update[BKE_idcode_to_index(GS(id->name))]) {
			updated = true;
			break;
		}
	}

	DEG::deg_editors_scene_update(bmain, scene, (updated || time));
}

void DEG_ids_clear_recalc(Main *bmain)
{
	ListBase *lbarray[MAX_LIBARRAY];
	bNodeTree *ntree;
	int a;

	/* TODO(sergey): Re-implement POST_UPDATE_HANDLER_WORKAROUND using entry_tags
	 * and id_tags storage from the new dependency graph.
	 */

	/* Loop over all ID types. */
	a  = set_listbasepointers(bmain, lbarray);
	while (a--) {
		ListBase *lb = lbarray[a];
		ID *id = (ID *)lb->first;

		if (id && bmain->id_tag_update[BKE_idcode_to_index(GS(id->name))]) {
			for (; id; id = (ID *)id->next) {
				id->tag &= ~(LIB_TAG_ID_RECALC | LIB_TAG_ID_RECALC_DATA);

				/* Some ID's contain semi-datablock nodetree */
				ntree = ntreeFromID(id);
				if (ntree != NULL) {
					ntree->id.tag &= ~(LIB_TAG_ID_RECALC | LIB_TAG_ID_RECALC_DATA);
				}
			}
		}
	}

	memset(bmain->id_tag_update, 0, sizeof(bmain->id_tag_update));
}
