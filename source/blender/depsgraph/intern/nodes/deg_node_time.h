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

/** \file depsgraph/intern/nodes/deg_node_time.h
 *  \ingroup depsgraph
 */

#pragma once

#include "intern/nodes/deg_node.h"

namespace DEG {

/* Time Source Node. */
struct TimeSourceDepsNode : public DepsNode {
	/* New "current time". */
	float cfra;

	/* time-offset relative to the "official" time source that this one has. */
	float offset;

	// TODO: evaluate() operation needed

	virtual void tag_update(Depsgraph *graph, eDepsTag_Source source) override;

	DEG_DEPSNODE_DECLARE;
};

}  // namespace DEG
