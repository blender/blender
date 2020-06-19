/*
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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */
#pragma once

#include <set>

struct Depsgraph;
struct ModifierData;
struct Object;
struct Scene;

namespace blender {
namespace io {
namespace alembic {

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

  static ModifierData *get_subdiv_modifier(Scene *scene, Object *ob);
};

}  // namespace alembic
}  // namespace io
}  // namespace blender
