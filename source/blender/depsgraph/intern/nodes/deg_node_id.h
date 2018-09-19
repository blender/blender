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

/** \file depsgraph/intern/nodes/deg_node_id.h
 *  \ingroup depsgraph
 */

#pragma once

#include "intern/nodes/deg_node.h"

namespace DEG {

struct ComponentDepsNode;

/* ID-Block Reference */
struct IDDepsNode : public DepsNode {
	struct ComponentIDKey {
		ComponentIDKey(eDepsNode_Type type, const char *name = "");
		bool operator==(const ComponentIDKey &other) const;

		eDepsNode_Type type;
		const char *name;
	};

	void init(const ID *id, const char *subdata);
	void init_copy_on_write(ID *id_cow_hint = NULL);
	~IDDepsNode();
	void destroy();

	virtual string identifier() const;

	ComponentDepsNode *find_component(eDepsNode_Type type,
	                                  const char *name = "") const;
	ComponentDepsNode *add_component(eDepsNode_Type type,
	                                 const char *name = "");

	void tag_update(Depsgraph *graph);

	void finalize_build(Depsgraph *graph);

	/* ID Block referenced. */
	ID *id_orig;
	ID *id_cow;

	/* Hash to make it faster to look up components. */
	GHash *components;

	/* Additional flags needed for scene evaluation.
	 * TODO(sergey): Only needed for until really granular updates
	 * of all the entities.
	 */
	int eval_flags;

	eDepsNode_LinkedState_Type linked_state;

	/* Indicates the datablock is visible in the evaluated scene. */
	bool is_directly_visible;
	/* Is used to detect when ID becomes visible within a dependency graph,
	 * this value equals to:
	 *   - False if the ID was never inside of the dependency graph.
	 *   - Value if is_visible of ID node from the previous state of the
	 *     dependency graph.
	 */
	bool is_previous_directly_visible;

	DEG_DEPSNODE_DECLARE;
};

}  // namespace DEG
