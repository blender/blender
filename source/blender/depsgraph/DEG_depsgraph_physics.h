/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Physics utilities for effectors and collision.
 */

#pragma once

#include "DEG_depsgraph.h"

struct DepsNodeHandle;
struct Depsgraph;
struct EffectorWeights;
struct ListBase;
struct Object;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ePhysicsRelationType {
  DEG_PHYSICS_EFFECTOR = 0,
  DEG_PHYSICS_COLLISION = 1,
  DEG_PHYSICS_SMOKE_COLLISION = 2,
  DEG_PHYSICS_DYNAMIC_BRUSH = 3,
  DEG_PHYSICS_RELATIONS_NUM = 4,
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
typedef bool (*DEG_CollobjFilterFunction)(struct Object *obj, struct ModifierData *md);

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
