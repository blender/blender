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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/DEG_depsgraph_physics.h
 *  \ingroup depsgraph
 *
 * Physics utilities for effectors and collision.
 */

#ifndef __DEG_DEPSGRAPH_PHYSICS_H__
#define __DEG_DEPSGRAPH_PHYSICS_H__

#include "DEG_depsgraph.h"

struct Colllection;
struct Depsgraph;
struct ListBase;

#ifdef __cplusplus
extern "C" {
#endif

/* Get collision/effector relations from collection or entire scene. These
 * created during depsgraph relations building and should only be accessed
 * during evaluation. */
struct ListBase *DEG_get_effector_relations(const struct Depsgraph *depsgraph,
                                            struct Collection *collection);
struct ListBase *DEG_get_collision_relations(const struct Depsgraph *depsgraph,
                                             struct Collection *collection);
struct ListBase *DEG_get_smoke_collision_relations(const struct Depsgraph *depsgraph,
                                                   struct Collection *collection);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* __DEG_DEPSGRAPH_PHYSICS_H__ */
