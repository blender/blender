/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "DNA_collection_types.h"
#include "DNA_particle_types.h"

#include "BKE_pointcache.h"

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class Particles {
 private:
  PassMain particle_ps_ = {"particle_ps_"};
  PassMain::Sub *dot_ps_ = nullptr;
  PassMain::Sub *shape_ps_ = nullptr;
  PassMain::Sub *hair_ps_ = nullptr;

  bool enabled_ = false;

 public:
  void begin_sync(Resources &res, const State &state)
  {
    enabled_ = state.space_type == SPACE_VIEW3D;

    if (!enabled_) {
      return;
    }

    const bool is_transform = (G.moving & G_TRANSFORM_OBJ) != 0;

    {
      auto &pass = particle_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                     state.clipping_plane_count);
      res.select_bind(pass);
      {
        auto &sub = pass.sub("Dots");
        sub.shader_set(res.shaders.particle_dot.get());
        sub.bind_ubo("globalsBlock", &res.globals_buf);
        sub.bind_texture("weightTex", res.weight_ramp_tx);
        dot_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("Shapes");
        sub.shader_set(res.shaders.particle_shape.get());
        sub.bind_ubo("globalsBlock", &res.globals_buf);
        sub.bind_texture("weightTex", res.weight_ramp_tx);
        shape_ps_ = &sub;
      }
      {
        auto &sub = pass.sub("Hair");
        sub.shader_set(res.shaders.particle_hair.get());
        sub.bind_ubo("globalsBlock", &res.globals_buf);
        sub.push_constant("colorType", state.v3d->shading.wire_color_type);
        sub.push_constant("isTransform", is_transform);
        hair_ps_ = &sub;
      }
    }
  }

  /* Particle data are stored in world space. If an object is instanced, the associated particle
   * systems need to be offset appropriately. */
  static float4x4 dupli_matrix_get(const ObjectRef &ob_ref)
  {
    float4x4 dupli_mat = float4x4::identity();

    if ((ob_ref.dupli_parent != nullptr) && (ob_ref.dupli_object != nullptr)) {
      if (ob_ref.dupli_object->type & OB_DUPLICOLLECTION) {
        Collection *collection = ob_ref.dupli_parent->instance_collection;
        if (collection != nullptr) {
          dupli_mat[3] -= float4(float3(collection->instance_offset), 0.0f);
        }
        dupli_mat = ob_ref.dupli_parent->object_to_world() * dupli_mat;
      }
      else {
        dupli_mat = ob_ref.object->object_to_world() *
                    math::invert(ob_ref.dupli_object->ob->object_to_world());
      }
    }
    return dupli_mat;
  }

  void object_sync(Manager &manager, const ObjectRef &ob_ref, Resources &res, const State &state)
  {
    if (!enabled_) {
      return;
    }

    Object *ob = ob_ref.object;

    ResourceHandle handle = {0};

    LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
      if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
        continue;
      }

      if (handle.raw == 0u) {
        handle = manager.resource_handle_for_psys(ob_ref, dupli_matrix_get(ob_ref));
      }

      const ParticleSettings *part = psys->part;

      auto set_color = [&](PassMain::Sub &sub) {
        /* NOTE(fclem): Is color even useful in our modern context? */
        Material *ma = BKE_object_material_get_eval(ob, part->omat);
        sub.push_constant("ucolor", float4(ma ? float3(&ma->r) : float3(0.6f), part->draw_size));
      };

      blender::gpu::Batch *geom = nullptr;
      const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;
      switch (draw_as) {
        case PART_DRAW_PATH:
          if ((state.is_wireframe_mode == false) && (part->draw_as == PART_DRAW_REND)) {
            /* Render engine should have rendered it already. */
            break;
          }
          geom = DRW_cache_particles_get_hair(ob, psys, nullptr);
          hair_ps_->push_constant("useColoring", true); /* TODO */
          hair_ps_->draw(geom, handle, res.select_id(ob_ref).get());
          break;
        case PART_DRAW_NOT:
          /* Nothing to draw. */
          break;
        case PART_DRAW_OB:
        case PART_DRAW_GR:
          /* Instances are realized by Depsgraph and rendered as a regular object instance. */
          break;
        default:
          /* Eventually, would be good to assert. But there are many other draw type that could be
           * set and they need to revert to PART_DRAW_DOT. */
          // BLI_assert_unreachable();
        case PART_DRAW_DOT:
          geom = DRW_cache_particles_get_dots(ob, psys);
          set_color(*dot_ps_);
          dot_ps_->draw(geom, handle, res.select_id(ob_ref).get());
          break;
        case PART_DRAW_AXIS:
          geom = DRW_cache_particles_get_dots(ob, psys);
          set_color(*shape_ps_);
          shape_ps_->push_constant("shape_type", PART_SHAPE_AXIS);
          shape_ps_->draw_expand(geom, GPU_PRIM_LINES, 3, 1, handle, res.select_id(ob_ref).get());
          break;
        case PART_DRAW_CIRC:
          geom = DRW_cache_particles_get_dots(ob, psys);
          set_color(*shape_ps_);
          shape_ps_->push_constant("shape_type", PART_SHAPE_CIRCLE);
          shape_ps_->draw_expand(geom,
                                 GPU_PRIM_LINES,
                                 PARTICLE_SHAPE_CIRCLE_RESOLUTION,
                                 1,
                                 handle,
                                 res.select_id(ob_ref).get());
          break;
        case PART_DRAW_CROSS:
          geom = DRW_cache_particles_get_dots(ob, psys);
          set_color(*shape_ps_);
          shape_ps_->push_constant("shape_type", PART_SHAPE_CROSS);
          shape_ps_->draw_expand(geom, GPU_PRIM_LINES, 3, 1, handle, res.select_id(ob_ref).get());
          break;
      }
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(particle_ps_, view);
  }
};
}  // namespace blender::draw::overlay
