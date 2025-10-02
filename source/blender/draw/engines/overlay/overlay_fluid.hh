/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "DNA_fluid_types.h"
#include "DNA_modifier_types.h"

#include "BKE_modifier.hh"

#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Draw fluid simulation overlays (water, smoke).
 */
class Fluids : Overlay {
 private:
  const SelectionType selection_type_;

  PassSimple fluid_ps_ = {"fluid_ps_"};
  PassSimple::Sub *velocity_needle_ps_ = nullptr;
  PassSimple::Sub *velocity_mac_ps_ = nullptr;
  PassSimple::Sub *velocity_streamline_ps_ = nullptr;
  PassSimple::Sub *grid_lines_flags_ps_ = nullptr;
  PassSimple::Sub *grid_lines_flat_ps_ = nullptr;
  PassSimple::Sub *grid_lines_range_ps_ = nullptr;

  ShapeInstanceBuf<ExtraInstanceData> cube_buf_ = {selection_type_, "cube_buf_"};

  int dominant_axis = -1;

 public:
  Fluids(const SelectionType selection_type) : selection_type_(selection_type) {};

  void begin_sync(Resources &res, const State &state) final
  {
    /* Against design. Should not sync depending on view. */
    float3 camera_direction = blender::draw::View::default_get().viewinv().z_axis();
    dominant_axis = math::dominant_axis(camera_direction);

    {
      auto &pass = fluid_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                     state.clipping_plane_count);
      res.select_bind(pass);

      /* TODO(fclem): Use either specialization constants or push constants to reduce the amount of
       * shader variants. */
      velocity_needle_ps_ = &fluid_ps_.sub("Velocity Needles");
      velocity_needle_ps_->shader_set(res.shaders->fluid_velocity_needle.get());

      velocity_mac_ps_ = &fluid_ps_.sub("Velocity Mac");
      velocity_mac_ps_->shader_set(res.shaders->fluid_velocity_mac.get());

      velocity_streamline_ps_ = &fluid_ps_.sub("Velocity Line");
      velocity_streamline_ps_->shader_set(res.shaders->fluid_velocity_streamline.get());

      grid_lines_flags_ps_ = &fluid_ps_.sub("Velocity Mac");
      grid_lines_flags_ps_->shader_set(res.shaders->fluid_grid_lines_flags.get());

      grid_lines_flat_ps_ = &fluid_ps_.sub("Velocity Needles");
      grid_lines_flat_ps_->shader_set(res.shaders->fluid_grid_lines_flat.get());

      grid_lines_range_ps_ = &fluid_ps_.sub("Velocity Line");
      grid_lines_range_ps_->shader_set(res.shaders->fluid_grid_lines_range.get());
    }

    cube_buf_.clear();
  }

  void object_sync(Manager &manager,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    Object *ob = ob_ref.object;

    /* Do not show for dupli objects as the fluid is baked for the original object. */
    if (is_from_dupli_or_set(ob)) {
      return;
    }

    /* NOTE: There can only be one fluid modifier per object. */
    ModifierData *md = BKE_modifiers_findby_type(ob, eModifierType_Fluid);

    if (md == nullptr) {
      return;
    }

    FluidModifierData *fmd = (FluidModifierData *)md;
    FluidDomainSettings *fds = fmd->domain;

    if (fds == nullptr) {
      return;
    }

    const bool is_active_frame_after_cache_start = state.scene->r.cfra >= fds->cache_frame_start;
    const bool is_active_frame_before_cache_end = state.scene->r.cfra <= fds->cache_frame_end;
    const bool is_active_frame_in_cache_range = is_active_frame_after_cache_start &&
                                                is_active_frame_before_cache_end;
    if (!is_active_frame_in_cache_range) {
      return;
    }

    ResourceHandleRange res_handle = manager.unique_handle(ob_ref);
    select::ID sel_id = res.select_id(ob_ref);

    /* Small cube showing voxel size. */
    {
      float3 min = float3(fds->p0) + float3(fds->cell_size) * float3(int3(fds->res_min));
      float4x4 voxel_cube_mat = math::from_loc_scale<float4x4>(min, float3(fds->cell_size) / 2.0f);
      /* Move small cube into the domain, otherwise its centered on corner of domain object. */
      voxel_cube_mat = math::translate(voxel_cube_mat, float3(1.0f));
      voxel_cube_mat = ob->object_to_world() * voxel_cube_mat;

      const float4 &color = res.object_wire_color(ob_ref, state);
      cube_buf_.append({voxel_cube_mat, color, 1.0f}, sel_id);
    }

    /* No volume data to display. */
    if (fds->fluid == nullptr) {
      return;
    }

    int slice_axis = slide_axis_get(*fds);

    if (fds->draw_velocity) {
      int lines_per_voxel = -1;
      PassSimple::Sub *sub_pass = nullptr;
      switch (fds->vector_draw_type) {
        default:
        case VECTOR_DRAW_STREAMLINE:
          sub_pass = velocity_streamline_ps_;
          lines_per_voxel = 1;
          break;
        case VECTOR_DRAW_NEEDLE:
          sub_pass = velocity_needle_ps_;
          lines_per_voxel = 6;
          break;
        case VECTOR_DRAW_MAC:
          sub_pass = velocity_mac_ps_;
          lines_per_voxel = 3;
          break;
      }

      int total_lines = lines_per_voxel * math::reduce_mul(int3(fds->res));
      if (slice_axis != -1) {
        /* Remove the sliced dimension. */
        total_lines /= fds->res[slice_axis];
      }

      DRW_smoke_ensure_velocity(fmd);

      PassSimple::Sub &sub = *sub_pass;
      sub.bind_texture("velocity_x", fds->tex_velocity_x);
      sub.bind_texture("velocity_y", fds->tex_velocity_y);
      sub.bind_texture("velocity_z", fds->tex_velocity_z);
      sub.push_constant("display_size", fds->vector_scale);
      sub.push_constant("slice_position", fds->slice_depth);
      sub.push_constant("cell_size", float3(fds->cell_size));
      sub.push_constant("domain_origin_offset", float3(fds->p0));
      sub.push_constant("adaptive_cell_offset", int3(fds->res_min));
      sub.push_constant("slice_axis", slice_axis);
      sub.push_constant("scale_with_magnitude", bool(fds->vector_scale_with_magnitude));
      sub.push_constant("is_cell_centered",
                        (fds->vector_field == FLUID_DOMAIN_VECTOR_FIELD_FORCE));
      if (fds->vector_draw_type == VECTOR_DRAW_MAC) {
        sub.push_constant("draw_macx", (fds->vector_draw_mac_components & VECTOR_DRAW_MAC_X));
        sub.push_constant("draw_macy", (fds->vector_draw_mac_components & VECTOR_DRAW_MAC_Y));
        sub.push_constant("draw_macz", (fds->vector_draw_mac_components & VECTOR_DRAW_MAC_Z));
      }
      sub.push_constant("in_select_id", int(sel_id.get()));
      sub.draw_procedural(GPU_PRIM_LINES, 1, total_lines * 2, -1, res_handle);
    }

    /* Show gridlines only for slices with no interpolation. */
    const bool show_gridlines = fds->show_gridlines &&
                                (fds->axis_slice_method == AXIS_SLICE_SINGLE) &&
                                (fds->interp_method == FLUID_DISPLAY_INTERP_CLOSEST ||
                                 fds->coba_field == FLUID_DOMAIN_FIELD_FLAGS);
    if (show_gridlines) {
      PassSimple::Sub *sub_pass = nullptr;
      switch (fds->gridlines_color_field) {
        default:
        case FLUID_GRIDLINE_COLOR_TYPE_FLAGS:
          DRW_fluid_ensure_flags(fmd);

          sub_pass = grid_lines_flags_ps_;
          sub_pass->bind_texture("flag_tx", fds->tex_flags);
          break;
        case FLUID_GRIDLINE_COLOR_TYPE_RANGE:
          if (fds->use_coba && (fds->coba_field != FLUID_DOMAIN_FIELD_FLAGS)) {
            DRW_fluid_ensure_flags(fmd);
            DRW_fluid_ensure_range_field(fmd);

            sub_pass = grid_lines_range_ps_;
            sub_pass->bind_texture("flag_tx", fds->tex_flags);
            sub_pass->bind_texture("field_tx", fds->tex_range_field);
            sub_pass->push_constant("lower_bound", fds->gridlines_lower_bound);
            sub_pass->push_constant("upper_bound", fds->gridlines_upper_bound);
            sub_pass->push_constant("range_color", float4(fds->gridlines_range_color));
            sub_pass->push_constant("cell_filter", int(fds->gridlines_cell_filter));
            break;
          }
          /* Otherwise, fall back to none color type. */
          ATTR_FALLTHROUGH;
        case FLUID_GRIDLINE_COLOR_TYPE_NONE:
          sub_pass = grid_lines_flat_ps_;
          break;
      }

      PassSimple::Sub &sub = *sub_pass;
      sub.push_constant("volume_size", int3(fds->res));
      sub.push_constant("slice_position", fds->slice_depth);
      sub.push_constant("cell_size", float3(fds->cell_size));
      sub.push_constant("domain_origin_offset", float3(fds->p0));
      sub.push_constant("adaptive_cell_offset", int3(fds->res_min));
      sub.push_constant("slice_axis", slice_axis);
      sub.push_constant("in_select_id", int(sel_id.get()));

      BLI_assert(slice_axis != -1);
      int lines_per_voxel = 4;
      int total_lines = lines_per_voxel * math::reduce_mul(int3(fds->res)) / fds->res[slice_axis];
      sub.draw_procedural(GPU_PRIM_LINES, 1, total_lines * 2, -1, res_handle);
    }
  }

  void end_sync(Resources &res, const State & /*state*/) final
  {
    fluid_ps_.shader_set(res.shaders->extra_shape.get());
    fluid_ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    fluid_ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);

    cube_buf_.end_sync(fluid_ps_, res.shapes.cube.get());
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    GPU_framebuffer_bind(framebuffer);
    manager.submit(fluid_ps_, view);
  }

 private:
  /* Return axis index or -1 if no slice. */
  int slide_axis_get(FluidDomainSettings &fluid_domain_settings) const
  {
    if (fluid_domain_settings.axis_slice_method != AXIS_SLICE_SINGLE) {
      return -1;
    }
    if (fluid_domain_settings.slice_axis == SLICE_AXIS_AUTO) {
      return dominant_axis;
    }
    return fluid_domain_settings.slice_axis - 1;
  }
};

}  // namespace blender::draw::overlay
