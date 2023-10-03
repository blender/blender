/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Physics utilities for effectors and collision.
 */

#pragma once

#include "DEG_depsgraph.hh"

struct Collection;
struct DepsNodeHandle;
struct Depsgraph;
struct EffectorWeights;
struct ListBase;
struct ModifierData;
struct Object;

enum ePhysicsRelationType {
  DEG_PHYSICS_EFFECTOR = 0,
  DEG_PHYSICS_COLLISION = 1,
  DEG_PHYSICS_SMOKE_COLLISION = 2,
  DEG_PHYSICS_DYNAMIC_BRUSH = 3,
  DEG_PHYSICS_RELATIONS_NUM = 4,
};

/* Get collision/effector relations from collection or entire scene. These
 * created during depsgraph relations building and should only be accessed
 * during evaluation. */
ListBase *DEG_get_effector_relations(const Depsgraph *depsgraph, Collection *collection);
ListBase *DEG_get_collision_relations(const Depsgraph *depsgraph,
                                      Collection *collection,
                                      unsigned int modifier_type);

/* Build collision/effector relations for depsgraph. */
using DEG_CollobjFilterFunction = bool (*)(Object *obj, ModifierData *md);

void DEG_add_collision_relations(DepsNodeHandle *handle,
                                 Object *object,
                                 Collection *collection,
                                 unsigned int modifier_type,
                                 DEG_CollobjFilterFunction filter_function,
                                 const char *name);
void DEG_add_forcefield_relations(DepsNodeHandle *handle,
                                  Object *object,
                                  EffectorWeights *eff,
                                  bool add_absorption,
                                  int skip_forcefield,
                                  const char *name);
