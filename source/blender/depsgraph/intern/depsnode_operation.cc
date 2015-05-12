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

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
} /* extern "C" */

#include "depsnode_operation.h" /* own include */
#include "depsnode_component.h"
#include "depsgraph.h"
#include "depsgraph_intern.h"

/* ******************************************************************* */
/* OpNode Identifiers Array - Exported to other depsgraph files too... */

/* identifiers for operations */
const char *DEG_OPNAMES[] = {
#define DEF_DEG_OPCODE(label) #label,
#include "depsnode_opcodes.h"
#undef DEF_DEG_OPCODE

	"<Invalid>"
};

/* *********** */
/* Inner Nodes */

OperationDepsNode::OperationDepsNode() :
    eval_priority(0.0f),
    flag(0)
{
}

OperationDepsNode::~OperationDepsNode()
{
}

string OperationDepsNode::identifier() const
{
	BLI_assert((opcode > 0) && (opcode < ARRAY_SIZE(DEG_OPNAMES)));
	return string(DEG_OPNAMES[opcode]) + "(" + name + ")";
}

/* Full node identifier, including owner name.
 * used for logging and debug prints.
 */
string OperationDepsNode::full_identifier() const
{
	string owner_str = "";
	if (owner->type == DEPSNODE_TYPE_BONE) {
		owner_str = owner->owner->name + "." + owner->name;
	}
	else {
		owner_str = owner->owner->name;
	}
	return owner_str + "." + identifier();
}

void OperationDepsNode::tag_update(Depsgraph *graph)
{
	if (flag & DEPSOP_FLAG_NEEDS_UPDATE) {
		return;
	}
	/* Tag for update, but also note that this was the source of an update. */
	flag |= (DEPSOP_FLAG_NEEDS_UPDATE | DEPSOP_FLAG_DIRECTLY_MODIFIED);
	graph->add_entry_tag(this);
}

DEG_DEPSNODE_DEFINE(OperationDepsNode, DEPSNODE_TYPE_OPERATION, "Operation");
static DepsNodeFactoryImpl<OperationDepsNode> DNTI_OPERATION;

void DEG_register_operation_depsnodes()
{
	DEG_register_node_typeinfo(&DNTI_OPERATION);
}
