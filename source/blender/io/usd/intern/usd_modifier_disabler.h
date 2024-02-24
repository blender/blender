/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.hh"

#include "DNA_modifier_types.h"

#include <set>

struct Depsgraph;
struct ModifierData;
struct Object;
struct Scene;

namespace blender::io::usd {

/**
 * This code is adapted from the SubdivModifierDisabler class
 * implementation in the Alembic exporter.
 *
 * Temporarily disable certain modifiers on mesh objects,
 * depending on the export options.  The destructor restores
 * all disabled modifiers.
 *
 * Currently, this class is used to disable Catmull-Clark subdivision modifiers.
 * It is done in a separate step before the exporter starts iterating over all
 * the frames, so that it only has to happen once per export.
 */
class ModifierDisabler final {
 private:
  Depsgraph *depsgraph_;
  std::set<ModifierData *> disabled_modifiers_;
  std::set<Object *> modified_objects_;
  const USDExportParams &export_params_;

 public:
  explicit ModifierDisabler(Depsgraph *depsgraph, const USDExportParams &export_params);
  ~ModifierDisabler();

  /**
   * Disable modifiers on all mesh objects.
   */
  void disable_modifiers();

  /**
   * Return the Catmull-Clark subsurf modifier on the mesh, if it's the last modifier
   * in the list or if it's the last modifier preceding any displace or
   * particle system modifiers. This function ignores Simple subsurf modifiers.
   */
  static ModifierData *get_subsurf_modifier(Scene *scene, const Object *ob, ModifierMode mode);

  /* Disallow copying. */
  ModifierDisabler(const ModifierDisabler &) = delete;
  ModifierDisabler &operator=(const ModifierDisabler &) = delete;

 private:
  /**
   * Disable the given modifier and add it to the disabled
   * modifiers list.
   */
  void disable_modifier(ModifierData *mod);
};

}  // namespace blender::io::usd
