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

#include "BLI_utildefines.h"

#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/nodes/deg_node_time.h"
#include "intern/depsgraph_intern.h"

#include "util/deg_util_foreach.h"
#include "util/deg_util_function.h"

namespace DEG {

/*******************************************************************************
 * Type information.
 */

DepsNode::TypeInfo::TypeInfo(eDepsNode_Type type,
                             const char *tname,
                             int id_recalc_tag)
        : type(type),
          tname(tname),
          id_recalc_tag(id_recalc_tag)
{
}

/*******************************************************************************
 * Evaluation statistics.
 */

DepsNode::Stats::Stats()
{
	reset();
}

void DepsNode::Stats::reset()
{
	current_time = 0.0;
}

void DepsNode::Stats::reset_current()
{
	current_time = 0.0;
}

/*******************************************************************************
 * Node itself.
 */

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
	return string(nodeTypeAsString(type)) + " : " + name;
}

eDepsNode_Class DepsNode::get_class() const {
	if (type == DEG_NODE_TYPE_OPERATION) {
		return DEG_NODE_CLASS_OPERATION;
	}
	else if (type < DEG_NODE_TYPE_PARAMETERS) {
		return DEG_NODE_CLASS_GENERIC;
	}
	else {
		return DEG_NODE_CLASS_COMPONENT;
	}
}

/*******************************************************************************
 * Generic nodes definition.
 */

DEG_DEPSNODE_DEFINE(TimeSourceDepsNode, DEG_NODE_TYPE_TIMESOURCE, "Time Source");
static DepsNodeFactoryImpl<TimeSourceDepsNode> DNTI_TIMESOURCE;

DEG_DEPSNODE_DEFINE(IDDepsNode, DEG_NODE_TYPE_ID_REF, "ID Node");
static DepsNodeFactoryImpl<IDDepsNode> DNTI_ID_REF;

void deg_register_base_depsnodes()
{
	deg_register_node_typeinfo(&DNTI_TIMESOURCE);
	deg_register_node_typeinfo(&DNTI_ID_REF);
}

}  // namespace DEG
