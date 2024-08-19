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
#include "overlay_next_facing.hh"
#include "overlay_next_force_field.hh"
#include "overlay_next_grease_pencil.hh"
#include "overlay_next_grid.hh"
#include "overlay_next_lattice.hh"
#include "overlay_next_light.hh"
#include "overlay_next_lightprobe.hh"
#include "overlay_next_mesh.hh"
#include "overlay_next_metaball.hh"
#include "overlay_next_outline.hh"
#include "overlay_next_prepass.hh"
#include "overlay_next_relation.hh"
#include "overlay_next_speaker.hh"
#include "overlay_next_wireframe.hh"

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
  Outline outline;

  struct OverlayLayer {
    const SelectionType selection_type_;
    Bounds bounds = {selection_type_};
    Cameras cameras = {selection_type_};
    Empties empties = {selection_type_};
    Facing facing = {selection_type_};
    ForceFields force_fields = {selection_type_};
    GreasePencil grease_pencil;
    Lattices lattices;
    Lights lights = {selection_type_};
    LightProbes light_probes = {selection_type_};
    Metaballs metaballs = {selection_type_};
    Meshes meshes;
    Prepass prepass = {selection_type_};
    Relations relations;
    Speakers speakers = {selection_type_};
    Wireframe wireframe;
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
  bool object_is_selected(const ObjectRef &ob_ref);
  bool object_is_edit_mode(const Object *object);
  bool object_is_paint_mode(const Object *object);
  /* Checks for both curve sculpt and regular sculpt mode. */
  bool object_is_sculpt_mode(const ObjectRef &ob_ref);
  /* Checks only for sculpt mode. */
  bool object_is_sculpt_mode(const Object *object);
  /* Any mode that requires to view the object without distraction. */
  bool object_is_edit_paint_mode(const ObjectRef &ob_ref,
                                 bool in_edit_mode,
                                 bool in_paint_mode,
                                 bool in_sculpt_mode);
};

}  // namespace blender::draw::overlay
