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
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph.cc
 *  \ingroup depsgraph
 *
 * Core routines for how the Depsgraph works.
 */

#include "intern/depsgraph.h" /* own include */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"

extern "C" {
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_sequence_types.h"

#include "BKE_depsgraph.h"

#include "RNA_access.h"
}

#include <cstring>

#include "DEG_depsgraph.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

namespace DEG {

static DEG_EditorUpdateIDCb deg_editor_update_id_cb = NULL;
static DEG_EditorUpdateSceneCb deg_editor_update_scene_cb = NULL;
static DEG_EditorUpdateScenePreCb deg_editor_update_scene_pre_cb = NULL;

Depsgraph::Depsgraph()
  : time_source(NULL),
    need_update(false),
    layers(0)
{
	BLI_spin_init(&lock);
	id_hash = BLI_ghash_ptr_new("Depsgraph id hash");
	entry_tags = BLI_gset_ptr_new("Depsgraph entry_tags");
}

Depsgraph::~Depsgraph()
{
	clear_id_nodes();
	BLI_ghash_free(id_hash, NULL, NULL);
	BLI_gset_free(entry_tags, NULL);
	if (time_source != NULL) {
		OBJECT_GUARDED_DELETE(time_source, TimeSourceDepsNode);
	}
	BLI_spin_end(&lock);
}

/* Query Conditions from RNA ----------------------- */

static bool pointer_to_id_node_criteria(const PointerRNA *ptr,
                                        const PropertyRNA *prop,
                                        ID **id)
{
	if (!ptr->type)
		return false;

	if (!prop) {
		if (RNA_struct_is_ID(ptr->type)) {
			*id = (ID *)ptr->data;
			return true;
		}
	}

	return false;
}

static bool pointer_to_component_node_criteria(const PointerRNA *ptr,
                                               const PropertyRNA *prop,
                                               ID **id,
                                               eDepsNode_Type *type,
                                               const char **subdata)
{
	if (!ptr->type)
		return false;

	/* Set default values for returns. */
	*id      = (ID *)ptr->id.data;  /* For obvious reasons... */
	*subdata = "";                 /* Default to no subdata (e.g. bone) name
	                                * lookup in most cases. */

	/* Handling of commonly known scenarios... */
	if (ptr->type == &RNA_PoseBone) {
		bPoseChannel *pchan = (bPoseChannel *)ptr->data;

		/* Bone - generally, we just want the bone component... */
		*type = DEG_NODE_TYPE_BONE;
		*subdata = pchan->name;

		return true;
	}
	else if (ptr->type == &RNA_Bone) {
		Bone *bone = (Bone *)ptr->data;

		/* armature-level bone, but it ends up going to bone component anyway */
		// TODO: the ID in thise case will end up being bArmature, not Object as needed!
		*type = DEG_NODE_TYPE_BONE;
		*subdata = bone->name;
		//*id = ...

		return true;
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
		Object *ob = (Object *)ptr->id.data;
		bConstraint *con = (bConstraint *)ptr->data;

		/* object or bone? */
		if (BLI_findindex(&ob->constraints, con) != -1) {
			/* object transform */
			// XXX: for now, we can't address the specific constraint or the constraint stack...
			*type = DEG_NODE_TYPE_TRANSFORM;
			return true;
		}
		else if (ob->pose) {
			bPoseChannel *pchan;
			for (pchan = (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				if (BLI_findindex(&pchan->constraints, con) != -1) {
					/* bone transforms */
					*type = DEG_NODE_TYPE_BONE;
					*subdata = pchan->name;
					return true;
				}
			}
		}
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_Modifier)) {
		//ModifierData *md = (ModifierData *)ptr->data;

		/* Modifier */
		/* NOTE: subdata is not the same as "operation name",
		 * so although we have unique ops for modifiers,
		 * we can't lump them together
		 */
		*type = DEG_NODE_TYPE_BONE;
		//*subdata = md->name;

		return true;
	}
	else if (ptr->type == &RNA_Object) {
		//Object *ob = (Object *)ptr->data;

		/* Transforms props? */
		if (prop) {
			const char *prop_identifier = RNA_property_identifier((PropertyRNA *)prop);
			/* TODO(sergey): How to optimize this? */
			if (strstr(prop_identifier, "location") ||
			    strstr(prop_identifier, "rotation") ||
			    strstr(prop_identifier, "scale") ||
			    strstr(prop_identifier, "matrix_"))
			{
				*type = DEG_NODE_TYPE_TRANSFORM;
				return true;
			}
			else if (strstr(prop_identifier, "data")) {
				/* We access object.data, most likely a geometry.
				 * Might be a bone tho..
				 */
				*type = DEG_NODE_TYPE_GEOMETRY;
				return true;
			}
		}
	}
	else if (ptr->type == &RNA_ShapeKey) {
		Key *key = (Key *)ptr->id.data;

		/* ShapeKeys are currently handled as geometry on the geometry that owns it */
		*id = key->from; // XXX
		*type = DEG_NODE_TYPE_PARAMETERS;

		return true;
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_Sequence)) {
		Sequence *seq = (Sequence *)ptr->data;
		/* Sequencer strip */
		*type = DEG_NODE_TYPE_SEQUENCER;
		*subdata = seq->name; // xxx?
		return true;
	}

	if (prop) {
		/* All unknown data effectively falls under "parameter evaluation" */
		*type = DEG_NODE_TYPE_PARAMETERS;
		return true;
	}

	return false;
}

/* Convenience wrapper to find node given just pointer + property. */
DepsNode *Depsgraph::find_node_from_pointer(const PointerRNA *ptr,
                                            const PropertyRNA *prop) const
{
	ID *id;
	eDepsNode_Type type;
	const char *name;

	/* Get querying conditions. */
	if (pointer_to_id_node_criteria(ptr, prop, &id)) {
		return find_id_node(id);
	}
	else if (pointer_to_component_node_criteria(ptr, prop, &id, &type, &name)) {
		IDDepsNode *id_node = find_id_node(id);
		if (id_node != NULL) {
			return id_node->find_component(type, name);
		}
	}

	return NULL;
}

/* Node Management ---------------------------- */

static void id_node_deleter(void *value)
{
	IDDepsNode *id_node = reinterpret_cast<IDDepsNode *>(value);
	OBJECT_GUARDED_DELETE(id_node, IDDepsNode);
}

TimeSourceDepsNode *Depsgraph::add_time_source()
{
	if (time_source == NULL) {
		DepsNodeFactory *factory = deg_get_node_factory(DEG_NODE_TYPE_TIMESOURCE);
		time_source = (TimeSourceDepsNode *)factory->create_node(NULL, "", "Time Source");
	}
	return time_source;
}

TimeSourceDepsNode *Depsgraph::find_time_source() const
{
	return time_source;
}

IDDepsNode *Depsgraph::find_id_node(const ID *id) const
{
	return reinterpret_cast<IDDepsNode *>(BLI_ghash_lookup(id_hash, id));
}

IDDepsNode *Depsgraph::add_id_node(ID *id, const char *name)
{
	IDDepsNode *id_node = find_id_node(id);
	if (!id_node) {
		DepsNodeFactory *factory = deg_get_node_factory(DEG_NODE_TYPE_ID_REF);
		id_node = (IDDepsNode *)factory->create_node(id, "", name);
		id->tag |= LIB_TAG_DOIT;
		/* register */
		BLI_ghash_insert(id_hash, id, id_node);
	}
	return id_node;
}

void Depsgraph::clear_id_nodes()
{
	BLI_ghash_clear(id_hash, NULL, id_node_deleter);
}

/* Add new relationship between two nodes. */
DepsRelation *Depsgraph::add_new_relation(OperationDepsNode *from,
                                          OperationDepsNode *to,
                                          const char *description)
{
	/* Create new relation, and add it to the graph. */
	DepsRelation *rel = OBJECT_GUARDED_NEW(DepsRelation, from, to, description);
	/* TODO(sergey): Find a better place for this. */
#ifdef WITH_OPENSUBDIV
	ComponentDepsNode *comp_node = from->owner;
	if (comp_node->type == DEG_NODE_TYPE_GEOMETRY) {
		IDDepsNode *id_to = to->owner->owner;
		IDDepsNode *id_from = from->owner->owner;
		if (id_to != id_from && (id_to->id->tag & LIB_TAG_ID_RECALC_ALL)) {
			if ((id_from->eval_flags & DAG_EVAL_NEED_CPU) == 0) {
				id_from->tag_update(this);
				id_from->eval_flags |= DAG_EVAL_NEED_CPU;
			}
		}
	}
#endif
	return rel;
}

/* Add new relation between two nodes */
DepsRelation *Depsgraph::add_new_relation(DepsNode *from, DepsNode *to,
                                          const char *description)
{
	/* Create new relation, and add it to the graph. */
	DepsRelation *rel = OBJECT_GUARDED_NEW(DepsRelation, from, to, description);
	return rel;
}

/* ************************ */
/* Relationships Management */

DepsRelation::DepsRelation(DepsNode *from,
                           DepsNode *to,
                           const char *description)
  : from(from),
    to(to),
    name(description),
    flag(0)
{
#ifndef NDEBUG
/*
	for (OperationDepsNode::Relations::const_iterator it = from->outlinks.begin();
	     it != from->outlinks.end();
	     ++it)
	{
		DepsRelation *rel = *it;
		if (rel->from == from &&
		    rel->to == to &&
		    rel->type == type &&
		    rel->name == description)
		{
			BLI_assert(!"Duplicated relation, should not happen!");
		}
	}
*/
#endif

	/* Hook it up to the nodes which use it.
	 *
	 * NOTE: We register relation in the nodes which this link connects to here
	 * in constructor but we don't unregister it in the destructor.
	 *
	 * Reasoning:
	 *
	 * - Destructor is currently used on global graph destruction, so there's no
	 *   real need in avoiding dangling pointers, all the memory is to be freed
	 *   anyway.
	 *
	 * - Unregistering relation is not a cheap operation, so better to have it
	 *   as an explicit call if we need this.
	 */
	from->outlinks.push_back(this);
	to->inlinks.push_back(this);
}

DepsRelation::~DepsRelation()
{
	/* Sanity check. */
	BLI_assert(this->from && this->to);
}

/* Low level tagging -------------------------------------- */

/* Tag a specific node as needing updates. */
void Depsgraph::add_entry_tag(OperationDepsNode *node)
{
	/* Sanity check. */
	if (!node)
		return;

	/* Add to graph-level set of directly modified nodes to start searching from.
	 * NOTE: this is necessary since we have several thousand nodes to play with...
	 */
	BLI_gset_insert(entry_tags, node);
}

void Depsgraph::clear_all_nodes()
{
	clear_id_nodes();
	BLI_ghash_clear(id_hash, NULL, NULL);
	if (time_source != NULL) {
		OBJECT_GUARDED_DELETE(time_source, TimeSourceDepsNode);
		time_source = NULL;
	}
}

void deg_editors_id_update(Main *bmain, ID *id)
{
	if (deg_editor_update_id_cb != NULL) {
		deg_editor_update_id_cb(bmain, id);
	}
}

void deg_editors_scene_update(Main *bmain, Scene *scene, bool updated)
{
	if (deg_editor_update_scene_cb != NULL) {
		deg_editor_update_scene_cb(bmain, scene, updated);
	}
}

}  // namespace DEG

/* **************** */
/* Public Graph API */

/* Initialize a new Depsgraph */
Depsgraph *DEG_graph_new()
{
	DEG::Depsgraph *deg_depsgraph = OBJECT_GUARDED_NEW(DEG::Depsgraph);
	return reinterpret_cast<Depsgraph *>(deg_depsgraph);
}

/* Free graph's contents and graph itself */
void DEG_graph_free(Depsgraph *graph)
{
	using DEG::Depsgraph;
	DEG::Depsgraph *deg_depsgraph = reinterpret_cast<DEG::Depsgraph *>(graph);
	OBJECT_GUARDED_DELETE(deg_depsgraph, Depsgraph);
}

/* Set callbacks which are being called when depsgraph changes. */
void DEG_editors_set_update_cb(DEG_EditorUpdateIDCb id_func,
                               DEG_EditorUpdateSceneCb scene_func,
                               DEG_EditorUpdateScenePreCb scene_pre_func)
{
	DEG::deg_editor_update_id_cb = id_func;
	DEG::deg_editor_update_scene_cb = scene_func;
	DEG::deg_editor_update_scene_pre_cb = scene_pre_func;
}

void DEG_editors_update_pre(Main *bmain, Scene *scene, bool time)
{
	if (DEG::deg_editor_update_scene_pre_cb != NULL) {
		DEG::deg_editor_update_scene_pre_cb(bmain, scene, time);
	}
}
