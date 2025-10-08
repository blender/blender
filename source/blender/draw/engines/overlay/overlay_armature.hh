/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "DNA_userdef_types.h"

#include "ED_view3d.hh"

#include "DRW_render.hh"

#include "overlay_base.hh"
#include "overlay_shader_shared.hh"

namespace blender::draw::overlay {
using namespace blender;

enum eArmatureDrawMode {
  ARM_DRAW_MODE_OBJECT,
  ARM_DRAW_MODE_POSE,
  ARM_DRAW_MODE_EDIT,
};

/**
 * Displays armature objects.
 * This includes Object, Edit and Pose mode.
 */
class Armatures : Overlay {
  using EmptyInstanceBuf = ShapeInstanceBuf<ExtraInstanceData>;
  using BoneInstanceBuf = ShapeInstanceBuf<BoneInstanceData>;
  using BoneEnvelopeBuf = ShapeInstanceBuf<BoneEnvelopeData>;
  using BoneStickBuf = ShapeInstanceBuf<BoneStickData>;
  using DegreesOfFreedomBuf = ShapeInstanceBuf<ExtraInstanceData>;

 private:
  const SelectionType selection_type_;

  PassSimple armature_ps_ = {"Armature"};

  /* Force transparent drawing in X-ray mode. */
  bool draw_transparent = false;
  /* Force disable drawing relation is relations are off in viewport. */
  bool show_relations = false;
  /* Show selection state. */
  bool show_outline = false;

  struct BoneBuffers {
    const SelectionType selection_type_;

    /* Bone end points (joints). */
    PassSimple::Sub *sphere_fill = nullptr;
    PassSimple::Sub *sphere_outline = nullptr;
    /* Bone shapes. */
    PassSimple::Sub *shape_fill = nullptr;
    PassSimple::Sub *shape_outline = nullptr;
    /* Custom bone wire-frame. */
    PassSimple::Sub *shape_wire = nullptr;
    PassSimple::Sub *shape_wire_strip = nullptr;
    /* Envelopes. */
    PassSimple::Sub *envelope_fill = nullptr;
    PassSimple::Sub *envelope_outline = nullptr;
    PassSimple::Sub *envelope_distance = nullptr;
    /* Stick bones. */
    PassSimple::Sub *stick = nullptr;
    /* Wire bones. */
    PassSimple::Sub *wire = nullptr;

    /* Bone axes. */
    PassSimple::Sub *arrows = nullptr;
    /* Degrees of freedom. */
    PassSimple::Sub *degrees_of_freedom_fill = nullptr;
    PassSimple::Sub *degrees_of_freedom_wire = nullptr;
    /* Relations. */
    PassSimple::Sub *relations = nullptr;

    BoneInstanceBuf bbones_fill_buf = {selection_type_, "bbones_fill_buf"};
    BoneInstanceBuf bbones_outline_buf = {selection_type_, "bbones_outline_buf"};

    BoneInstanceBuf octahedral_fill_buf = {selection_type_, "octahedral_fill_buf"};
    BoneInstanceBuf octahedral_outline_buf = {selection_type_, "octahedral_outline_buf"};

    BoneInstanceBuf sphere_fill_buf = {selection_type_, "sphere_fill_buf"};
    BoneInstanceBuf sphere_outline_buf = {selection_type_, "sphere_outline_buf"};

    BoneEnvelopeBuf envelope_fill_buf = {selection_type_, "envelope_fill_buf"};
    BoneEnvelopeBuf envelope_outline_buf = {selection_type_, "envelope_outline_buf"};
    BoneEnvelopeBuf envelope_distance_buf = {selection_type_, "envelope_distance_buf"};

    BoneStickBuf stick_buf = {selection_type_, "stick_buf"};

    LinePrimitiveBuf wire_buf = {selection_type_, "wire_buf"};

    EmptyInstanceBuf arrows_buf = {selection_type_, "arrows_buf"};

    DegreesOfFreedomBuf degrees_of_freedom_fill_buf = {SelectionType::DISABLED,
                                                       "degrees_of_freedom_buf"};
    DegreesOfFreedomBuf degrees_of_freedom_wire_buf = {SelectionType::DISABLED,
                                                       "degrees_of_freedom_buf"};

    LinePrimitiveBuf relations_buf = {SelectionType::DISABLED, "relations_buf"};

    Map<gpu::Batch *, std::unique_ptr<BoneInstanceBuf>> custom_shape_fill;
    Map<gpu::Batch *, std::unique_ptr<BoneInstanceBuf>> custom_shape_outline;
    Map<gpu::Batch *, std::unique_ptr<BoneInstanceBuf>> custom_shape_wire;
    Map<gpu::Batch *, std::unique_ptr<BoneInstanceBuf>> custom_shape_wire_strip;

    BoneInstanceBuf &custom_shape_fill_get_buffer(gpu::Batch *geom)
    {
      return *custom_shape_fill.lookup_or_add_cb(geom, [this]() {
        return std::make_unique<BoneInstanceBuf>(this->selection_type_, "CustomBoneSolid");
      });
    }

    BoneInstanceBuf &custom_shape_outline_get_buffer(gpu::Batch *geom)
    {
      return *custom_shape_outline.lookup_or_add_cb(geom, [this]() {
        return std::make_unique<BoneInstanceBuf>(this->selection_type_, "CustomBoneOutline");
      });
    }

    BoneInstanceBuf &custom_shape_wire_get_buffer(gpu::Batch *geom)
    {
      if (geom->prim_type == GPU_PRIM_LINE_STRIP) {
        return *custom_shape_wire_strip.lookup_or_add_cb(geom, [this]() {
          return std::make_unique<BoneInstanceBuf>(this->selection_type_, "CustomBoneWireStrip");
        });
      }
      return *custom_shape_wire.lookup_or_add_cb(geom, [this]() {
        return std::make_unique<BoneInstanceBuf>(this->selection_type_, "CustomBoneWire");
      });
    }

    BoneBuffers(const SelectionType selection_type) : selection_type_(selection_type) {};
  };

  BoneBuffers opaque_ = {selection_type_};
  BoneBuffers transparent_ = {selection_type_};

 public:
  Armatures(const SelectionType selection_type) : selection_type_(selection_type) {};

  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_v3d() && state.show_bones();

    if (!enabled_) {
      return;
    }

    draw_transparent = (state.v3d->shading.type == OB_WIRE) || XRAY_FLAG_ENABLED(state.v3d);
    show_relations = !((state.v3d->flag & V3D_HIDE_HELPLINES) || res.is_selection());
    show_outline = (state.v3d->flag & V3D_SELECT_OUTLINE);

    const bool do_smooth_wire = U.gpu_flag & USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE;
    const float wire_alpha = state.ctx_mode == CTX_MODE_PAINT_WEIGHT ?
                                 state.overlay.bone_wire_alpha :
                                 1.0f;
    /* Draw bone outlines and custom shape wire with a specific alpha. */
    const bool use_wire_alpha = (wire_alpha < 1.0f);

    gpu::Texture **depth_tex = (state.xray_enabled) ? &res.depth_tx : &res.dummy_depth_tx;

    armature_ps_.init();
    armature_ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    armature_ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    res.select_bind(armature_ps_);

    /* Envelope distances and degrees of freedom need to be drawn first as they use additive
     * transparent blending. */
    {
      DRWState transparent_state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL |
                                   DRW_STATE_BLEND_ADD;
      {
        auto &sub = armature_ps_.sub("opaque.envelope_distance");
        sub.state_set(transparent_state | DRW_STATE_CULL_FRONT, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_envelope_fill.get());
        sub.push_constant("alpha", 1.0f);
        sub.push_constant("is_distance", true);
        opaque_.envelope_distance = &sub;
      }
      if (use_wire_alpha) {
        auto &sub = armature_ps_.sub("transparent.envelope_distance");
        sub.state_set(transparent_state | DRW_STATE_CULL_FRONT, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_envelope_fill.get());
        sub.push_constant("alpha", wire_alpha);
        sub.push_constant("is_distance", true);
        transparent_.envelope_distance = &sub;
      }
      else {
        transparent_.envelope_distance = opaque_.envelope_distance;
      }

      {
        auto &sub = armature_ps_.sub("opaque.degrees_of_freedom_fill");
        sub.state_set(transparent_state, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_degrees_of_freedom.get());
        sub.push_constant("alpha", 1.0f);
        opaque_.degrees_of_freedom_fill = &sub;
      }
      if (use_wire_alpha) {
        auto &sub = armature_ps_.sub("transparent.degrees_of_freedom_fill");
        sub.state_set(transparent_state, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_degrees_of_freedom.get());
        sub.push_constant("alpha", wire_alpha);
        transparent_.degrees_of_freedom_fill = &sub;
      }
      else {
        transparent_.degrees_of_freedom_fill = opaque_.degrees_of_freedom_fill;
      }
    }

    DRWState default_state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL |
                             DRW_STATE_WRITE_DEPTH;

    /* Bone Shapes (Octahedral, Box, Custom Shapes, Spheres). */
    {
      {
        auto &sub = armature_ps_.sub("opaque.sphere_fill");
        sub.state_set(default_state, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_sphere_fill.get());
        sub.push_constant("alpha", 1.0f);
        opaque_.sphere_fill = &sub;
      }
      {
        auto &sub = armature_ps_.sub("transparent.sphere_fill");
        sub.state_set((default_state & ~DRW_STATE_WRITE_DEPTH) | DRW_STATE_BLEND_ALPHA,
                      state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_sphere_fill.get());
        sub.push_constant("alpha", wire_alpha * 0.4f);
        transparent_.sphere_fill = &sub;
      }

      {
        auto &sub = armature_ps_.sub("opaque.shape_fill");
        sub.state_set(default_state, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_shape_fill.get());
        sub.push_constant("alpha", 1.0f);
        opaque_.shape_fill = &sub;
      }
      {
        auto &sub = armature_ps_.sub("transparent.shape_fill");
        sub.state_set((default_state & ~DRW_STATE_WRITE_DEPTH) | DRW_STATE_BLEND_ALPHA,
                      state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_shape_fill.get());
        sub.push_constant("alpha", wire_alpha * 0.6f);
        transparent_.shape_fill = &sub;
      }

      {
        auto &sub = armature_ps_.sub("opaque.sphere_outline");
        sub.state_set(default_state, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_sphere_outline.get());
        sub.push_constant("alpha", 1.0f);
        opaque_.sphere_outline = &sub;
      }
      if (use_wire_alpha) {
        auto &sub = armature_ps_.sub("transparent.sphere_outline");
        sub.state_set(default_state | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_sphere_outline.get());
        sub.push_constant("alpha", wire_alpha);
        transparent_.sphere_outline = &sub;
      }
      else {
        transparent_.sphere_outline = opaque_.sphere_outline;
      }

      {
        auto &sub = armature_ps_.sub("opaque.shape_outline");
        sub.state_set(default_state, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_shape_outline.get());
        sub.push_constant("alpha", 1.0f);
        opaque_.shape_outline = &sub;
      }
      if (use_wire_alpha) {
        auto &sub = armature_ps_.sub("transparent.shape_outline");
        sub.state_set(default_state | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_shape_outline.get());
        sub.bind_texture("depth_tx", depth_tex);
        sub.push_constant("alpha", wire_alpha * 0.6f);
        sub.push_constant("do_smooth_wire", do_smooth_wire);
        transparent_.shape_outline = &sub;
      }
      else {
        transparent_.shape_outline = opaque_.shape_outline;
      }

      {
        auto &sub = armature_ps_.sub("opaque.shape_wire");
        sub.state_set(default_state | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_shape_wire.get());
        sub.push_constant("alpha", 1.0f);
        sub.push_constant("do_smooth_wire", do_smooth_wire);
        sub.push_constant("use_arrow_drawing", false);
        opaque_.shape_wire = &sub;
      }
      if (use_wire_alpha) {
        auto &sub = armature_ps_.sub("transparent.shape_wire");
        sub.state_set(default_state | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_shape_wire.get());
        sub.bind_texture("depth_tx", depth_tex);
        sub.push_constant("alpha", wire_alpha * 0.6f);
        sub.push_constant("do_smooth_wire", do_smooth_wire);
        sub.push_constant("use_arrow_drawing", false);
        transparent_.shape_wire = &sub;
      }
      else {
        transparent_.shape_wire = opaque_.shape_wire;
      }

      {
        auto &sub = armature_ps_.sub("opaque.shape_wire_strip");
        sub.state_set(default_state | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_shape_wire_strip.get());
        sub.push_constant("alpha", 1.0f);
        sub.push_constant("do_smooth_wire", do_smooth_wire);
        sub.push_constant("use_arrow_drawing", false);
        opaque_.shape_wire_strip = &sub;
      }
      if (use_wire_alpha) {
        auto &sub = armature_ps_.sub("transparent.shape_wire_strip");
        sub.state_set(default_state | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_shape_wire_strip.get());
        sub.bind_texture("depth_tx", depth_tex);
        sub.push_constant("alpha", wire_alpha * 0.6f);
        sub.push_constant("do_smooth_wire", do_smooth_wire);
        sub.push_constant("use_arrow_drawing", false);
        transparent_.shape_wire_strip = &sub;
      }
      else {
        transparent_.shape_wire_strip = opaque_.shape_wire_strip;
      }
    }
    /* Degrees of freedom. */
    {
      {
        auto &sub = armature_ps_.sub("opaque.degrees_of_freedom_wire");
        sub.shader_set(res.shaders->armature_degrees_of_freedom.get());
        sub.push_constant("alpha", 1.0f);
        opaque_.degrees_of_freedom_wire = &sub;
      }
      if (use_wire_alpha) {
        auto &sub = armature_ps_.sub("transparent.degrees_of_freedom_wire");
        sub.shader_set(res.shaders->armature_degrees_of_freedom.get());
        sub.push_constant("alpha", wire_alpha);
        transparent_.degrees_of_freedom_wire = &sub;
      }
      else {
        transparent_.degrees_of_freedom_wire = opaque_.degrees_of_freedom_wire;
      }
    }
    /* Stick bones. */
    {
      {
        auto &sub = armature_ps_.sub("opaque.stick");
        sub.shader_set(res.shaders->armature_stick.get());
        sub.push_constant("alpha", 1.0f);
        opaque_.stick = &sub;
      }
      if (use_wire_alpha) {
        auto &sub = armature_ps_.sub("transparent.stick");
        sub.state_set(default_state | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_stick.get());
        sub.push_constant("alpha", wire_alpha);
        transparent_.stick = &sub;
      }
      else {
        transparent_.stick = opaque_.stick;
      }
    }
    /* Envelopes. */
    {
      {
        auto &sub = armature_ps_.sub("opaque.envelope_fill");
        sub.state_set(default_state | DRW_STATE_CULL_BACK, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_envelope_fill.get());
        sub.push_constant("is_distance", false);
        sub.push_constant("alpha", 1.0f);
        opaque_.envelope_fill = &sub;
      }
      {
        auto &sub = armature_ps_.sub("transparent.envelope_fill");
        sub.state_set((default_state & ~DRW_STATE_WRITE_DEPTH) |
                          (DRW_STATE_BLEND_ALPHA | DRW_STATE_CULL_BACK),
                      state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_envelope_fill.get());
        sub.push_constant("alpha", wire_alpha * 0.6f);
        transparent_.envelope_fill = &sub;
      }

      {
        auto &sub = armature_ps_.sub("opaque.envelope_outline");
        sub.state_set(default_state | DRW_STATE_CULL_BACK, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_envelope_outline.get());
        sub.push_constant("alpha", 1.0f);
        opaque_.envelope_outline = &sub;
      }
      if (use_wire_alpha) {
        auto &sub = armature_ps_.sub("transparent.envelope_outline");
        sub.state_set((default_state & ~DRW_STATE_WRITE_DEPTH) |
                          (DRW_STATE_BLEND_ALPHA | DRW_STATE_CULL_BACK),
                      state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_envelope_outline.get());
        sub.push_constant("alpha", wire_alpha);
        transparent_.envelope_outline = &sub;
      }
      else {
        transparent_.envelope_outline = opaque_.envelope_outline;
      }
    }
    {
      {
        auto &sub = armature_ps_.sub("opaque.wire");
        sub.shader_set(res.shaders->armature_wire.get());
        sub.push_constant("alpha", 1.0f);
        opaque_.wire = &sub;
      }
      if (use_wire_alpha) {
        auto &sub = armature_ps_.sub("transparent.wire");
        sub.state_set(default_state | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
        sub.shader_set(res.shaders->armature_wire.get());
        sub.push_constant("alpha", wire_alpha);
        transparent_.wire = &sub;
      }
      else {
        transparent_.wire = opaque_.wire;
      }
    }

    {
      auto &sub = armature_ps_.sub("opaque.arrow");
      sub.shader_set(res.shaders->extra_shape.get());
      opaque_.arrows = &sub;
      transparent_.arrows = opaque_.arrows;
    }

    {
      auto &sub = armature_ps_.sub("opaque.relations");
      sub.shader_set(res.shaders->extra_wire.get());
      opaque_.relations = &sub;
      transparent_.relations = opaque_.relations;
    }

    auto shape_instance_bufs_begin_sync = [](BoneBuffers &bb) {
      bb.envelope_fill_buf.clear();
      bb.envelope_outline_buf.clear();
      bb.envelope_distance_buf.clear();
      bb.bbones_fill_buf.clear();
      bb.bbones_outline_buf.clear();
      bb.octahedral_fill_buf.clear();
      bb.octahedral_outline_buf.clear();
      bb.sphere_fill_buf.clear();
      bb.sphere_outline_buf.clear();
      bb.stick_buf.clear();
      bb.wire_buf.clear();
      bb.arrows_buf.clear();
      bb.degrees_of_freedom_fill_buf.clear();
      bb.degrees_of_freedom_wire_buf.clear();
      bb.relations_buf.clear();
      /* TODO(fclem): Potentially expensive operation recreating a lot of gpu buffers.
       * Prefer a pruning strategy. */
      bb.custom_shape_fill.clear();
      bb.custom_shape_outline.clear();
      bb.custom_shape_wire.clear();
      bb.custom_shape_wire_strip.clear();
    };

    shape_instance_bufs_begin_sync(transparent_);
    shape_instance_bufs_begin_sync(opaque_);
  }

  struct DrawContext {
    /* Current armature object */
    Object *ob = nullptr;
    const ObjectRef *ob_ref = nullptr;

    /* Note: can be mutated inside `draw_armature_pose()`. */
    eArmatureDrawMode draw_mode = ARM_DRAW_MODE_OBJECT;
    eArmature_Drawtype drawtype = ARM_DRAW_TYPE_OCTA;

    Armatures::BoneBuffers *bone_buf = nullptr;
    Resources *res = nullptr;
    DRWTextStore *dt = nullptr;

    /* Not a theme, this is an override. */
    const float *const_color = nullptr;
    /* Wire thickness. */
    float const_wire = 0.0f;

    bool do_relations = false;
    bool transparent = false;
    bool show_relations = false;
    bool draw_envelope_distance = false;
    bool draw_relation_from_head = false;
    bool show_text = false;
    /* Draw the inner part of the bones, otherwise render just outlines. */
    bool is_filled = false;

    const ThemeWireColor *bcolor = nullptr; /* Pose-channel color. */

    DrawContext() = default;
  };

  DrawContext create_draw_context(const ObjectRef &ob_ref,
                                  Resources &res,
                                  const State &state,
                                  eArmatureDrawMode draw_mode)
  {
    bArmature &arm = DRW_object_get_data_for_drawing<bArmature>(*ob_ref.object);

    DrawContext ctx;
    ctx.ob = ob_ref.object;
    ctx.ob_ref = &ob_ref;
    ctx.res = &res;
    ctx.dt = state.dt;
    ctx.draw_mode = draw_mode;
    ctx.drawtype = eArmature_Drawtype(arm.drawtype);

    const bool is_edit_or_pose_mode = draw_mode != ARM_DRAW_MODE_OBJECT;
    const bool draw_as_wire = (ctx.ob->dt < OB_SOLID);
    const bool is_transparent = draw_transparent || (draw_as_wire && is_edit_or_pose_mode);

    ctx.bone_buf = is_transparent ? &transparent_ : &opaque_;

    ctx.is_filled = (!draw_transparent && !draw_as_wire) || is_edit_or_pose_mode;
    ctx.show_relations = show_relations;
    ctx.do_relations = show_relations && is_edit_or_pose_mode;
    ctx.draw_envelope_distance = is_edit_or_pose_mode;
    ctx.draw_relation_from_head = (arm.flag & ARM_DRAW_RELATION_FROM_HEAD);
    ctx.show_text = state.show_text;
    ctx.const_color = is_edit_or_pose_mode ? nullptr : &res.object_wire_color(ob_ref, state)[0];
    ctx.const_wire = (!ctx.is_filled || is_transparent) ? 1.0f : 0.0f;
    if ((ctx.ob->base_flag & BASE_SELECTED) && show_outline) {
      ctx.const_wire = 1.5f;
    }
    return ctx;
  }

  void edit_object_sync(Manager & /*manager*/,
                        const ObjectRef &ob_ref,
                        Resources &res,
                        const State &state) final
  {
    if (!enabled_) {
      return;
    }

    DrawContext ctx = create_draw_context(ob_ref, res, state, ARM_DRAW_MODE_EDIT);
    draw_armature_edit(&ctx);
  }

  void object_sync(Manager & /*manager*/,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    if (!enabled_ || ob_ref.object->dt == OB_BOUNDBOX) {
      return;
    }

    eArmatureDrawMode draw_mode = is_pose_mode(ob_ref.object, state) ? ARM_DRAW_MODE_POSE :
                                                                       ARM_DRAW_MODE_OBJECT;

    DrawContext ctx = create_draw_context(ob_ref, res, state, draw_mode);
    draw_armature_pose(&ctx);
  }

  void end_sync(Resources &res, const State & /*state*/) final
  {
    if (!enabled_) {
      return;
    }

    auto end_sync = [&](BoneBuffers &bb) {
      bb.sphere_fill_buf.end_sync(*bb.sphere_fill, res.shapes.bone_sphere.get());
      bb.sphere_outline_buf.end_sync(*bb.sphere_outline, res.shapes.bone_sphere_wire.get());

      bb.octahedral_fill_buf.end_sync(*bb.shape_fill, res.shapes.bone_octahedron.get());
      bb.octahedral_outline_buf.end_sync(
          *bb.shape_outline, res.shapes.bone_octahedron_wire.get(), GPU_PRIM_LINES, 1);

      bb.bbones_fill_buf.end_sync(*bb.shape_fill, res.shapes.bone_box.get());
      bb.bbones_outline_buf.end_sync(
          *bb.shape_outline, res.shapes.bone_box_wire.get(), GPU_PRIM_LINES, 1);

      bb.envelope_fill_buf.end_sync(*bb.envelope_fill, res.shapes.bone_envelope.get());
      bb.envelope_outline_buf.end_sync(*bb.envelope_outline, res.shapes.bone_envelope_wire.get());
      bb.envelope_distance_buf.end_sync(*bb.envelope_distance, res.shapes.bone_envelope.get());

      bb.stick_buf.end_sync(*bb.stick, res.shapes.bone_stick.get());

      bb.wire_buf.end_sync(*bb.wire);

      bb.arrows_buf.end_sync(*bb.arrows, res.shapes.arrows.get());

      bb.degrees_of_freedom_fill_buf.end_sync(*bb.degrees_of_freedom_fill,
                                              res.shapes.bone_degrees_of_freedom.get());
      bb.degrees_of_freedom_wire_buf.end_sync(*bb.degrees_of_freedom_wire,
                                              res.shapes.bone_degrees_of_freedom_wire.get());

      bb.relations_buf.end_sync(*bb.relations);

      using CustomShapeBuf = MutableMapItem<gpu::Batch *, std::unique_ptr<BoneInstanceBuf>>;

      gpu::Batch *arrow_batch = res.shapes.arrows.get();
      for (CustomShapeBuf item : bb.custom_shape_fill.items()) {
        item.value->end_sync(*bb.shape_fill, item.key);
      }
      for (CustomShapeBuf item : bb.custom_shape_outline.items()) {
        item.value->end_sync(*bb.shape_outline, item.key, GPU_PRIM_LINES, 1);
      }
      for (CustomShapeBuf item : bb.custom_shape_wire.items()) {
        /* WORKAROUND: This shape needs a special vertex shader path that should be triggered by
         * its `vclass` attribute. However, to avoid many changes in the primitive expansion API,
         * we create a specific path inside the shader only for this shape batch and infer the
         * value of the `vclass` attribute based on the vertex index. */
        if (item.key == arrow_batch) {
          bb.shape_wire->push_constant("use_arrow_drawing", true);
        }

        item.value->end_sync(*bb.shape_wire, item.key, GPU_PRIM_TRIS, 2);

        if (item.key == arrow_batch) {
          bb.shape_wire->push_constant("use_arrow_drawing", false);
        }
      }
      for (CustomShapeBuf item : bb.custom_shape_wire_strip.items()) {
        item.value->end_sync(*bb.shape_wire_strip, item.key, GPU_PRIM_TRIS, 2);
      }
    };

    end_sync(transparent_);
    end_sync(opaque_);
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(armature_ps_, view);
  }

  /* Public for the time of the Overlay Next port to avoid duplicated logic. */
  static void draw_armature_pose(Armatures::DrawContext *ctx);
  static void draw_armature_edit(Armatures::DrawContext *ctx);

  static bool is_pose_mode(const Object *armature_ob, const State &state)
  {
    const Object *active_ob = state.object_active;

    /* Armature is in pose mode. */
    if (((armature_ob == active_ob) || (armature_ob->mode & OB_MODE_POSE)) &&
        ((state.object_mode & OB_MODE_POSE) != 0))
    {
      return true;
    }

    /* Active object is in weight paint and the associated armature is in pose mode. */
    if ((active_ob != nullptr) && (state.object_mode & OB_MODE_ALL_WEIGHT_PAINT)) {
      if (armature_ob == BKE_object_pose_armature_get(const_cast<Object *>(active_ob))) {
        return true;
      }
    }

    return false;
  }
};

}  // namespace blender::draw::overlay
