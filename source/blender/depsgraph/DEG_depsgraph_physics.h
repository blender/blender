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
struct DepsNodeHandle;
struct EffectorWeights;
struct ListBase;
struct Object;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ePhysicsRelationType {
	DEG_PHYSICS_EFFECTOR        = 0,
	DEG_PHYSICS_COLLISION       = 1,
	DEG_PHYSICS_SMOKE_COLLISION = 2,
	DEG_PHYSICS_DYNAMIC_BRUSH   = 3,
	DEG_PHYSICS_RELATIONS_NUM   = 4
} ePhysicsRelationType;

/* Get collision/effector relations from collection or entire scene. These
 * created during depsgraph relations building and should only be accessed
 * during evaluation. */
struct ListBase *DEG_get_effector_relations(const struct Depsgraph *depsgraph,
                                            struct Collection *collection);
struct ListBase *DEG_get_collision_relations(const struct Depsgraph *depsgraph,
                                             struct Collection *collection,
                                             unsigned int modifier_type);


/* Build collision/effector relations for depsgraph. */
typedef bool (*DEG_CollobjFilterFunction)(struct Object *obj,
                                          struct ModifierData *md);

void DEG_add_collision_relations(struct DepsNodeHandle *handle,
                                 struct Object *object,
                                 struct Collection *collection,
                                 unsigned int modifier_type,
                                 DEG_CollobjFilterFunction filter_function,
                                 const char *name);
void DEG_add_forcefield_relations(struct DepsNodeHandle *handle,
                                  struct Object *object,
                                  struct EffectorWeights *eff,
                                  bool add_absorption,
                                  int skip_forcefield,
                                  const char *name);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* __DEG_DEPSGRAPH_PHYSICS_H__ */
