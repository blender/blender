/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_paint.hh"
#include "DNA_volume_types.h"

#include "draw_common.hh"

#include "overlay_next_mesh.hh"

namespace blender::draw::overlay {

class Wireframe {
 private:
  PassMain wireframe_ps_ = {"Wireframe"};
  struct ColoringPass {
    PassMain::Sub *curves_ps_ = nullptr;
    PassMain::Sub *mesh_ps_ = nullptr;
    PassMain::Sub *pointcloud_ps_ = nullptr;
    /* Variant for meshes that force drawing all edges. */
    PassMain::Sub *mesh_all_edges_ps_ = nullptr;
  } colored, non_colored;

  /* Copy of the depth buffer to be able to read it during wireframe rendering. */
  TextureFromPool tmp_depth_tx_ = {"tmp_depth_tx"};
  bool do_depth_copy_workaround_ = false;

  /* Force display of wireframe on surface objects, regardless of the object display settings. */
  bool show_wire_ = false;

  bool enabled_ = false;

 public:
  void begin_sync(Resources &res, const State &state)
  {
    enabled_ = (state.space_type == SPACE_VIEW3D) &&
               (state.is_wireframe_mode || !state.hide_overlays);
    if (!enabled_) {
      return;
    }

    show_wire_ = state.is_wireframe_mode || (state.overlay.flag & V3D_OVERLAY_WIREFRAMES);

    const bool is_selection = res.selection_type != SelectionType::DISABLED;
    const bool do_smooth_lines = (U.gpu_flag & USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE) != 0;
    const bool is_transform = (G.moving & G_TRANSFORM_OBJ) != 0;
    const float wire_threshold = wire_discard_threshold_get(state.overlay.wireframe_threshold);

    GPUTexture **depth_tex = (state.xray_enabled) ? &res.depth_tx : &tmp_depth_tx_;
    if (is_selection) {
      depth_tex = &res.dummy_depth_tx;
    }

    /* Note: Depth buffer has different format when doing selection. Avoid copy in this case. */
    do_depth_copy_workaround_ = !is_selection && (depth_tex == &tmp_depth_tx_);

    {
      auto &pass = wireframe_ps_;
      pass.init();
      pass.state_set(DRW_STATE_FIRST_VERTEX_CONVENTION | DRW_STATE_WRITE_COLOR |
                         DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                     state.clipping_plane_count);
      res.select_bind(pass);

      auto shader_pass =
          [&](GPUShader *shader, const char *name, bool use_coloring, float wire_threshold) {
            auto &sub = pass.sub(name);
            if (res.shaders.wireframe_mesh.get() == shader) {
              sub.specialize_constant(shader, "use_custom_depth_bias", do_smooth_lines);
            }
            sub.shader_set(shader);
            sub.bind_ubo("globalsBlock", &res.globals_buf);
            sub.bind_texture("depthTex", depth_tex);
            sub.push_constant("wireOpacity", state.overlay.wireframe_opacity);
            sub.push_constant("isTransform", is_transform);
            sub.push_constant("colorType", state.v3d->shading.wire_color_type);
            sub.push_constant("useColoring", use_coloring);
            sub.push_constant("wireStepParam", wire_threshold);
            sub.push_constant("isHair", false);
            return &sub;
          };

      auto coloring_pass = [&](ColoringPass &ps, bool use_color) {
        overlay::ShaderModule &sh = res.shaders;
        ps.mesh_ps_ = shader_pass(sh.wireframe_mesh.get(), "Mesh", use_color, wire_threshold);
        ps.mesh_all_edges_ps_ = shader_pass(sh.wireframe_mesh.get(), "Wire", use_color, 1.0f);
        ps.pointcloud_ps_ = shader_pass(sh.wireframe_points.get(), "PtCloud", use_color, 1.0f);
        ps.curves_ps_ = shader_pass(sh.wireframe_curve.get(), "Curve", use_color, 1.0f);
      };

      coloring_pass(non_colored, false);
      coloring_pass(colored, true);
    }
  }

  void object_sync(Manager &manager,
                   const ObjectRef &ob_ref,
                   const State &state,
                   Resources &res,
                   const bool in_edit_paint_mode)
  {
    if (!enabled_) {
      return;
    }

    if (ob_ref.object->dt < OB_WIRE) {
      return;
    }

    const bool all_edges = (ob_ref.object->dtx & OB_DRAW_ALL_EDGES) != 0;
    const bool show_surface_wire = show_wire_ || (ob_ref.object->dtx & OB_DRAWWIRE) ||
                                   (ob_ref.object->dt == OB_WIRE);

    ColoringPass &coloring = in_edit_paint_mode ? non_colored : colored;
    switch (ob_ref.object->type) {
      case OB_CURVES_LEGACY: {
        gpu::Batch *geom = DRW_cache_curve_edge_wire_get(ob_ref.object);
        coloring.curves_ps_->draw(
            geom, manager.unique_handle(ob_ref), res.select_id(ob_ref).get());
        break;
      }
      case OB_FONT: {
        gpu::Batch *geom = DRW_cache_text_edge_wire_get(ob_ref.object);
        coloring.curves_ps_->draw(
            geom, manager.unique_handle(ob_ref), res.select_id(ob_ref).get());
        break;
      }
      case OB_SURF: {
        gpu::Batch *geom = DRW_cache_surf_edge_wire_get(ob_ref.object);
        coloring.curves_ps_->draw(
            geom, manager.unique_handle(ob_ref), res.select_id(ob_ref).get());
        break;
      }
      case OB_CURVES:
        /* TODO(fclem): Not yet implemented. */
        break;
      case OB_GREASE_PENCIL: {
        if (show_surface_wire) {
          gpu::Batch *geom = DRW_cache_grease_pencil_face_wireframe_get(state.scene,
                                                                        ob_ref.object);
          coloring.curves_ps_->draw(
              geom, manager.unique_handle(ob_ref), res.select_id(ob_ref).get());
        }
        break;
      }
      case OB_MESH:
        if (show_surface_wire) {
          if (BKE_sculptsession_use_pbvh_draw(ob_ref.object, state.rv3d)) {
            ResourceHandle handle = manager.unique_handle(ob_ref);

            for (SculptBatch &batch : sculpt_batches_get(ob_ref.object, SCULPT_BATCH_WIREFRAME)) {
              coloring.mesh_all_edges_ps_->draw(batch.batch, handle);
            }
          }
          else {
            gpu::Batch *geom = DRW_cache_mesh_face_wireframe_get(ob_ref.object);
            (all_edges ? coloring.mesh_all_edges_ps_ : coloring.mesh_ps_)
                ->draw(geom, manager.unique_handle(ob_ref), res.select_id(ob_ref).get());
          }
        }

        /* Draw loose geometry. */
        if (!in_edit_paint_mode || Meshes::mesh_has_edit_cage(ob_ref.object)) {
          const Mesh *mesh = static_cast<const Mesh *>(ob_ref.object->data);
          gpu::Batch *geom;
          if ((mesh->edges_num == 0) && (mesh->verts_num > 0)) {
            geom = DRW_cache_mesh_all_verts_get(ob_ref.object);
            coloring.pointcloud_ps_->draw(
                geom, manager.unique_handle(ob_ref), res.select_id(ob_ref).get());
          }
          else if ((geom = DRW_cache_mesh_loose_edges_get(ob_ref.object))) {
            coloring.mesh_all_edges_ps_->draw(
                geom, manager.unique_handle(ob_ref), res.select_id(ob_ref).get());
          }
        }
        break;
      case OB_POINTCLOUD: {
        if (show_surface_wire) {
          gpu::Batch *geom = DRW_pointcloud_batch_cache_get_dots(ob_ref.object);
          coloring.pointcloud_ps_->draw(
              geom, manager.unique_handle(ob_ref), res.select_id(ob_ref).get());
        }
        break;
      }
      case OB_VOLUME: {
        gpu::Batch *geom = DRW_cache_volume_face_wireframe_get(ob_ref.object);
        if (static_cast<Volume *>(ob_ref.object->data)->display.wireframe_type ==
            VOLUME_WIREFRAME_POINTS)
        {
          coloring.pointcloud_ps_->draw(
              geom, manager.unique_handle(ob_ref), res.select_id(ob_ref).get());
        }
        else {
          coloring.mesh_ps_->draw(
              geom, manager.unique_handle(ob_ref), res.select_id(ob_ref).get());
        }
        break;
      }
      default:
        /* Would be good to have. */
        // BLI_assert_unreachable();
        break;
    }
  }

  void pre_draw(Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    manager.generate_commands(wireframe_ps_, view);
  }

  void draw(Framebuffer &framebuffer, Resources &res, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    if (do_depth_copy_workaround_) {
      eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
      int2 render_size = int2(res.depth_tx.size());
      tmp_depth_tx_.acquire(render_size, GPU_DEPTH24_STENCIL8, usage);

      /* WORKAROUND: Nasty framebuffer copy.
       * We should find a way to have nice wireframe without this. */
      GPU_texture_copy(tmp_depth_tx_, res.depth_tx);
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(wireframe_ps_, view);

    if (do_depth_copy_workaround_) {
      tmp_depth_tx_.release();
    }
  }

 private:
  float wire_discard_threshold_get(float threshold)
  {
    /* Use `sqrt` since the value stored in the edge is a variation of the cosine, so its square
     * becomes more proportional with a variation of angle. */
    threshold = sqrt(abs(threshold));
    /* The maximum value (255 in the VBO) is used to force hide the edge. */
    return math::interpolate(0.0f, 1.0f - (1.0f / 255.0f), threshold);
  }
};

}  // namespace blender::draw::overlay
