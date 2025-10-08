/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_private.hh"

#include "overlay_antialiasing.hh"
#include "overlay_armature.hh"
#include "overlay_attribute_text.hh"
#include "overlay_attribute_viewer.hh"
#include "overlay_axes.hh"
#include "overlay_background.hh"
#include "overlay_bounds.hh"
#include "overlay_camera.hh"
#include "overlay_cursor.hh"
#include "overlay_curve.hh"
#include "overlay_empty.hh"
#include "overlay_facing.hh"
#include "overlay_fade.hh"
#include "overlay_fluid.hh"
#include "overlay_force_field.hh"
#include "overlay_grease_pencil.hh"
#include "overlay_grid.hh"
#include "overlay_lattice.hh"
#include "overlay_light.hh"
#include "overlay_lightprobe.hh"
#include "overlay_mesh.hh"
#include "overlay_metaball.hh"
#include "overlay_mode_transfer.hh"
#include "overlay_motion_path.hh"
#include "overlay_name.hh"
#include "overlay_origin.hh"
#include "overlay_outline.hh"
#include "overlay_paint.hh"
#include "overlay_particle.hh"
#include "overlay_pointcloud.hh"
#include "overlay_prepass.hh"
#include "overlay_relation.hh"
#include "overlay_sculpt.hh"
#include "overlay_speaker.hh"
#include "overlay_text.hh"
#include "overlay_wireframe.hh"
#include "overlay_xray_fade.hh"

namespace blender::draw::overlay {

/**
 * Selection engine reuse most of the Overlay engine by creating selection IDs for each
 * selectable component and using a special shaders for drawing.
 */
class Instance : public DrawEngine {
  const SelectionType selection_type_;
  bool clipping_enabled_;

 public:
  ShapeCache shapes;

  /** Global types. */
  Resources resources = {selection_type_, shapes};
  State state;

  /** Overlay types. */
  Background background;
  ImagePrepass image_prepass;
  Origins origins = {selection_type_};
  Outline outline;
  MotionPath motion_paths;
  Cursor cursor;

  struct OverlayLayer {
    const SelectionType selection_type_;
    Armatures armatures = {selection_type_};
    AttributeViewer attribute_viewer;
    AttributeTexts attribute_texts;
    Axes axes = {selection_type_};
    Bounds bounds = {selection_type_};
    Cameras cameras = {selection_type_};
    Curves curves;
    Text text = {selection_type_};
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
    PointClouds pointclouds;
    Prepass prepass;
    Relations relations = {selection_type_};
    Sculpts sculpts;
    Speakers speakers = {selection_type_};
    Wireframe wireframe;
  } regular{selection_type_}, infront{selection_type_};

  Grid grid;

  AntiAliasing anti_aliasing;
  XrayFade xray_fade;

  Instance() : selection_type_(select::SelectionType::DISABLED) {};
  Instance(const SelectionType selection_type) : selection_type_(selection_type) {};
  ~Instance()
  {
    DRW_text_cache_destroy(state.dt);
  }

  blender::StringRefNull name_get() final
  {
    return "Overlay";
  }

  void init() final;
  void begin_sync() final;
  void object_sync(ObjectRef &ob_ref, Manager &manager) final;
  void end_sync() final;
  void draw(Manager &manager) final;

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

  void draw_text(Framebuffer &framebuffer);

  void ensure_weight_ramp_texture();
};

}  // namespace blender::draw::overlay
