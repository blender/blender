/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "DNA_listBase.h"

namespace blender {

struct Collection;
struct CollisionRelation;
struct EffectorRelation;

namespace deg {

struct Depsgraph;

ListBaseT<EffectorRelation> *build_effector_relations(Depsgraph *graph, Collection *collection);
ListBaseT<CollisionRelation> *build_collision_relations(Depsgraph *graph,
                                                        Collection *collection,
                                                        unsigned int modifier_type);
void clear_physics_relations(Depsgraph *graph);

}  // namespace deg
}  // namespace blender
