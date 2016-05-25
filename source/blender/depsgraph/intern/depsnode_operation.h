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

/** \file blender/depsgraph/intern/depsnode_operation.h
 *  \ingroup depsgraph
 */

#ifndef __DEPSNODE_OPERATION_H__
#define __DEPSNODE_OPERATION_H__

#include "depsnode.h"

struct ID;

struct Depsgraph;
struct DepsgraphCopyContext;

/* Flags for Depsgraph Nodes */
typedef enum eDepsOperation_Flag {
	/* node needs to be updated */
	DEPSOP_FLAG_NEEDS_UPDATE       = (1 << 0),

	/* node was directly modified, causing need for update */
	/* XXX: intention is to make it easier to tell when we just need to take subgraphs */
	DEPSOP_FLAG_DIRECTLY_MODIFIED  = (1 << 1),

	/* Operation is evaluated using CPython; has GIL and security implications... */
	DEPSOP_FLAG_USES_PYTHON   = (1 << 2),
} eDepsOperation_Flag;

/* Atomic Operation - Base type for all operations */
struct OperationDepsNode : public DepsNode {


	OperationDepsNode();
	~OperationDepsNode();

	string identifier() const;
	string full_identifier() const;

	void tag_update(Depsgraph *graph);

	bool is_noop() const { return (bool)evaluate == false; }

	OperationDepsNode *get_entry_operation() { return this; }
	OperationDepsNode *get_exit_operation() { return this; }

	ComponentDepsNode *owner;     /* component that contains the operation */

	DepsEvalOperationCb evaluate; /* callback for operation */


	uint32_t num_links_pending; /* how many inlinks are we still waiting on before we can be evaluated... */
	float eval_priority;
	bool scheduled;

	short optype;                 /* (eDepsOperation_Type) stage of evaluation */
	int   opcode;                 /* (eDepsOperation_Code) identifier for the operation being performed */

	int flag;                     /* (eDepsOperation_Flag) extra settings affecting evaluation */

	/* Extra customdata mask which needs to be evaluated for the object. */
	uint64_t customdata_mask;

	DEG_DEPSNODE_DECLARE;
};

void DEG_register_operation_depsnodes();

#endif  /* __DEPSNODE_OPERATION_H__ */
