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
 *
 * Core routines for how the Depsgraph works
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"

extern "C" {
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_sequence_types.h"

#include "RNA_access.h"
}

#include "DEG_depsgraph.h"
#include "depsgraph.h" /* own include */
#include "depsnode.h"
#include "depsnode_operation.h"
#include "depsnode_component.h"
#include "depsgraph_intern.h"

static DEG_EditorUpdateIDCb deg_editor_update_id_cb = NULL;
static DEG_EditorUpdateSceneCb deg_editor_update_scene_cb = NULL;

Depsgraph::Depsgraph()
  : root_node(NULL),
    need_update(false),
    layers((1 << 20) - 1)
{
	BLI_spin_init(&lock);
}

Depsgraph::~Depsgraph()
{
	/* Free root node - it won't have been freed yet... */
	clear_id_nodes();
	clear_subgraph_nodes();
	if (this->root_node != NULL) {
		OBJECT_GUARDED_DELETE(this->root_node, RootDepsNode);
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
                                               string *subdata)
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
		*type = DEPSNODE_TYPE_BONE;
		*subdata = pchan->name;

		return true;
	}
	else if (ptr->type == &RNA_Bone) {
		Bone *bone = (Bone *)ptr->data;

		/* armature-level bone, but it ends up going to bone component anyway */
		// TODO: the ID in thise case will end up being bArmature, not Object as needed!
		*type = DEPSNODE_TYPE_BONE;
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
			*type = DEPSNODE_TYPE_TRANSFORM;
			return true;
		}
		else if (ob->pose) {
			bPoseChannel *pchan;
			for (pchan = (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				if (BLI_findindex(&pchan->constraints, con) != -1) {
					/* bone transforms */
					*type = DEPSNODE_TYPE_BONE;
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
		*type = DEPSNODE_TYPE_BONE;
		//*subdata = md->name;

		return true;
	}
	else if (ptr->type == &RNA_Object) {
		//Object *ob = (Object *)ptr->data;

		/* Transforms props? */
		if (prop) {
			const char *prop_identifier = RNA_property_identifier((PropertyRNA *)prop);

			if (strstr(prop_identifier, "location") ||
			    strstr(prop_identifier, "rotation") ||
			    strstr(prop_identifier, "scale"))
			{
				*type = DEPSNODE_TYPE_TRANSFORM;
				return true;
			}
		}
		// ...
	}
	else if (ptr->type == &RNA_ShapeKey) {
		Key *key = (Key *)ptr->id.data;

		/* ShapeKeys are currently handled as geometry on the geometry that owns it */
		*id = key->from; // XXX
		*type = DEPSNODE_TYPE_PARAMETERS;

		return true;
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_Sequence)) {
		Sequence *seq = (Sequence *)ptr->data;
		/* Sequencer strip */
		*type = DEPSNODE_TYPE_SEQUENCER;
		*subdata = seq->name; // xxx?
		return true;
	}

	if (prop) {
		/* All unknown data effectively falls under "parameter evaluation" */
		*type = DEPSNODE_TYPE_PARAMETERS;
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
	string name;

	/* Get querying conditions. */
	if (pointer_to_id_node_criteria(ptr, prop, &id)) {
		return find_id_node(id);
	}
	else if (pointer_to_component_node_criteria(ptr, prop, &id, &type, &name)) {
		IDDepsNode *id_node = find_id_node(id);
		if (id_node)
			return id_node->find_component(type, name);
	}

	return NULL;
}

/* Node Management ---------------------------- */

RootDepsNode *Depsgraph::add_root_node()
{
	if (!root_node) {
		DepsNodeFactory *factory = DEG_get_node_factory(DEPSNODE_TYPE_ROOT);
		root_node = (RootDepsNode *)factory->create_node(NULL, "", "Root (Scene)");
	}
	return root_node;
}

TimeSourceDepsNode *Depsgraph::find_time_source(const ID *id) const
{
	/* Search for one attached to a particular ID? */
	if (id) {
		/* Check if it was added as a component
		 * (as may be done for subgraphs needing timeoffset).
		 */
		IDDepsNode *id_node = find_id_node(id);
		if (id_node) {
			// XXX: review this
//			return id_node->find_component(DEPSNODE_TYPE_TIMESOURCE);
		}
		BLI_assert(!"Not implemented yet");
	}
	else {
		/* Use "official" timesource. */
		return root_node->time_source;
	}
	return NULL;
}

SubgraphDepsNode *Depsgraph::add_subgraph_node(const ID *id)
{
	DepsNodeFactory *factory = DEG_get_node_factory(DEPSNODE_TYPE_SUBGRAPH);
	SubgraphDepsNode *subgraph_node =
		(SubgraphDepsNode *)factory->create_node(id, "", id->name + 2);

	/* Add to subnodes list. */
	this->subgraphs.insert(subgraph_node);

	/* if there's an ID associated, add to ID-nodes lookup too */
	if (id) {
#if 0
		/* XXX subgraph node is NOT a true IDDepsNode - what is this supposed to do? */
		// TODO: what to do if subgraph's ID has already been added?
		BLI_assert(!graph->find_id_node(id));
		graph->id_hash[id] = this;
#endif
	}

	return subgraph_node;
}

void Depsgraph::remove_subgraph_node(SubgraphDepsNode *subgraph_node)
{
	subgraphs.erase(subgraph_node);
	OBJECT_GUARDED_DELETE(subgraph_node, SubgraphDepsNode);
}

void Depsgraph::clear_subgraph_nodes()
{
	for (Subgraphs::iterator it = subgraphs.begin();
	     it != subgraphs.end();
	     ++it)
	{
		SubgraphDepsNode *subgraph_node = *it;
		OBJECT_GUARDED_DELETE(subgraph_node, SubgraphDepsNode);
	}
	subgraphs.clear();
}

IDDepsNode *Depsgraph::find_id_node(const ID *id) const
{
	IDNodeMap::const_iterator it = this->id_hash.find(id);
	return it != this->id_hash.end() ? it->second : NULL;
}

IDDepsNode *Depsgraph::add_id_node(ID *id, const string &name)
{
	IDDepsNode *id_node = find_id_node(id);
	if (!id_node) {
		DepsNodeFactory *factory = DEG_get_node_factory(DEPSNODE_TYPE_ID_REF);
		id_node = (IDDepsNode *)factory->create_node(id, "", name);
		id->flag |= LIB_DOIT;
		/* register */
		this->id_hash[id] = id_node;
	}
	return id_node;
}

void Depsgraph::remove_id_node(const ID *id)
{
	IDDepsNode *id_node = find_id_node(id);
	if (id_node) {
		/* unregister */
		this->id_hash.erase(id);
		OBJECT_GUARDED_DELETE(id_node, IDDepsNode);
	}
}

void Depsgraph::clear_id_nodes()
{
	for (IDNodeMap::const_iterator it = id_hash.begin();
	     it != id_hash.end();
	     ++it)
	{
		IDDepsNode *id_node = it->second;
		OBJECT_GUARDED_DELETE(id_node, IDDepsNode);
	}
	id_hash.clear();
}

/* Add new relationship between two nodes. */
DepsRelation *Depsgraph::add_new_relation(OperationDepsNode *from,
                                          OperationDepsNode *to,
                                          eDepsRelation_Type type,
                                          const char *description)
{
	/* Create new relation, and add it to the graph. */
	DepsRelation *rel = OBJECT_GUARDED_NEW(DepsRelation, from, to, type, description);
	return rel;
}

/* Add new relation between two nodes */
DepsRelation *Depsgraph::add_new_relation(DepsNode *from, DepsNode *to,
                                          eDepsRelation_Type type,
                                          const char *description)
{
	/* Create new relation, and add it to the graph. */
	DepsRelation *rel = OBJECT_GUARDED_NEW(DepsRelation, from, to, type, description);
	return rel;
}

/* ************************ */
/* Relationships Management */

DepsRelation::DepsRelation(DepsNode *from,
                           DepsNode *to,
                           eDepsRelation_Type type,
                           const char *description)
  : from(from),
    to(to),
    name(description),
    type(type),
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

	/* Hook it up to the nodes which use it. */
	from->outlinks.insert(this);
	to->inlinks.insert(this);
}

DepsRelation::~DepsRelation()
{
	/* Sanity check. */
	BLI_assert(this->from && this->to);
	/* Remove it from the nodes that use it. */
	this->from->outlinks.erase(this);
	this->to->inlinks.erase(this);
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
	this->entry_tags.insert(node);
}

void Depsgraph::clear_all_nodes()
{
	clear_id_nodes();
	clear_subgraph_nodes();
	id_hash.clear();
	if (this->root_node) {
		OBJECT_GUARDED_DELETE(this->root_node, RootDepsNode);
		root_node = NULL;
	}
}

/* **************** */
/* Public Graph API */

/* Initialize a new Depsgraph */
Depsgraph *DEG_graph_new()
{
	return OBJECT_GUARDED_NEW(Depsgraph);
}

/* Free graph's contents and graph itself */
void DEG_graph_free(Depsgraph *graph)
{
	OBJECT_GUARDED_DELETE(graph, Depsgraph);
}

/* Set callbacks which are being called when depsgraph changes. */
void DEG_editors_set_update_cb(DEG_EditorUpdateIDCb id_func,
                               DEG_EditorUpdateSceneCb scene_func)
{
	deg_editor_update_id_cb = id_func;
	deg_editor_update_scene_cb = scene_func;
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
