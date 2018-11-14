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
#include "BLI_sys_types.h"

namespace DEG {

struct ComponentDepsNode;

typedef uint64_t IDComponentsMask;

/* ID-Block Reference */
struct IDDepsNode : public DepsNode {
	struct ComponentIDKey {
		ComponentIDKey(eDepsNode_Type type, const char *name = "");
		bool operator==(const ComponentIDKey &other) const;

		eDepsNode_Type type;
		const char *name;
	};

	virtual void init(const ID *id, const char *subdata) override;
	void init_copy_on_write(ID *id_cow_hint = NULL);
	~IDDepsNode();
	void destroy();

	virtual string identifier() const override;

	ComponentDepsNode *find_component(eDepsNode_Type type,
	                                  const char *name = "") const;
	ComponentDepsNode *add_component(eDepsNode_Type type,
	                                 const char *name = "");

	virtual void tag_update(Depsgraph *graph, eDepsTag_Source source) override;

	void finalize_build(Depsgraph *graph);

	IDComponentsMask get_visible_components_mask() const;

	/* ID Block referenced. */
	ID *id_orig;
	ID *id_cow;

	/* Hash to make it faster to look up components. */
	GHash *components;

	/* Additional flags needed for scene evaluation.
	 * TODO(sergey): Only needed for until really granular updates
	 * of all the entities.
	 */
	uint32_t eval_flags;
	uint32_t previous_eval_flags;

	eDepsNode_LinkedState_Type linked_state;

	/* Indicates the datablock is visible in the evaluated scene. */
	bool is_directly_visible;

	/* For the collection type of ID, denotes whether collection was fully
	 * recursed into. 	 */
	bool is_collection_fully_expanded;

	IDComponentsMask visible_components_mask;
	IDComponentsMask previously_visible_components_mask;

	DEG_DEPSNODE_DECLARE;
};

}  // namespace DEG
