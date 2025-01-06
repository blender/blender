/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_private.hh"

#include "overlay_next_antialiasing.hh"
#include "overlay_next_armature.hh"
#include "overlay_next_attribute_text.hh"
#include "overlay_next_attribute_viewer.hh"
#include "overlay_next_axes.hh"
#include "overlay_next_background.hh"
#include "overlay_next_bounds.hh"
#include "overlay_next_camera.hh"
#include "overlay_next_curve.hh"
#include "overlay_next_edit_text.hh"
#include "overlay_next_empty.hh"
#include "overlay_next_facing.hh"
#include "overlay_next_fade.hh"
#include "overlay_next_fluid.hh"
#include "overlay_next_force_field.hh"
#include "overlay_next_grease_pencil.hh"
#include "overlay_next_grid.hh"
#include "overlay_next_lattice.hh"
#include "overlay_next_light.hh"
#include "overlay_next_lightprobe.hh"
#include "overlay_next_mesh.hh"
#include "overlay_next_metaball.hh"
#include "overlay_next_mode_transfer.hh"
#include "overlay_next_motion_path.hh"
#include "overlay_next_name.hh"
#include "overlay_next_origin.hh"
#include "overlay_next_outline.hh"
#include "overlay_next_paint.hh"
#include "overlay_next_particle.hh"
#include "overlay_next_prepass.hh"
#include "overlay_next_relation.hh"
#include "overlay_next_sculpt.hh"
#include "overlay_next_speaker.hh"
#include "overlay_next_wireframe.hh"
#include "overlay_next_xray_fade.hh"

namespace blender::draw::overlay {

/**
 * Selection engine reuse most of the Overlay engine by creating selection IDs for each
 * selectable component and using a special shaders for drawing.
 */
class Instance {
  const SelectionType selection_type_;
  const bool clipping_enabled_;

 public:
  /* WORKAROUND: Legacy. Move to grid pass. */
  GPUUniformBuf *grid_ubo = nullptr;

  ShapeCache shapes;

  /** Global types. */
  Resources resources = {selection_type_,
                         overlay::ShaderModule::module_get(selection_type_, clipping_enabled_),
                         shapes};
  State state;

  /** Overlay types. */
  Background background;
  ImagePrepass image_prepass;
  Origins origins = {selection_type_};
  Outline outline;
  MotionPath motion_paths;

  struct OverlayLayer {
    const SelectionType selection_type_;
    Armatures armatures = {selection_type_};
    AttributeViewer attribute_viewer;
    AttributeTexts attribute_texts;
    Axes axes = {selection_type_};
    Bounds bounds = {selection_type_};
    Cameras cameras = {selection_type_};
    Curves curves;
    EditText edit_text = {selection_type_};
    Empties empties = {selection_type_};
    Facing facing;
    Fade fade;
    Fluids fluids = {selection_type_};
    ForceFields force_fields = {selection_type_};
    GreasePencil grease_pencil;
    Lattices lattices;
    Lights lights = {selection_type_};
    LightProbes light_probes = {selection_type_};
    Meshes meshes;
    MeshUVs mesh_uvs;
    Metaballs metaballs = {selection_type_};
    ModeTransfer mode_transfer;
    Names names;
    Paints paints;
    Particles particles;
    Prepass prepass;
    Relations relations = {selection_type_};
    Sculpts sculpts;
    Speakers speakers = {selection_type_};
    Wireframe wireframe;
  } regular{selection_type_}, infront{selection_type_};

  Grid grid;

  AntiAliasing anti_aliasing;
  XrayFade xray_fade;

  Instance(const SelectionType selection_type, const bool clipping_enabled)
      : selection_type_(selection_type), clipping_enabled_(clipping_enabled){};

  ~Instance()
  {
    GPU_UBO_FREE_SAFE(grid_ubo);
  }

  void init();
  void begin_sync();
  void object_sync(ObjectRef &ob_ref, Manager &manager);
  void end_sync();
  void draw(Manager &manager);

  bool clipping_enabled() const
  {
    return clipping_enabled_;
  }

 private:
  bool object_is_selected(const ObjectRef &ob_ref);
  bool object_is_edit_mode(const Object *object);
  bool object_is_paint_mode(const Object *object);
  bool object_is_particle_edit_mode(const ObjectRef &ob_ref);
  /* Checks for both curve sculpt and regular sculpt mode. */
  bool object_is_sculpt_mode(const ObjectRef &ob_ref);
  /* Checks only for sculpt mode. */
  bool object_is_sculpt_mode(const Object *object);
  /* Any mode that requires to view the object without distraction. */
  bool object_is_edit_paint_mode(const ObjectRef &ob_ref,
                                 bool in_edit_mode,
                                 bool in_paint_mode,
                                 bool in_sculpt_mode);
  bool object_is_in_front(const Object *object, const State &state);
  bool object_needs_prepass(const ObjectRef &ob_ref, bool in_paint_mode);

  /* Returns true if the object is rendered transparent by the render engine.
   * Overlays should not rely on the correct depth being available (and do a depth pre-pass). */
  bool object_is_rendered_transparent(const Object *object, const State &state);

  void draw_node(Manager &manager, View &view);
  void draw_v2d(Manager &manager, View &view);
  void draw_v3d(Manager &manager, View &view);
};

}  // namespace blender::draw::overlay
