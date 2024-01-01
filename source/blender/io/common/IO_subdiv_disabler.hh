/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DNA_modifier_types.h"

#include <set>

struct Depsgraph;
struct ModifierData;
struct Object;
struct Scene;

namespace blender::io {

/**
 * This code is shared between the Alembic and USD exporters.
 * Temporarily disable the subdiv modifier on mesh objects,
 * if the subdiv modifier is last on the modifier stack.
 *
 * The destructor restores all disabled modifiers.
 *
 * Currently, this class is used to disable Catmull-Clark subdivision modifiers.
 * It is done in a separate step before the exporter starts iterating over all
 * the frames, so that it only has to happen once per export.
 */
class SubdivModifierDisabler final {
 private:
  Depsgraph *depsgraph_;
  std::set<ModifierData *> disabled_modifiers_;
  std::set<Object *> modified_objects_;

 public:
  explicit SubdivModifierDisabler(Depsgraph *depsgraph);
  ~SubdivModifierDisabler();

  /**
   * Disable subdiv modifiers on all mesh objects.
   */
  void disable_modifiers();

  /**
   * Return the Catmull-Clark subdiv modifier on the mesh, if it's the last modifier
   * in the list or if it's the last modifier preceding any particle system modifiers.
   * This function ignores Simple subdiv modifiers.
   */
  static ModifierData *get_subdiv_modifier(Scene *scene, const Object *ob, ModifierMode mode);

  /* Disallow copying. */
  SubdivModifierDisabler(const SubdivModifierDisabler &) = delete;
  SubdivModifierDisabler &operator=(const SubdivModifierDisabler &) = delete;

 private:
  /**
   * Disable the given modifier and add it to the disabled
   * modifiers list.
   */
  void disable_modifier(ModifierData *mod);
};

}  // namespace blender::io
