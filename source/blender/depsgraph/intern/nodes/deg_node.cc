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

/** \file blender/depsgraph/intern/nodes/deg_node.cc
 *  \ingroup depsgraph
 */

#include "intern/nodes/deg_node.h"

#include <stdio.h>
#include <cstring>  /* required for STREQ later on. */

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

extern "C" {
#include "DNA_ID.h"
#include "DNA_anim_types.h"

#include "BKE_animsys.h"
}

#include "DEG_depsgraph.h"

#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

namespace DEG {

/* *************** */
/* Node Management */

/* Add ------------------------------------------------ */

DepsNode::TypeInfo::TypeInfo(eDepsNode_Type type, const char *tname)
{
	this->type = type;
	if (type == DEG_NODE_TYPE_OPERATION)
		this->tclass = DEG_NODE_CLASS_OPERATION;
	else if (type < DEG_NODE_TYPE_PARAMETERS)
		this->tclass = DEG_NODE_CLASS_GENERIC;
	else
		this->tclass = DEG_NODE_CLASS_COMPONENT;
	this->tname = tname;
}

DepsNode::DepsNode()
{
	name = "";
}

DepsNode::~DepsNode()
{
	/* Free links. */
	/* NOTE: We only free incoming links. This is to avoid double-free of links
	 * when we're trying to free same link from both it's sides. We don't have
	 * dangling links so this is not a problem from memory leaks point of view.
	 */
	foreach (DepsRelation *rel, inlinks) {
		OBJECT_GUARDED_DELETE(rel, DepsRelation);
	}
}


/* Generic identifier for Depsgraph Nodes. */
string DepsNode::identifier() const
{
	char typebuf[7];
	sprintf(typebuf, "(%d)", type);

	return string(typebuf) + " : " + name;
}

/* ************* */
/* Generic Nodes */

/* Time Source Node ============================================== */

void TimeSourceDepsNode::tag_update(Depsgraph *graph)
{
	foreach (DepsRelation *rel, outlinks) {
		DepsNode *node = rel->to;
		node->tag_update(graph);
	}
}

/* Time Source Node ======================================= */

DEG_DEPSNODE_DEFINE(TimeSourceDepsNode, DEG_NODE_TYPE_TIMESOURCE, "Time Source");
static DepsNodeFactoryImpl<TimeSourceDepsNode> DNTI_TIMESOURCE;

/* ID Node ================================================ */

IDDepsNode::ComponentIDKey::ComponentIDKey(eDepsNode_Type type,
                                           const char *name)
        : type(type), name(name)
{
}

bool IDDepsNode::ComponentIDKey::operator== (const ComponentIDKey &other) const
{
    return type == other.type &&
           STREQ(name, other.name);
}

static unsigned int id_deps_node_hash_key(const void *key_v)
{
	const IDDepsNode::ComponentIDKey *key =
	        reinterpret_cast<const IDDepsNode::ComponentIDKey *>(key_v);
	return BLI_ghashutil_combine_hash(BLI_ghashutil_uinthash(key->type),
	                                  BLI_ghashutil_strhash_p(key->name));
}

static bool id_deps_node_hash_key_cmp(const void *a, const void *b)
{
	const IDDepsNode::ComponentIDKey *key_a =
	        reinterpret_cast<const IDDepsNode::ComponentIDKey *>(a);
	const IDDepsNode::ComponentIDKey *key_b =
	        reinterpret_cast<const IDDepsNode::ComponentIDKey *>(b);
	return !(*key_a == *key_b);
}

static void id_deps_node_hash_key_free(void *key_v)
{
	typedef IDDepsNode::ComponentIDKey ComponentIDKey;
	ComponentIDKey *key = reinterpret_cast<ComponentIDKey *>(key_v);
	OBJECT_GUARDED_DELETE(key, ComponentIDKey);
}

static void id_deps_node_hash_value_free(void *value_v)
{
	ComponentDepsNode *comp_node = reinterpret_cast<ComponentDepsNode *>(value_v);
	OBJECT_GUARDED_DELETE(comp_node, ComponentDepsNode);
}

/* Initialize 'id' node - from pointer data given. */
void IDDepsNode::init(const ID *id, const char *UNUSED(subdata))
{
	/* Store ID-pointer. */
	BLI_assert(id != NULL);
	this->id = (ID *)id;
	this->layers = (1 << 20) - 1;
	this->eval_flags = 0;

	/* For object we initialize layers to layer from base. */
	if (GS(id->name) == ID_OB) {
		this->layers = 0;
	}

	components = BLI_ghash_new(id_deps_node_hash_key,
	                           id_deps_node_hash_key_cmp,
	                           "Depsgraph id components hash");

	/* NOTE: components themselves are created if/when needed.
	 * This prevents problems with components getting added
	 * twice if an ID-Ref needs to be created to house it...
	 */
}

/* Free 'id' node. */
IDDepsNode::~IDDepsNode()
{
	BLI_ghash_free(components,
	               id_deps_node_hash_key_free,
	               id_deps_node_hash_value_free);
}

ComponentDepsNode *IDDepsNode::find_component(eDepsNode_Type type,
                                              const char *name) const
{
	ComponentIDKey key(type, name);
	return reinterpret_cast<ComponentDepsNode *>(BLI_ghash_lookup(components, &key));
}

ComponentDepsNode *IDDepsNode::add_component(eDepsNode_Type type,
                                             const char *name)
{
	ComponentDepsNode *comp_node = find_component(type, name);
	if (!comp_node) {
		DepsNodeFactory *factory = deg_get_node_factory(type);
		comp_node = (ComponentDepsNode *)factory->create_node(this->id, "", name);

		/* Register. */
		ComponentIDKey *key = OBJECT_GUARDED_NEW(ComponentIDKey, type, name);
		BLI_ghash_insert(components, key, comp_node);
		comp_node->owner = this;
	}
	return comp_node;
}

void IDDepsNode::tag_update(Depsgraph *graph)
{
	GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp_node, components)
	{
		/* TODO(sergey): What about drievrs? */
		bool do_component_tag = comp_node->type != DEG_NODE_TYPE_ANIMATION;
		if (comp_node->type == DEG_NODE_TYPE_ANIMATION) {
			AnimData *adt = BKE_animdata_from_id(id);
			/* Animation data might be null if relations are tagged for update. */
			if (adt != NULL && (adt->recalc & ADT_RECALC_ANIM)) {
				do_component_tag = true;
			}
		}
		if (do_component_tag) {
			comp_node->tag_update(graph);
		}
	}
	GHASH_FOREACH_END();
}

void IDDepsNode::finalize_build()
{
	GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp_node, components)
	{
		comp_node->finalize_build();
	}
	GHASH_FOREACH_END();
}

DEG_DEPSNODE_DEFINE(IDDepsNode, DEG_NODE_TYPE_ID_REF, "ID Node");
static DepsNodeFactoryImpl<IDDepsNode> DNTI_ID_REF;

void deg_register_base_depsnodes()
{
	deg_register_node_typeinfo(&DNTI_TIMESOURCE);
	deg_register_node_typeinfo(&DNTI_ID_REF);
}

}  // namespace DEG
