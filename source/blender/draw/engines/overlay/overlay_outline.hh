/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_base.hh"
#include "overlay_grease_pencil.hh"

#include "draw_common.hh"

#include "DNA_userdef_types.h"

namespace blender::draw::overlay {

/**
 * Display selected object outline.
 * The option can be found under (Viewport Overlays > Objects > Outline Selected).
 */
class Outline : Overlay {
 private:
  /* Simple render pass that renders an object ID pass. */
  PassMain outline_prepass_ps_ = {"Prepass"};
  PassMain::Sub *prepass_curves_ps_ = nullptr;
  PassMain::Sub *prepass_pointcloud_ps_ = nullptr;
  PassMain::Sub *prepass_gpencil_ps_ = nullptr;
  PassMain::Sub *prepass_mesh_ps_ = nullptr;
  PassMain::Sub *prepass_volume_ps_ = nullptr;
  PassMain::Sub *prepass_wire_ps_ = nullptr;
  /* Detect edges inside the ID pass and output color for each of them. */
  PassSimple outline_resolve_ps_ = {"Resolve"};

  TextureFromPool object_id_tx_ = {"outline_ob_id_tx"};
  TextureFromPool tmp_depth_tx_ = {"outline_depth_tx"};

  Framebuffer prepass_fb_ = {"outline.prepass_fb"};

  Vector<FlatObjectRef> flat_objects_;

  PassMain outline_prepass_flat_ps_ = {"PrepassFlat"};

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = !res.is_selection();
    enabled_ &= state.v3d && (state.v3d_flag & V3D_SELECT_OUTLINE);

    flat_objects_.clear();

    if (!enabled_) {
      return;
    }

    const float outline_width = UI_GetThemeValuef(TH_OUTLINE_WIDTH);
    const bool do_smooth_lines = (U.gpu_flag & USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE) != 0;
    const bool do_expand = (U.pixelsize > 1.0) || (outline_width > 2.0f);
    const bool is_transform = (G.moving & G_TRANSFORM_OBJ) != 0;

    {
      auto &pass = outline_prepass_ps_;
      pass.init();
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
      pass.framebuffer_set(&prepass_fb_);
      pass.clear_color_depth_stencil(float4(0.0f), 1.0f, 0x0);
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                     state.clipping_plane_count);
      {
        auto &sub = pass.sub("Curves");
        sub.shader_set(res.shaders->outline_prepass_curves.get());
        sub.push_constant("is_transform", is_transform);
        prepass_curves_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("PointCloud");
        sub.shader_set(res.shaders->outline_prepass_pointcloud.get());
        sub.push_constant("is_transform", is_transform);
        prepass_pointcloud_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("GreasePencil");
        sub.shader_set(res.shaders->outline_prepass_gpencil.get());
        sub.push_constant("is_transform", is_transform);
        prepass_gpencil_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("Mesh");
        sub.shader_set(res.shaders->outline_prepass_mesh.get());
        sub.push_constant("is_transform", is_transform);
        prepass_mesh_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("Volume");
        sub.shader_set(res.shaders->outline_prepass_mesh.get());
        sub.push_constant("is_transform", is_transform);
        prepass_volume_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("Wire");
        sub.shader_set(res.shaders->outline_prepass_wire.get());
        sub.push_constant("is_transform", is_transform);
        prepass_wire_ps_ = &sub;
      }
    }
    {
      auto &pass = outline_resolve_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL);
      pass.shader_set(res.shaders->outline_detect.get());
      /* Don't occlude the outline if in xray mode as it causes too much flickering. */
      pass.push_constant("alpha_occlu", state.xray_enabled ? 1.0f : 0.35f);
      pass.push_constant("do_thick_outlines", do_expand);
      pass.push_constant("do_anti_aliasing", do_smooth_lines);
      pass.push_constant("is_xray_wires", state.xray_enabled_and_not_wire);
      pass.bind_texture("outline_id_tx", &object_id_tx_);
      pass.bind_texture("scene_depth_tx", &res.depth_tx);
      pass.bind_texture("outline_depth_tx", &tmp_depth_tx_);
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
      pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
  }

  void object_sync(Manager &manager,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    /* Outlines of bounding boxes are not drawn. */
    if (ob_ref.object->dt == OB_BOUNDBOX) {
      return;
    }

    gpu::Batch *geom;
    switch (ob_ref.object->type) {
      case OB_CURVES: {
        const char *error = nullptr;
        /* The error string will always have been printed by the engine already.
         * No need to display it twice. */
        geom = curves_sub_pass_setup(*prepass_curves_ps_, state.scene, ob_ref.object, error);
        prepass_curves_ps_->draw(geom, manager.unique_handle(ob_ref));
        break;
      }
      case OB_GREASE_PENCIL:
        GreasePencil::draw_grease_pencil(
            res, *prepass_gpencil_ps_, state.scene, ob_ref.object, manager.unique_handle(ob_ref));
        break;
      case OB_MESH:
        if (state.xray_enabled_and_not_wire) {
          geom = DRW_cache_mesh_edge_detection_get(ob_ref.object, nullptr);
          prepass_wire_ps_->draw_expand(geom, GPU_PRIM_LINES, 1, 1, manager.unique_handle(ob_ref));
        }
        else {
          geom = DRW_cache_mesh_surface_get(ob_ref.object);
          prepass_mesh_ps_->draw(geom, manager.unique_handle(ob_ref));

          /* Display flat object as a line when view is orthogonal to them.
           * This fixes only the biggest case which is a plane in ortho view. */
          int flat_axis = FlatObjectRef::flat_axis_index_get(ob_ref.object);
          if (flat_axis != -1) {
            geom = DRW_cache_mesh_edge_detection_get(ob_ref.object, nullptr);
            flat_objects_.append({geom, manager.unique_handle(ob_ref), flat_axis});
          }
        }
        break;
      case OB_POINTCLOUD:
        /* Looks bad in wireframe mode. Could be relaxed if we draw a wireframe of some sort in
         * the future. */
        if (!state.is_wireframe_mode) {
          geom = pointcloud_sub_pass_setup(*prepass_pointcloud_ps_, ob_ref.object);
          prepass_pointcloud_ps_->draw(geom, manager.unique_handle(ob_ref));
        }
        break;
      case OB_VOLUME:
        geom = DRW_cache_volume_selection_surface_get(ob_ref.object);
        /* TODO(fclem): Get rid of these check and enforce correct API on the batch cache. */
        if (geom) {
          prepass_volume_ps_->draw(geom, manager.unique_handle(ob_ref));
        }
        break;
      default:
        break;
    }
  }

  /* Flat objects outline workaround need to generate passes for each redraw. */
  void flat_objects_pass_sync(Manager &manager, View &view, Resources &res, const State &state)
  {
    outline_prepass_flat_ps_.init();

    if (!enabled_) {
      return;
    }

    if (!view.is_persp()) {
      const bool is_transform = (G.moving & G_TRANSFORM_OBJ) != 0;
      /* Note: We need a dedicated pass since we have to populated it for each redraw. */
      auto &pass = outline_prepass_flat_ps_;
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
      pass.framebuffer_set(&prepass_fb_);
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                     state.clipping_plane_count);
      pass.shader_set(res.shaders->outline_prepass_wire.get());
      pass.push_constant("is_transform", is_transform);

      for (FlatObjectRef flag_ob_ref : flat_objects_) {
        flag_ob_ref.if_flat_axis_orthogonal_to_view(
            manager, view, [&](gpu::Batch *geom, ResourceIndex resource_index) {
              pass.draw_expand(geom, GPU_PRIM_LINES, 1, 1, resource_index);
            });
      }
    }
  }

  void pre_draw(Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    manager.generate_commands(outline_prepass_ps_, view);
    manager.generate_commands(outline_prepass_flat_ps_, view);
  }

  /* TODO(fclem): Remove dependency on Resources. */
  void draw_line_only_ex(Framebuffer &framebuffer, Resources &res, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_debug_group_begin("Outline");

    int2 render_size = int2(res.depth_tx.size());

    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    tmp_depth_tx_.acquire(render_size, gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8, usage);
    object_id_tx_.acquire(render_size, gpu::TextureFormat::UINT_16, usage);

    prepass_fb_.ensure(GPU_ATTACHMENT_TEXTURE(tmp_depth_tx_),
                       GPU_ATTACHMENT_TEXTURE(object_id_tx_));

    manager.submit_only(outline_prepass_ps_, view);
    manager.submit_only(outline_prepass_flat_ps_, view);

    GPU_framebuffer_bind(framebuffer);
    manager.submit(outline_resolve_ps_, view);

    tmp_depth_tx_.release();
    object_id_tx_.release();

    GPU_debug_group_end();
  }
};

}  // namespace blender::draw::overlay
