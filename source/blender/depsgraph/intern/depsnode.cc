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

#include <stdio.h>
#include <string.h>

#include "BLI_utildefines.h"

extern "C" {
#include "DNA_ID.h"
#include "DNA_anim_types.h"

#include "BKE_animsys.h"

#include "DEG_depsgraph.h"
}

#include "depsnode.h" /* own include */
#include "depsnode_component.h"
#include "depsnode_operation.h"
#include "depsgraph_intern.h"

/* *************** */
/* Node Management */

/* Add ------------------------------------------------ */

DepsNode::TypeInfo::TypeInfo(eDepsNode_Type type, const char *tname)
{
	this->type = type;
	if (type == DEPSNODE_TYPE_OPERATION)
		this->tclass = DEPSNODE_CLASS_OPERATION;
	else if (type < DEPSNODE_TYPE_PARAMETERS)
		this->tclass = DEPSNODE_CLASS_GENERIC;
	else
		this->tclass = DEPSNODE_CLASS_COMPONENT;
	this->tname = tname;
}

DepsNode::DepsNode()
{
	this->name[0] = '\0';
}

DepsNode::~DepsNode()
{
	/* free links
	 * note: deleting relations will remove them from the node relations set,
	 * but only touch the same position as we are using here, which is safe.
	 */
	DEPSNODE_RELATIONS_ITER_BEGIN(this->inlinks, rel)
	{
		OBJECT_GUARDED_DELETE(rel, DepsRelation);
	}
	DEPSNODE_RELATIONS_ITER_END;

	DEPSNODE_RELATIONS_ITER_BEGIN(this->outlinks, rel)
	{
		OBJECT_GUARDED_DELETE(rel, DepsRelation);
	}
	DEPSNODE_RELATIONS_ITER_END;
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
	for (DepsNode::Relations::const_iterator it = outlinks.begin();
	     it != outlinks.end();
	     ++it)
	{
		DepsRelation *rel = *it;
		DepsNode *node = rel->to;
		node->tag_update(graph);
	}
}


/* Root Node ============================================== */

RootDepsNode::~RootDepsNode()
{
	OBJECT_GUARDED_DELETE(time_source, TimeSourceDepsNode);
}

TimeSourceDepsNode *RootDepsNode::add_time_source(const string &name)
{
	if (!time_source) {
		DepsNodeFactory *factory = DEG_get_node_factory(DEPSNODE_TYPE_TIMESOURCE);
		time_source = (TimeSourceDepsNode *)factory->create_node(NULL, "", name);
		/*time_source->owner = this;*/ // XXX
	}
	return time_source;
}

DEG_DEPSNODE_DEFINE(RootDepsNode, DEPSNODE_TYPE_ROOT, "Root DepsNode");
static DepsNodeFactoryImpl<RootDepsNode> DNTI_ROOT;

/* Time Source Node ======================================= */

DEG_DEPSNODE_DEFINE(TimeSourceDepsNode, DEPSNODE_TYPE_TIMESOURCE, "Time Source");
static DepsNodeFactoryImpl<TimeSourceDepsNode> DNTI_TIMESOURCE;

/* ID Node ================================================ */

/* Initialize 'id' node - from pointer data given. */
void IDDepsNode::init(const ID *id, const string &UNUSED(subdata))
{
	/* Store ID-pointer. */
	BLI_assert(id != NULL);
	this->id = (ID *)id;
	this->layers = (1 << 20) - 1;
	this->eval_flags = 0;

	/* NOTE: components themselves are created if/when needed.
	 * This prevents problems with components getting added
	 * twice if an ID-Ref needs to be created to house it...
	 */
}

/* Free 'id' node. */
IDDepsNode::~IDDepsNode()
{
	clear_components();
}

/* Copy 'id' node. */
void IDDepsNode::copy(DepsgraphCopyContext *dcc, const IDDepsNode *src)
{
	(void)src;  /* Ignored. */
	/* Iterate over items in original hash, adding them to new hash. */
	for (IDDepsNode::ComponentMap::const_iterator it = this->components.begin();
	     it != this->components.end();
	     ++it)
	{
		/* Get current <type : component> mapping. */
		ComponentIDKey c_key    = it->first;
		DepsNode *old_component = it->second;

		/* Make a copy of component. */
		ComponentDepsNode *component = (ComponentDepsNode *)DEG_copy_node(dcc, old_component);

		/* Add new node to hash... */
		this->components[c_key] = component;
	}

	// TODO: perform a second loop to fix up links?
	BLI_assert(!"Not expected to be used");
}

ComponentDepsNode *IDDepsNode::find_component(eDepsNode_Type type,
                                              const string &name) const
{
	ComponentIDKey key(type, name);
	ComponentMap::const_iterator it = components.find(key);
	return it != components.end() ? it->second : NULL;
}

ComponentDepsNode *IDDepsNode::add_component(eDepsNode_Type type,
                                             const string &name)
{
	ComponentIDKey key(type, name);
	ComponentDepsNode *comp_node = find_component(type, name);
	if (!comp_node) {
		DepsNodeFactory *factory = DEG_get_node_factory(type);
		comp_node = (ComponentDepsNode *)factory->create_node(this->id, "", name);

		/* Register. */
		this->components[key] = comp_node;
		comp_node->owner = this;
	}
	return comp_node;
}

void IDDepsNode::remove_component(eDepsNode_Type type, const string &name)
{
	ComponentIDKey key(type, name);
	ComponentDepsNode *comp_node = find_component(type, name);
	if (comp_node) {
		/* Unregister. */
		this->components.erase(key);
		OBJECT_GUARDED_DELETE(comp_node, ComponentDepsNode);
	}
}

void IDDepsNode::clear_components()
{
	for (ComponentMap::const_iterator it = components.begin();
	     it != components.end();
	     ++it)
	{
		ComponentDepsNode *comp_node = it->second;
		OBJECT_GUARDED_DELETE(comp_node, ComponentDepsNode);
	}
	components.clear();
}

void IDDepsNode::tag_update(Depsgraph *graph)
{
	for (ComponentMap::const_iterator it = components.begin();
	     it != components.end();
	     ++it)
	{
		ComponentDepsNode *comp_node = it->second;
		/* TODO(sergey): What about drievrs? */
		bool do_component_tag = comp_node->type != DEPSNODE_TYPE_ANIMATION;
		if (comp_node->type == DEPSNODE_TYPE_ANIMATION) {
			AnimData *adt = BKE_animdata_from_id(id);
			BLI_assert(adt != NULL);
			if (adt->recalc & ADT_RECALC_ANIM) {
				do_component_tag = true;
			}
		}
		if (do_component_tag) {
			comp_node->tag_update(graph);
		}
	}
}

DEG_DEPSNODE_DEFINE(IDDepsNode, DEPSNODE_TYPE_ID_REF, "ID Node");
static DepsNodeFactoryImpl<IDDepsNode> DNTI_ID_REF;

/* Subgraph Node ========================================== */

/* Initialize 'subgraph' node - from pointer data given. */
void SubgraphDepsNode::init(const ID *id, const string &UNUSED(subdata))
{
	/* Store ID-ref if provided. */
	this->root_id = (ID *)id;

	/* NOTE: graph will need to be added manually,
	 * as we don't have any way of passing this down.
	 */
}

/* Free 'subgraph' node */
SubgraphDepsNode::~SubgraphDepsNode()
{
	/* Only free if graph not shared, of if this node is the first
	 * reference to it...
	 */
	// XXX: prune these flags a bit...
	if ((this->flag & SUBGRAPH_FLAG_FIRSTREF) || !(this->flag & SUBGRAPH_FLAG_SHARED)) {
		/* Free the referenced graph. */
		DEG_graph_free(this->graph);
		this->graph = NULL;
	}
}

/* Copy 'subgraph' node - Assume that the subgraph doesn't get copied for now... */
void SubgraphDepsNode::copy(DepsgraphCopyContext * /*dcc*/,
                            const SubgraphDepsNode * /*src*/)
{
	//const SubgraphDepsNode *src_node = (const SubgraphDepsNode *)src;
	//SubgraphDepsNode *dst_node       = (SubgraphDepsNode *)dst;

	/* for now, subgraph itself isn't copied... */
	BLI_assert(!"Not expected to be used");
}

DEG_DEPSNODE_DEFINE(SubgraphDepsNode, DEPSNODE_TYPE_SUBGRAPH, "Subgraph Node");
static DepsNodeFactoryImpl<SubgraphDepsNode> DNTI_SUBGRAPH;


void DEG_register_base_depsnodes()
{
	DEG_register_node_typeinfo(&DNTI_ROOT);
	DEG_register_node_typeinfo(&DNTI_TIMESOURCE);

	DEG_register_node_typeinfo(&DNTI_ID_REF);
	DEG_register_node_typeinfo(&DNTI_SUBGRAPH);
}
