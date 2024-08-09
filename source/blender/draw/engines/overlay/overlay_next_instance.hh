/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_private.hh"

#include "overlay_next_antialiasing.hh"
#include "overlay_next_background.hh"
#include "overlay_next_bounds.hh"
#include "overlay_next_camera.hh"
#include "overlay_next_empty.hh"
#include "overlay_next_force_field.hh"
#include "overlay_next_grid.hh"
#include "overlay_next_lattice.hh"
#include "overlay_next_light.hh"
#include "overlay_next_lightprobe.hh"
#include "overlay_next_mesh.hh"
#include "overlay_next_metaball.hh"
#include "overlay_next_prepass.hh"
#include "overlay_next_relation.hh"
#include "overlay_next_speaker.hh"

namespace blender::draw::overlay {

/**
 * Selection engine reuse most of the Overlay engine by creating selection IDs for each
 * selectable component and using a special shaders for drawing.
 */
class Instance {
  const SelectionType selection_type_;

 public:
  /* WORKAROUND: Legacy. Move to grid pass. */
  GPUUniformBuf *grid_ubo = nullptr;

  ShapeCache shapes;

  /** Global types. */
  Resources resources = {selection_type_,
                         overlay::ShaderModule::module_get(selection_type_, false /*TODO*/)};
  State state;

  /** Overlay types. */
  Background background;

  struct OverlayLayer {
    const SelectionType selection_type_;
    Bounds bounds = {selection_type_};
    Cameras cameras = {selection_type_};
    Empties empties = {selection_type_};
    ForceFields force_fields = {selection_type_};
    Lattices lattices;
    Lights lights = {selection_type_};
    LightProbes light_probes = {selection_type_};
    Metaballs metaballs = {selection_type_};
    Meshes meshes;
    Prepass prepass;
    Relations relations;
    Speakers speakers = {selection_type_};
  } regular{selection_type_}, infront{selection_type_};

  Grid grid;

  AntiAliasing anti_aliasing;

  Instance(const SelectionType selection_type) : selection_type_(selection_type){};

  ~Instance()
  {
    DRW_UBO_FREE_SAFE(grid_ubo);
  }

  void init();
  void begin_sync();
  void object_sync(ObjectRef &ob_ref, Manager &manager);
  void end_sync();
  void draw(Manager &manager);

 private:
  bool object_is_edit_mode(const Object *ob);
};

}  // namespace blender::draw::overlay
