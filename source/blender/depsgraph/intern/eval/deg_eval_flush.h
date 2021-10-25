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

/** \file blender/depsgraph/intern/eval/deg_eval_flush.h
 *  \ingroup depsgraph
 *
 * Core routines for how the Depsgraph works.
 */

#pragma once

struct Main;

namespace DEG {

struct Depsgraph;

/* Flush updates from tagged nodes outwards until all affected nodes
 * are tagged.
 */
void deg_graph_flush_updates(struct Main *bmain, struct Depsgraph *graph);

/* Clear tags from all operation nodes. */
void deg_graph_clear_tags(struct Depsgraph *graph);

}  // namespace DEG
