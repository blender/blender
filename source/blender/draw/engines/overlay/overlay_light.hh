/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "DNA_light_types.h"

#include "BLI_math_matrix.h"

#include "overlay_base.hh"

namespace blender::draw::overlay {

class Lights : Overlay {
  using LightInstanceBuf = ShapeInstanceBuf<ExtraInstanceData>;
  using GroundLineInstanceBuf = ShapeInstanceBuf<float4>;

 private:
  const SelectionType selection_type_;

  PassSimple ps_ = {"Lights"};

  struct CallBuffers {
    const SelectionType selection_type_;
    GroundLineInstanceBuf ground_line_buf = {selection_type_, "ground_line_buf"};
    LightInstanceBuf icon_inner_buf = {selection_type_, "icon_inner_buf"};
    LightInstanceBuf icon_outer_buf = {selection_type_, "icon_outer_buf"};
    LightInstanceBuf icon_sun_rays_buf = {selection_type_, "icon_sun_rays_buf"};
    LightInstanceBuf point_buf = {selection_type_, "point_buf"};
    LightInstanceBuf sun_buf = {selection_type_, "sun_buf"};
    LightInstanceBuf spot_buf = {selection_type_, "spot_buf"};
    LightInstanceBuf spot_cone_back_buf = {selection_type_, "spot_cone_back_buf"};
    LightInstanceBuf spot_cone_front_buf = {selection_type_, "spot_cone_front_buf"};
    LightInstanceBuf area_disk_buf = {selection_type_, "area_disk_buf"};
    LightInstanceBuf area_square_buf = {selection_type_, "area_square_buf"};
  } call_buffers_{selection_type_};

 public:
  Lights(const SelectionType selection_type) : selection_type_(selection_type) {};

  void begin_sync(Resources & /*res*/, const State &state) final
  {
    enabled_ = state.is_space_v3d() && state.show_extras();
    if (!enabled_) {
      return;
    }

    call_buffers_.ground_line_buf.clear();
    call_buffers_.icon_inner_buf.clear();
    call_buffers_.icon_outer_buf.clear();
    call_buffers_.icon_sun_rays_buf.clear();
    call_buffers_.point_buf.clear();
    call_buffers_.sun_buf.clear();
    call_buffers_.spot_buf.clear();
    call_buffers_.spot_cone_back_buf.clear();
    call_buffers_.spot_cone_front_buf.clear();
    call_buffers_.area_disk_buf.clear();
    call_buffers_.area_square_buf.clear();
  }

  void object_sync(Manager & /*manager*/,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    ExtraInstanceData data(ob_ref.object->object_to_world(),
                           float4(res.object_wire_color(ob_ref, state).xyz(), 1.0f),
                           1.0f);
    float4 &theme_color = data.color_;

    /* Pack render data into object matrix. */
    float4x4 &matrix = data.object_to_world;
    float &area_size_x = matrix[0].w;
    float &area_size_y = matrix[1].w;
    float &spot_cosine = matrix[0].w;
    float &spot_blend = matrix[1].w;
    float &clip_start = matrix[2].w;
    float &clip_end = matrix[3].w;

    const Light &la = DRW_object_get_data_for_drawing<Light>(*ob_ref.object);
    const select::ID select_id = res.select_id(ob_ref);

    /* FIXME / TODO: clip_end has no meaning nowadays.
     * In EEVEE, Only clip_start is used shadow-mapping.
     * Clip end is computed automatically based on light power.
     * For now, always use the custom distance as clip_end. */
    clip_end = la.att_dist;
    clip_start = la.clipsta;

    call_buffers_.ground_line_buf.append(float4(matrix.location(), 0.0f), select_id);

    const float4 light_color = {la.r, la.g, la.b, 1.0f};
    const bool show_light_colors = state.show_light_colors();

    /* Draw the outer ring of the light icon and the sun rays in `light_color`, if required. */
    call_buffers_.icon_outer_buf.append(data, select_id);
    call_buffers_.icon_inner_buf.append(show_light_colors ? data.with_color(light_color) : data,
                                        select_id);

    switch (la.type) {
      case LA_LOCAL:
        area_size_x = area_size_y = la.radius;
        call_buffers_.point_buf.append(data, select_id);
        break;
      case LA_SUN:
        call_buffers_.sun_buf.append(data, select_id);
        call_buffers_.icon_sun_rays_buf.append(
            show_light_colors ? data.with_color(light_color) : data, select_id);
        break;
      case LA_SPOT: {
        /* Previous implementation was using the clip-end distance as cone size.
         * We cannot do this anymore so we use a fixed size of 10. (see #72871) */
        rescale_m4(matrix.ptr(), float3(10.0f, 10.0f, 10.0f));
        /* For cycles and EEVEE the spot attenuation is:
         * `y = (1/sqrt(1 + x^2) - a)/((1 - a) b)`
         * x being the tangent of the angle between the light direction and the generatrix of the
         * cone. We solve the case where spot attenuation y = 1 and y = 0 root for y = 1 is
         * `sqrt(1/c^2 - 1)`. root for y = 0 is `sqrt(1/a^2 - 1)` and use that to position the
         * blend circle. */
        const float a = cosf(la.spotsize * 0.5f);
        const float b = la.spotblend;
        const float c = a * b - a - b;
        const float a2 = a * a;
        const float c2 = c * c;
        /* Optimized version or root1 / root0 */
        spot_blend = sqrtf((a2 - a2 * c2) / (c2 - a2 * c2));
        spot_cosine = a;
        /* HACK: We pack the area size in alpha color. This is decoded by the shader. */
        theme_color[3] = -max_ff(la.radius, FLT_MIN);
        call_buffers_.spot_buf.append(data, select_id);
        if ((la.mode & LA_SHOW_CONE) && !res.is_selection()) {
          const float4 color_inside{0.0f, 0.0f, 0.0f, 0.5f};
          const float4 color_outside{1.0f, 1.0f, 1.0f, 0.3f};
          call_buffers_.spot_cone_front_buf.append(data.with_color(color_inside), select_id);
          call_buffers_.spot_cone_back_buf.append(data.with_color(color_outside), select_id);
        }
        break;
      }
      case LA_AREA:
        const bool uniform_scale = !ELEM(la.area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE);
        LightInstanceBuf &area_buf = ELEM(la.area_shape, LA_AREA_SQUARE, LA_AREA_RECT) ?
                                         call_buffers_.area_square_buf :
                                         call_buffers_.area_disk_buf;
        area_size_x = la.area_size;
        area_size_y = uniform_scale ? la.area_size : la.area_sizey;
        area_buf.append(data, select_id);
        break;
    }
  }

  void end_sync(Resources &res, const State &state) final
  {
    if (!enabled_) {
      return;
    }

    const DRWState pass_state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                DRW_STATE_DEPTH_LESS_EQUAL;
    ps_.init();
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    res.select_bind(ps_);

    {
      PassSimple::Sub &sub_pass = ps_.sub("spot_cone_front");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA |
                             DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_FRONT,
                         state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->light_spot_cone.get());
      call_buffers_.spot_cone_front_buf.end_sync(sub_pass, res.shapes.light_spot_volume.get());
    }
    {
      PassSimple::Sub &sub_pass = ps_.sub("spot_cone_back");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA |
                             DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK,
                         state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->light_spot_cone.get());
      call_buffers_.spot_cone_back_buf.end_sync(sub_pass, res.shapes.light_spot_volume.get());
    }
    {
      PassSimple::Sub &sub_pass = ps_.sub("light_shapes");
      sub_pass.state_set(pass_state, state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->extra_shape.get());
      call_buffers_.icon_inner_buf.end_sync(sub_pass, res.shapes.light_icon_outer_lines.get());
      call_buffers_.icon_outer_buf.end_sync(sub_pass, res.shapes.light_icon_inner_lines.get());
      call_buffers_.icon_sun_rays_buf.end_sync(sub_pass, res.shapes.light_icon_sun_rays.get());
      call_buffers_.point_buf.end_sync(sub_pass, res.shapes.light_point_lines.get());
      call_buffers_.sun_buf.end_sync(sub_pass, res.shapes.light_sun_lines.get());
      call_buffers_.spot_buf.end_sync(sub_pass, res.shapes.light_spot_lines.get());
      call_buffers_.area_disk_buf.end_sync(sub_pass, res.shapes.light_area_disk_lines.get());
      call_buffers_.area_square_buf.end_sync(sub_pass, res.shapes.light_area_square_lines.get());
    }
    {
      PassSimple::Sub &sub_pass = ps_.sub("ground_line");
      sub_pass.state_set(pass_state | DRW_STATE_BLEND_ALPHA, state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->extra_ground_line.get());
      call_buffers_.ground_line_buf.end_sync(sub_pass, res.shapes.ground_line.get());
    }
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};

}  // namespace blender::draw::overlay
