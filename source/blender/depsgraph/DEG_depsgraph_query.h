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
 */

/** \file blender/depsgraph/DEG_depsgraph_query.h
 *  \ingroup depsgraph
 *
 * Public API for Querying and Filtering Depsgraph.
 */

#ifndef __DEG_DEPSGRAPH_QUERY_H__
#define __DEG_DEPSGRAPH_QUERY_H__

struct ID;

struct Depsgraph;

#ifdef __cplusplus
extern "C" {
#endif

/* Check if given ID type was tagged for update. */
bool DEG_id_type_tagged(struct Main *bmain, short idtype);

/* Get additional evaluation flags for the given ID. */
short DEG_get_eval_flags_for_id(struct Depsgraph *graph, struct ID *id);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* __DEG_DEPSGRAPH_QUERY_H__ */
