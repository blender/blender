/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include <set>

struct Depsgraph;
struct ModifierData;
struct Object;
struct Scene;

namespace blender::io::alembic {

/**
 * Temporarily all subdivision modifiers on mesh objects.
 * The destructor restores all disabled modifiers.
 *
 * This is used to export unsubdivided meshes to Alembic. It is done in a separate step before the
 * exporter starts iterating over all the frames, so that it only has to happen once per export.
 */
class SubdivModifierDisabler final {
 private:
  Depsgraph *depsgraph_;
  std::set<ModifierData *> disabled_modifiers_;

 public:
  explicit SubdivModifierDisabler(Depsgraph *depsgraph);
  ~SubdivModifierDisabler();

  void disable_modifiers();

  /**
   * Check if the mesh is a subsurf, ignoring disabled modifiers and
   * displace if it's after subsurf.
   */
  static ModifierData *get_subdiv_modifier(Scene *scene, Object *ob);
};

}  // namespace blender::io::alembic
