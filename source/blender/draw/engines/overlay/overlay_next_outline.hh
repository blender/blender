/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_grease_pencil.hh"
#include "overlay_next_private.hh"

#include "draw_common.hh"

namespace blender::draw::overlay {

class Outline {
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

  bool enabled_ = false;

  overlay::GreasePencil::ViewParameters grease_pencil_view;

 public:
  void begin_sync(Resources &res, const State &state)
  {
    enabled_ = state.v3d && (state.v3d_flag & V3D_SELECT_OUTLINE);
    if (!enabled_) {
      return;
    }

    {
      /* TODO(fclem): This is against design. We should not sync depending on view position.
       * Eventually, we should do this in a compute shader prepass. */
      float4x4 viewinv;
      DRW_view_viewmat_get(nullptr, viewinv.ptr(), true);
      grease_pencil_view = {DRW_view_is_persp_get(nullptr), viewinv};
    }

    const float outline_width = UI_GetThemeValuef(TH_OUTLINE_WIDTH);
    const bool do_smooth_lines = (U.gpu_flag & USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE) != 0;
    const bool do_expand = (U.pixelsize > 1.0) || (outline_width > 2.0f);
    const bool is_transform = (G.moving & G_TRANSFORM_OBJ) != 0;

    {
      auto &pass = outline_prepass_ps_;
      pass.init();
      pass.framebuffer_set(&prepass_fb_);
      pass.clear_color_depth_stencil(float4(0.0f), 1.0f, 0x0);
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                     state.clipping_plane_count);
      {
        auto &sub = pass.sub("Curves");
        sub.shader_set(res.shaders.outline_prepass_curves.get());
        sub.push_constant("isTransform", is_transform);
        sub.bind_ubo("globalsBlock", &res.globals_buf);
        prepass_curves_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("PointCloud");
        sub.shader_set(res.shaders.outline_prepass_pointcloud.get());
        sub.push_constant("isTransform", is_transform);
        sub.bind_ubo("globalsBlock", &res.globals_buf);
        prepass_pointcloud_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("GreasePencil");
        sub.shader_set(res.shaders.outline_prepass_gpencil.get());
        sub.push_constant("isTransform", is_transform);
        sub.bind_ubo("globalsBlock", &res.globals_buf);
        prepass_gpencil_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("Mesh");
        sub.shader_set(res.shaders.outline_prepass_mesh.get());
        sub.push_constant("isTransform", is_transform);
        sub.bind_ubo("globalsBlock", &res.globals_buf);
        prepass_mesh_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("Volume");
        sub.shader_set(res.shaders.outline_prepass_mesh.get());
        sub.push_constant("isTransform", is_transform);
        sub.bind_ubo("globalsBlock", &res.globals_buf);
        prepass_volume_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("Wire");
        sub.shader_set(res.shaders.outline_prepass_wire.get());
        sub.push_constant("isTransform", is_transform);
        sub.bind_ubo("globalsBlock", &res.globals_buf);
        prepass_wire_ps_ = &sub;
      }
    }
    {
      auto &pass = outline_resolve_ps_;
      pass.init();
      pass.framebuffer_set(&res.overlay_line_only_fb);
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL);
      pass.shader_set(res.shaders.outline_detect.get());
      /* Don't occlude the outline if in xray mode as it causes too much flickering. */
      pass.push_constant("alphaOcclu", state.xray_enabled ? 1.0f : 0.35f);
      pass.push_constant("doThickOutlines", do_expand);
      pass.push_constant("doAntiAliasing", do_smooth_lines);
      pass.push_constant("isXrayWires", state.xray_enabled_and_not_wire);
      pass.bind_texture("outlineId", &object_id_tx_);
      pass.bind_texture("sceneDepth", &res.depth_tx);
      pass.bind_texture("outlineDepth", &tmp_depth_tx_);
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
  }

  void object_sync(Manager &manager, const ObjectRef &ob_ref, const State &state)
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
      case OB_GPENCIL_LEGACY:
        /* TODO ? */
        break;
      case OB_CURVES:
        geom = curves_sub_pass_setup(*prepass_curves_ps_, state.scene, ob_ref.object);
        prepass_curves_ps_->draw(geom, manager.unique_handle(ob_ref));
        break;
      case OB_GREASE_PENCIL:
        GreasePencil::draw_grease_pencil(*prepass_gpencil_ps_,
                                         grease_pencil_view,
                                         state.scene,
                                         ob_ref.object,
                                         manager.unique_handle(ob_ref));
        break;
      case OB_MESH:
        if (!state.xray_enabled_and_not_wire) {
          geom = DRW_cache_mesh_surface_get(ob_ref.object);
          prepass_mesh_ps_->draw(geom, manager.unique_handle(ob_ref));
        }
        {
          /* TODO(fclem): This is against design. We should not sync depending on view position.
           * Eventually, add a bounding box display pass with some special culling phase. */

          /* Display flat object as a line when view is orthogonal to them.
           * This fixes only the biggest case which is a plane in ortho view. */
          int flat_axis = 0;
          bool is_flat_object_viewed_from_side = ((state.rv3d->persp == RV3D_ORTHO) &&
                                                  DRW_object_is_flat(ob_ref.object, &flat_axis) &&
                                                  DRW_object_axis_orthogonal_to_view(ob_ref.object,
                                                                                     flat_axis));
          if (state.xray_enabled_and_not_wire || is_flat_object_viewed_from_side) {
            geom = DRW_cache_mesh_edge_detection_get(ob_ref.object, nullptr);
            prepass_wire_ps_->draw_expand(
                geom, GPU_PRIM_LINES, 1, 1, manager.unique_handle(ob_ref));
          }
        }
        break;
      case OB_POINTCLOUD:
        /* Looks bad in wireframe mode. Could be relaxed if we draw a wireframe of some sort in
         * the future. */
        if (!state.is_wireframe_mode) {
          geom = point_cloud_sub_pass_setup(*prepass_pointcloud_ps_, ob_ref.object);
          prepass_pointcloud_ps_->draw(geom, manager.unique_handle(ob_ref));
        }
        break;
      case OB_VOLUME:
        geom = DRW_cache_volume_selection_surface_get(ob_ref.object);
        prepass_volume_ps_->draw(geom, manager.unique_handle(ob_ref));
        break;
      default:
        break;
    }
  }

  void draw(Resources &res, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_debug_group_begin("Outline");

    int2 render_size = int2(res.depth_tx.size());

    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    tmp_depth_tx_.acquire(render_size, GPU_DEPTH24_STENCIL8, usage);
    object_id_tx_.acquire(render_size, GPU_R16UI, usage);

    prepass_fb_.ensure(GPU_ATTACHMENT_TEXTURE(tmp_depth_tx_),
                       GPU_ATTACHMENT_TEXTURE(object_id_tx_));

    manager.submit(outline_prepass_ps_, view);
    manager.submit(outline_resolve_ps_, view);

    tmp_depth_tx_.release();
    object_id_tx_.release();

    GPU_debug_group_end();
  }
};

}  // namespace blender::draw::overlay
