/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_paint.hh"

#include "DNA_particle_types.h"

#include "draw_sculpt.hh"

#include "overlay_next_base.hh"
#include "overlay_next_grease_pencil.hh"
#include "overlay_next_particle.hh"

namespace blender::draw::overlay {

/**
 * A depth pass that write surface depth when it is needed.
 * It is also used for selecting non overlay-only objects.
 */
class Prepass : Overlay {
 private:
  PassMain ps_ = {"prepass"};
  PassMain::Sub *mesh_ps_ = nullptr;
  PassMain::Sub *hair_ps_ = nullptr;
  PassMain::Sub *curves_ps_ = nullptr;
  PassMain::Sub *point_cloud_ps_ = nullptr;
  PassMain::Sub *grease_pencil_ps_ = nullptr;

  bool use_material_slot_selection_ = false;

  overlay::GreasePencil::ViewParameters grease_pencil_view;

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_v3d();

    if (!enabled_) {
      /* Not used. But release the data. */
      ps_.init();
      mesh_ps_ = nullptr;
      curves_ps_ = nullptr;
      point_cloud_ps_ = nullptr;
      return;
    }

    {
      /* TODO(fclem): This is against design. We should not sync depending on view position.
       * Eventually, we should do this in a compute shader prepass. */
      float4x4 viewinv;
      DRW_view_viewmat_get(nullptr, viewinv.ptr(), true);
      grease_pencil_view = {DRW_view_is_persp_get(nullptr), viewinv};
    }

    use_material_slot_selection_ = state.is_material_select;

    const View3DShading &shading = state.v3d->shading;
    bool use_cull = ((shading.type == OB_SOLID) && (shading.flag & V3D_SHADING_BACKFACE_CULLING));
    DRWState backface_cull_state = use_cull ? DRW_STATE_CULL_BACK : DRWState(0);

    ps_.init();
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | backface_cull_state,
                  state.clipping_plane_count);
    res.select_bind(ps_);
    {
      auto &sub = ps_.sub("Mesh");
      sub.shader_set(res.is_selection() ? res.shaders.depth_mesh_conservative.get() :
                                          res.shaders.depth_mesh.get());
      mesh_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("Hair");
      sub.shader_set(res.shaders.depth_mesh.get());
      hair_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("Curves");
      sub.shader_set(res.shaders.depth_curves.get());
      curves_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("PointCloud");
      sub.shader_set(res.shaders.depth_point_cloud.get());
      point_cloud_ps_ = &sub;
    }
    {
      auto &sub = ps_.sub("GreasePencil");
      sub.shader_set(res.shaders.depth_grease_pencil.get());
      grease_pencil_ps_ = &sub;
    }
  }

  void particle_sync(Manager &manager, const ObjectRef &ob_ref, Resources &res, const State &state)
  {
    Object *ob = ob_ref.object;

    ResourceHandle handle = {0};

    LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
      if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
        continue;
      }

      const ParticleSettings *part = psys->part;
      const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;
      switch (draw_as) {
        case PART_DRAW_PATH:
          if ((state.is_wireframe_mode == false) && (part->draw_as == PART_DRAW_REND)) {
            /* Case where the render engine should have rendered it, but we need to draw it for
             * selection purpose. */
            if (handle.raw == 0u) {
              handle = manager.resource_handle_for_psys(
                  ob_ref, overlay::Particles::dupli_matrix_get(ob_ref));
            }

            select::ID select_id = use_material_slot_selection_ ?
                                       res.select_id(ob_ref, part->omat << 16) :
                                       res.select_id(ob_ref);

            gpu::Batch *geom = DRW_cache_particles_get_hair(ob, psys, nullptr);
            mesh_ps_->draw(geom, handle, select_id.get());
            break;
          }
          break;
        default:
          /* Other draw modes should be handled by the particle overlay. */
          break;
      }
    }
  }

  void sculpt_sync(Manager &manager, const ObjectRef &ob_ref, Resources &res)
  {
    ResourceHandle handle = manager.resource_handle_for_sculpt(ob_ref);
    select::ID select_id = res.select_id(ob_ref);

    for (SculptBatch &batch : sculpt_batches_get(ob_ref.object, SCULPT_BATCH_DEFAULT)) {
      mesh_ps_->draw(batch.batch, handle, select_id.get());
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

    particle_sync(manager, ob_ref, res, state);

    const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob_ref.object, state.rv3d) &&
                                 !state.is_image_render;

    if (use_sculpt_pbvh) {
      sculpt_sync(manager, ob_ref, res);
      return;
    }

    gpu::Batch *geom_single = nullptr;
    Span<gpu::Batch *> geom_list(&geom_single, 1);

    PassMain::Sub *pass = nullptr;
    switch (ob_ref.object->type) {
      case OB_MESH:
        if (use_material_slot_selection_) {
          /* TODO(fclem): Improve the API. */
          const int materials_len = DRW_cache_object_material_count_get(ob_ref.object);
          Array<GPUMaterial *> materials(materials_len);
          materials.fill(nullptr);

          gpu::Batch **geom_per_mat = DRW_cache_mesh_surface_shaded_get(
              ob_ref.object, materials.data(), materials_len);

          geom_list = {geom_per_mat, materials_len};
        }
        else {
          geom_single = DRW_cache_mesh_surface_get(ob_ref.object);
        }
        pass = mesh_ps_;
        break;
      case OB_VOLUME:
        if (!res.is_selection()) {
          /* Disable during display, only enable for selection. */
          /* TODO(fclem): Would be nice to have even when not selecting to occlude overlays. */
          return;
        }
        geom_single = DRW_cache_volume_selection_surface_get(ob_ref.object);
        pass = mesh_ps_;
        break;
      case OB_POINTCLOUD:
        geom_single = point_cloud_sub_pass_setup(*point_cloud_ps_, ob_ref.object);
        pass = point_cloud_ps_;
        break;
      case OB_CURVES:
        geom_single = curves_sub_pass_setup(*curves_ps_, state.scene, ob_ref.object);
        pass = curves_ps_;
        break;
      case OB_GREASE_PENCIL:
        if (!res.is_selection()) {
          /* Disable during display, only enable for selection.
           * The grease pencil engine already renders it properly. */
          return;
        }
        GreasePencil::draw_grease_pencil(*grease_pencil_ps_,
                                         grease_pencil_view,
                                         state.scene,
                                         ob_ref.object,
                                         manager.unique_handle(ob_ref),
                                         res.select_id(ob_ref));
        return;
      default:
        break;
    }

    if (pass == nullptr) {
      return;
    }

    ResourceHandle res_handle = manager.unique_handle(ob_ref);

    for (int material_id : geom_list.index_range()) {
      select::ID select_id = use_material_slot_selection_ ?
                                 res.select_id(ob_ref, (material_id + 1) << 16) :
                                 res.select_id(ob_ref);

      if (res.is_selection() && (pass == mesh_ps_)) {
        /* Conservative shader needs expanded draw-call. */
        pass->draw_expand(
            geom_list[material_id], GPU_PRIM_TRIS, 1, 1, res_handle, select_id.get());
      }
      else {
        pass->draw(geom_list[material_id], res_handle, select_id.get());
      }
    }
  }

  void pre_draw(Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    manager.generate_commands(ps_, view);
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }
    /* Should be fine to use the line buffer since the prepass only writes to the depth buffer. */
    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(ps_, view);
  }
};

}  // namespace blender::draw::overlay
