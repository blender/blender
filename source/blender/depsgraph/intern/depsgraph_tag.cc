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
#include <cstring>
#include <queue>

extern "C" {
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_task.h"

#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"

#define new new_
#include "BKE_screen.h"
#undef new

#include "DEG_depsgraph.h"
} /* extern "C" */

#include "depsgraph_debug.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"
#include "depsgraph_intern.h"

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
		if (flag & OB_RECALC_DATA)
			lib_id_recalc_data_tag(bmain, id);
	}
	else {
		lib_id_recalc_tag(bmain, id);
	}
}

}  /* namespace */

/* Tag all nodes in ID-block for update.
 * This is a crude measure, but is most convenient for old code.
 */
void DEG_graph_id_tag_update(Main *bmain, Depsgraph *graph, ID *id)
{
	IDDepsNode *node = graph->find_id_node(id);
	lib_id_recalc_tag(bmain, id);
	if (node != NULL) {
		node->tag_update(graph);
	}
}

/* Tag nodes related to a specific piece of data */
void DEG_graph_data_tag_update(Depsgraph *graph, const PointerRNA *ptr)
{
	DepsNode *node = graph->find_node_from_pointer(ptr, NULL);
	if (node) {
		node->tag_update(graph);
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
	DepsNode *node = graph->find_node_from_pointer(ptr, prop);
	if (node) {
		node->tag_update(graph);
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
		}
	}
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
	/* We tag based on first ID type character to avoid
	 * looping over all ID's in case there are no tags.
	 */
	bmain->id_tag_update[((unsigned char *)&idtype)[0]] = 1;
}

/* Update Flushing ---------------------------------- */

/* FIFO queue for tagged nodes that need flushing */
/* XXX This may get a dedicated implementation later if needed - lukas */
typedef std::queue<OperationDepsNode *> FlushQueue;

static void flush_init_func(void *data_v, int i)
{
	/* ID node's done flag is used to avoid multiple editors update
	 * for the same ID.
	 */
	Depsgraph *graph = (Depsgraph *)data_v;
	OperationDepsNode *node = graph->operations[i];
	IDDepsNode *id_node = node->owner->owner;
	id_node->done = 0;
	node->scheduled = false;
	node->owner->flags &= ~DEPSCOMP_FULLY_SCHEDULED;
}

/* Flush updates from tagged nodes outwards until all affected nodes are tagged. */
void DEG_graph_flush_updates(Main *bmain, Depsgraph *graph)
{
	/* sanity check */
	if (graph == NULL)
		return;

	/* Nothing to update, early out. */
	if (graph->entry_tags.size() == 0) {
		return;
	}

	/* TODO(sergey): With a bit of flag magic we can get rid of this
	 * extra loop.
	 */
	const int num_operations = graph->operations.size();
	const bool do_threads = num_operations > 256;
	BLI_task_parallel_range(0, num_operations, graph, flush_init_func, do_threads);

	FlushQueue queue;
	/* Starting from the tagged "entry" nodes, flush outwards... */
	/* NOTE: Also need to ensure that for each of these, there is a path back to
	 *       root, or else they won't be done.
	 * NOTE: Count how many nodes we need to handle - entry nodes may be
	 *       component nodes which don't count for this purpose!
	 */
	for (Depsgraph::EntryTags::const_iterator it = graph->entry_tags.begin();
	     it != graph->entry_tags.end();
	     ++it)
	{
		OperationDepsNode *node = *it;
		IDDepsNode *id_node = node->owner->owner;
		queue.push(node);
		if (id_node->done == 0) {
			deg_editors_id_update(bmain, id_node->id);
			id_node->done = 1;
		}
		node->scheduled = true;
	}

	while (!queue.empty()) {
		OperationDepsNode *node = queue.front();
		queue.pop();

		IDDepsNode *id_node = node->owner->owner;
		lib_id_recalc_tag(bmain, id_node->id);
		/* TODO(sergey): For until we've got proper data nodes in the graph. */
		lib_id_recalc_data_tag(bmain, id_node->id);

		ID *id = id_node->id;
		/* This code is used to preserve those areas which does direct
		 * object update,
		 *
		 * Plus it ensures visibility changes and relations and layers
		 * visibility update has proper flags to work with.
		 */
		if (GS(id->name) == ID_OB) {
			Object *object = (Object *)id;
			ComponentDepsNode *comp_node = node->owner;
			if (comp_node->type == DEPSNODE_TYPE_ANIMATION) {
				object->recalc |= OB_RECALC_TIME;
			}
			else if (comp_node->type == DEPSNODE_TYPE_TRANSFORM) {
				object->recalc |= OB_RECALC_OB;
			}
			else {
				object->recalc |= OB_RECALC_DATA;
			}
		}

		/* Flush to nodes along links... */
		for (OperationDepsNode::Relations::const_iterator it = node->outlinks.begin();
		     it != node->outlinks.end();
		     ++it)
		{
			DepsRelation *rel = *it;
			OperationDepsNode *to_node = (OperationDepsNode *)rel->to;
			if (to_node->scheduled == false) {
				to_node->flag |= DEPSOP_FLAG_NEEDS_UPDATE;
				queue.push(to_node);
				to_node->scheduled = true;
				if (id_node->done == 0) {
					deg_editors_id_update(bmain, id_node->id);
					id_node->done = 1;
				}
			}
		}

		/* TODO(sergey): For until incremental updates are possible
		 * witin a component at least we tag the whole component
		 * for update.
		 */
		ComponentDepsNode *component = node->owner;
		if ((component->flags & DEPSCOMP_FULLY_SCHEDULED) == 0) {
			for (ComponentDepsNode::OperationMap::iterator it = component->operations.begin();
			     it != node->owner->operations.end();
			     ++it)
			{
				OperationDepsNode *op = it->second;
				op->flag |= DEPSOP_FLAG_NEEDS_UPDATE;
			}
			component->flags |= DEPSCOMP_FULLY_SCHEDULED;
		}
	}
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
		/* TODO(sergey): Only visible scenes? */
		if (scene->depsgraph != NULL) {
			DEG_graph_flush_updates(bmain, scene->depsgraph);
		}
	}
}

static void graph_clear_func(void *data_v, int i)
{
	Depsgraph *graph = (Depsgraph *)data_v;
	OperationDepsNode *node = graph->operations[i];
	/* Clear node's "pending update" settings. */
	node->flag &= ~(DEPSOP_FLAG_DIRECTLY_MODIFIED | DEPSOP_FLAG_NEEDS_UPDATE);
}

/* Clear tags from all operation nodes. */
void DEG_graph_clear_tags(Depsgraph *graph)
{
	/* Go over all operation nodes, clearing tags. */
	const int num_operations = graph->operations.size();
	const bool do_threads = num_operations > 256;
	BLI_task_parallel_range(0, num_operations, graph, graph_clear_func, do_threads);
	/* Clear any entry tags which haven't been flushed. */
	graph->entry_tags.clear();
}

/* Update dependency graph when visible scenes/layers changes. */
void DEG_graph_on_visible_update(Main *bmain, Scene *scene)
{
	Depsgraph *graph = scene->depsgraph;
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
		for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin();
		     it != graph->id_hash.end();
		     ++it)
		{
			IDDepsNode *id_node = it->second;
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
					ComponentDepsNode *anim_comp =
					        id_node->find_component(DEPSNODE_TYPE_ANIMATION);
					if (anim_comp != NULL && object->recalc & OB_RECALC_TIME) {
						anim_comp->tag_update(graph);
					}
				}
			}
		}
	}
	scene->lay_updated |= graph->layers;
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

		/* We tag based on first ID type character to avoid
		 * looping over all ID's in case there are no tags.
		 */
		if (id && bmain->id_tag_update[(unsigned char)id->name[0]]) {
			updated = true;
			break;
		}
	}

	deg_editors_scene_update(bmain, scene, (updated || time));
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

		/* We tag based on first ID type character to avoid
		 * looping over all ID's in case there are no tags.
		 */
		if (id && bmain->id_tag_update[(unsigned char)id->name[0]]) {
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
