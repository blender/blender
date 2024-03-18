/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_math_quaternion_types.hh"

#include "BKE_grease_pencil.hh"
#include "BKE_image.h"
#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "draw_manager.hh"
#include "draw_pass.hh"

#include "gpencil_layer.hh"
#include "gpencil_material.hh"
#include "gpencil_shader.hh"

namespace blender::draw::greasepencil {

using namespace draw;

class ObjectModule {
 private:
  LayerModule &layers_;
  MaterialModule &materials_;
  ShaderModule &shaders_;

  /** Contains all Objects in the scene. Indexed by drw_ResourceID. */
  StorageArrayBuffer<gpObject> objects_buf_ = "gp_objects_buf";

  /** Contains all gpencil objects in the scene as well as their effect sub-passes. */
  PassSortable main_ps_ = {"gp_main_ps"};

  /** Contains all composited GPencil layers from one object if is uses VFX. */
  TextureFromPool object_color_tx_ = {"gp_color_object_tx"};
  TextureFromPool object_reveal_tx_ = {"gp_reveal_object_tx"};
  Framebuffer object_fb_ = {"gp_object_fb"};
  bool is_object_fb_needed_ = false;

  /** Contains all strokes from one layer if is uses blending. (also used as target for VFX) */
  TextureFromPool layer_color_tx_ = {"gp_color_layer_tx"};
  TextureFromPool layer_reveal_tx_ = {"gp_reveal_layer_tx"};
  Framebuffer layer_fb_ = {"gp_layer_fb"};
  bool is_layer_fb_needed_ = false;

  bool use_onion_ = true;
  bool use_stroke_fill_ = true;
  bool use_vfx_ = true;
  bool is_render_ = true;
  bool is_persp_ = true;

  /** Forward vector used to sort gpencil objects. */
  float3 camera_forward_;
  float3 camera_pos_;

  const Scene *scene_ = nullptr;

  /** \note Needs not to be temporary variable since it is dereferenced later. */
  std::array<float4, 2> clear_colors_ = {float4(0.0f, 0.0f, 0.0f, 0.0f),
                                         float4(1.0f, 1.0f, 1.0f, 1.0f)};

 public:
  ObjectModule(LayerModule &layers, MaterialModule &materials, ShaderModule &shaders)
      : layers_(layers), materials_(materials), shaders_(shaders){};

  void init(const View3D *v3d, const Scene *scene)
  {
    const bool is_viewport = (v3d != nullptr);
    scene_ = scene;

    if (is_viewport) {
      /* TODO(fclem): Avoid access to global DRW. */
      const bContext *evil_C = DRW_context_state_get()->evil_C;
      const bool playing = (evil_C != nullptr) ?
                               ED_screen_animation_playing(CTX_wm_manager(evil_C)) != nullptr :
                               false;
      const bool hide_overlay = ((v3d->flag2 & V3D_HIDE_OVERLAYS) != 0);
      const bool show_onion = ((v3d->gp_flag & V3D_GP_SHOW_ONION_SKIN) != 0);
      use_onion_ = show_onion && !hide_overlay && !playing;
      use_stroke_fill_ = GPENCIL_SIMPLIFY_FILL(scene, playing);
      use_vfx_ = GPENCIL_SIMPLIFY_FX(scene, playing);
      is_render_ = false;
    }
    else {
      use_stroke_fill_ = GPENCIL_SIMPLIFY_FILL(scene, false);
      use_vfx_ = GPENCIL_SIMPLIFY_FX(scene, false);
    }
  }

  void begin_sync(Depsgraph * /*depsgraph*/, const View &main_view)
  {
    camera_forward_ = main_view.forward();
    camera_pos_ = main_view.location();

    is_object_fb_needed_ = false;
    is_layer_fb_needed_ = false;

    is_persp_ = main_view.is_persp();
    /* TODO(fclem): Shrink buffer. */
    // objects_buf_.shrink();
  }

  void sync_grease_pencil(Manager &manager,
                          ObjectRef &object_ref,
                          Framebuffer &main_fb,
                          Framebuffer &scene_fb,
                          TextureFromPool &depth_tx,
                          PassSortable &main_ps)
  {
    using namespace blender::bke::greasepencil;

    Object *object = object_ref.object;
    const GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

    if (grease_pencil.drawings().is_empty()) {
      return;
    }

    const bool is_stroke_order_3d = false; /* TODO */
    bool do_material_holdout = false;
    bool do_layer_blending = false;
    bool object_has_vfx = false; /* TODO: `vfx.object_has_vfx(gpd);`. */

    uint material_offset = materials_.object_offset_get();
    for (const int i : IndexRange(BKE_object_material_count_eval(object))) {
      materials_.sync(object, i, do_material_holdout);
    }

    uint layer_offset = layers_.object_offset_get();
    for (const Layer *layer : grease_pencil.layers()) {
      layers_.sync(object, *layer, do_layer_blending);
    }

    /* Order rendering using camera Z distance. */
    float3 position = float3(object->object_to_world[3]);
    float camera_z = math::dot(position, camera_forward_);

    PassMain::Sub &object_subpass = main_ps.sub("GPObject", camera_z);
    object_subpass.framebuffer_set((object_has_vfx) ? &object_fb_ : &main_fb);
    object_subpass.clear_depth(is_stroke_order_3d ? 1.0f : 0.0f);
    if (object_has_vfx) {
      object_subpass.clear_multi(clear_colors_);
    }

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_BLEND_ALPHA_PREMUL;
    /* For 2D mode, we render all strokes with uniform depth (increasing with stroke id). */
    state |= (is_stroke_order_3d) ? DRW_STATE_DEPTH_LESS_EQUAL : DRW_STATE_DEPTH_GREATER;
    /* Always write stencil. Only used as optimization for blending. */
    state |= DRW_STATE_WRITE_STENCIL | DRW_STATE_STENCIL_ALWAYS;

    object_subpass.state_set(state);
    object_subpass.shader_set(shaders_.static_shader_get(GREASE_PENCIL));

    GPUVertBuf *position_tx = DRW_cache_grease_pencil_position_buffer_get(scene_, object);
    GPUVertBuf *color_tx = DRW_cache_grease_pencil_color_buffer_get(scene_, object);
    GPUBatch *geom = DRW_cache_grease_pencil_get(scene_, object);

    /* TODO(fclem): Pass per frame object matrix here. */
    ResourceHandle handle = manager.resource_handle(object_ref);
    gpObject &ob = objects_buf_.get_or_resize(handle.resource_index());
    ob.is_shadeless = false;
    ob.stroke_order3d = false;
    ob.tint = float4(1.0);  // frame_tint_get(gpd, frame.gpf, current_frame_);
    ob.layer_offset = layer_offset;
    ob.material_offset = material_offset;

    if (do_layer_blending) {
      /* TODO: Do layer blending. */
      // for (const LayerData &layer : frame.layers) {
      //    UNUSED_VARS(layer);
      //    if (has_blending(layer)) {
      //      object_subpass.framebuffer_set(*vfx_fb.current());
      //    }

      /* TODO(fclem): Only draw subrange of geometry for this layer. */
      object_subpass.bind_texture("gp_pos_tx", position_tx);
      object_subpass.bind_texture("gp_col_tx", color_tx);
      object_subpass.draw(geom, handle);

      /* TODO: Do layer blending. */
      //    if (has_blending(layer)) {
      //      layer_blend_sync(object_ref, object_subpass);
      //    }
      // }
    }
    else {
      /* Fast path. */
      object_subpass.bind_texture("gp_pos_tx", position_tx);
      object_subpass.bind_texture("gp_col_tx", color_tx);
      object_subpass.draw(geom, handle);
    }

    /** Merging the object depth buffer into the scene depth buffer. */
    float4x4 plane_mat = get_object_plane_mat(*object);
    ResourceHandle handle_plane_mat = manager.resource_handle(plane_mat);
    object_subpass.framebuffer_set(&scene_fb);
    object_subpass.state_set(DRW_STATE_DEPTH_LESS | DRW_STATE_WRITE_DEPTH);
    object_subpass.shader_set(shaders_.static_shader_get(DEPTH_MERGE));
    object_subpass.bind_texture("depthBuf", (object_has_vfx) ? nullptr : &depth_tx);
    object_subpass.draw(DRW_cache_quad_get(), handle_plane_mat);

    /* TODO: Do object VFX. */
#if 0
    if (object_has_vfx) {
      VfxContext vfx_ctx(object_subpass,
                         layer_fb_,
                         object_fb_,
                         object_color_tx_,
                         layer_color_tx_,
                         object_reveal_tx_,
                         layer_reveal_tx_,
                         is_render_);

      /* \note Update this boolean as the actual number of vfx drawn might differ. */
      object_has_vfx = vfx.object_sync(main_fb_, object_ref, vfx_ctx, do_material_holdout);

      if (object_has_vfx || do_layer_blending) {
        is_layer_fb_needed_ = true;
      }
    }
#endif
  }

  void end_sync()
  {
    objects_buf_.push_update();
  }

  void bind_resources(PassMain::Sub &sub)
  {
    sub.bind_ssbo(GPENCIL_OBJECT_SLOT, &objects_buf_);
  }

  void acquire_temporary_buffers(int2 render_size, eGPUTextureFormat format)
  {
    object_color_tx_.acquire(render_size, format);
    object_reveal_tx_.acquire(render_size, format);
    object_fb_.ensure(GPU_ATTACHMENT_NONE,
                      GPU_ATTACHMENT_TEXTURE(object_color_tx_),
                      GPU_ATTACHMENT_TEXTURE(object_reveal_tx_));
    if (is_layer_fb_needed_) {
      layer_color_tx_.acquire(render_size, format);
      layer_reveal_tx_.acquire(render_size, format);
      layer_fb_.ensure(GPU_ATTACHMENT_NONE,
                       GPU_ATTACHMENT_TEXTURE(layer_color_tx_),
                       GPU_ATTACHMENT_TEXTURE(layer_reveal_tx_));
    }
  }

  void release_temporary_buffers()
  {
    object_color_tx_.release();
    object_reveal_tx_.release();

    layer_color_tx_.release();
    layer_reveal_tx_.release();
  }

  bool scene_has_visible_gpencil_object() const
  {
    return objects_buf_.size() > 0;
  }

  /**
   * Define a matrix that will be used to render a triangle to merge the depth of the rendered
   * gpencil object with the rest of the scene.
   */
  float4x4 get_object_plane_mat(const Object &object)
  {
    using namespace math;
    /* Find the normal most likely to represent the gpObject. */
    /* TODO: This does not work quite well if you use
     * strokes not aligned with the object axes. Maybe we could try to
     * compute the minimum axis of all strokes. But this would be more
     * computationally heavy and should go into the GPData evaluation. */
    BLI_assert(object.type == OB_GREASE_PENCIL);
    const GreasePencil &grease_pencil = *static_cast<const GreasePencil *>(object.data);
    const std::optional<Bounds<float3>> bounds = grease_pencil.bounds_min_max_eval();
    if (!bounds) {
      return float4x4::identity();
    }

    /* Convert bbox to matrix */
    const float3 size = float3(bounds->max - bounds->min) + 1e-8f;
    const float3 center = midpoint(bounds->min, bounds->max);

    /* BBox space to World. */
    const float4x4 object_to_world = float4x4(object.object_to_world);
    float4x4 bbox_mat = object_to_world *
                        from_loc_rot_scale<float4x4>(center, Quaternion::identity(), size);
    float3 plane_normal;
    if (is_persp_) {
      /* BBox center to camera vector. */
      plane_normal = camera_pos_ - bbox_mat.location();
    }
    else {
      plane_normal = camera_forward_;
    }
    /* World to BBox space. */
    float4x4 bbox_mat_inv = invert(bbox_mat);
    /* mat_inv_t is a "normal" matrix which will transform
     * BBox normal space to world space. */
    float4x4 bbox_mat_inv_t = transpose(bbox_mat_inv);

    /* Normalize the vector in BBox space. */
    plane_normal = normalize(transform_direction(bbox_mat_inv, plane_normal));
    plane_normal = normalize(transform_direction(bbox_mat_inv_t, plane_normal));

    float4x4 plane_mat = from_up_axis<float4x4>(plane_normal);
    float radius = length(transform_direction(object_to_world, size));
    plane_mat = scale(plane_mat, float3(radius));
    plane_mat.location() = transform_point(object_to_world, center);
    return plane_mat;
  }
};

}  // namespace blender::draw::greasepencil
