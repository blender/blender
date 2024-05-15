/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_grease_pencil.hh"
#include "BLI_color.hh"
#include "BLI_math_matrix.hh"

#include "BKE_attribute.hh"
#include "BKE_camera.h"
#include "BKE_curves.hh"
#include "BKE_image.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "GPU_debug.hh"
#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_shader_shared.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"
#include "GPU_vertex_format.hh"

namespace blender::ed::greasepencil::image_render {

/* Enable GPU debug capture (needs WITH_RENDERDOC option). */
constexpr const bool enable_debug_gpu_capture = true;

RegionViewData region_init(ARegion &region, const int2 &win_size)
{
  const RegionViewData data = {int2{region.winx, region.winy}, region.winrct};

  /* Resize region. */
  region.winrct.xmin = 0;
  region.winrct.ymin = 0;
  region.winrct.xmax = win_size.x;
  region.winrct.ymax = win_size.y;
  region.winx = short(win_size.x);
  region.winy = short(win_size.y);

  return data;
}

void region_reset(ARegion &region, const RegionViewData &data)
{
  region.winx = data.region_winsize.x;
  region.winy = data.region_winsize.y;
  region.winrct = data.region_winrct;
}

GPUOffScreen *image_render_begin(const int2 &win_size)
{
  if (enable_debug_gpu_capture) {
    GPU_debug_capture_begin("Grease Pencil Image Render");
  }

  char err_out[256] = "unknown";
  GPUOffScreen *offscreen = GPU_offscreen_create(
      win_size.x, win_size.y, true, GPU_RGBA8, GPU_TEXTURE_USAGE_HOST_READ, err_out);
  if (offscreen == nullptr) {
    return nullptr;
  }

  GPU_offscreen_bind(offscreen, true);

  GPU_matrix_push_projection();
  GPU_matrix_identity_projection_set();
  GPU_matrix_push();
  GPU_matrix_identity_set();

  GPU_clear_color(0.0f, 0.0f, 0.0f, 0.0f);
  GPU_clear_depth(1.0f);

  return offscreen;
}

Image *image_render_end(Main &bmain, GPUOffScreen *buffer)
{
  const int2 win_size = {GPU_offscreen_width(buffer), GPU_offscreen_height(buffer)};
  const uint imb_flag = IB_rect;
  ImBuf *ibuf = IMB_allocImBuf(win_size.x, win_size.y, 32, imb_flag);
  if (ibuf->float_buffer.data) {
    GPU_offscreen_read_color(buffer, GPU_DATA_FLOAT, ibuf->float_buffer.data);
  }
  else if (ibuf->byte_buffer.data) {
    GPU_offscreen_read_color(buffer, GPU_DATA_UBYTE, ibuf->byte_buffer.data);
  }
  if (ibuf->float_buffer.data && ibuf->byte_buffer.data) {
    IMB_rect_from_float(ibuf);
  }

  Image *ima = BKE_image_add_from_imbuf(&bmain, ibuf, "Grease Pencil Fill");
  ima->id.tag |= LIB_TAG_DOIT;

  BKE_image_release_ibuf(ima, ibuf, nullptr);

  /* Switch back to regular frame-buffer. */
  GPU_offscreen_unbind(buffer, true);
  GPU_offscreen_free(buffer);

  if (enable_debug_gpu_capture) {
    GPU_debug_capture_end();
  }

  return ima;
}

void set_viewmat(const ViewContext &view_context,
                 const Scene &scene,
                 const int2 &win_size,
                 const float2 &zoom,
                 const float2 &offset)
{
  rctf viewplane;
  float clip_start, clip_end;
  const bool is_ortho = ED_view3d_viewplane_get(view_context.depsgraph,
                                                view_context.v3d,
                                                view_context.rv3d,
                                                win_size.x,
                                                win_size.y,
                                                &viewplane,
                                                &clip_start,
                                                &clip_end,
                                                nullptr);

  /* Rescale `viewplane` to fit all strokes. */
  const float2 view_min = float2(viewplane.xmin, viewplane.ymin);
  const float2 view_max = float2(viewplane.xmax, viewplane.ymax);
  const float2 view_extent = view_max - view_min;
  const float2 view_center = 0.5f * (view_max + view_min);
  const float2 offset_abs = offset * view_extent;
  const float2 view_min_new = (view_min - view_center) * zoom + view_center + offset_abs;
  const float2 view_max_new = (view_max - view_center) * zoom + view_center + offset_abs;
  viewplane.xmin = view_min_new.x;
  viewplane.ymin = view_min_new.y;
  viewplane.xmax = view_max_new.x;
  viewplane.ymax = view_max_new.y;

  float4x4 winmat;
  if (is_ortho) {
    orthographic_m4(winmat.ptr(),
                    viewplane.xmin,
                    viewplane.xmax,
                    viewplane.ymin,
                    viewplane.ymax,
                    -clip_end,
                    clip_end);
  }
  else {
    perspective_m4(winmat.ptr(),
                   viewplane.xmin,
                   viewplane.xmax,
                   viewplane.ymin,
                   viewplane.ymax,
                   clip_start,
                   clip_end);
  }

  ED_view3d_update_viewmat(view_context.depsgraph,
                           &scene,
                           view_context.v3d,
                           view_context.region,
                           nullptr,
                           winmat.ptr(),
                           nullptr,
                           true);
  GPU_matrix_set(view_context.rv3d->viewmat);
  GPU_matrix_projection_set(view_context.rv3d->winmat);
}

void clear_viewmat()
{
  GPU_matrix_pop_projection();
  GPU_matrix_pop();
}

void draw_dot(const float3 &position, const float point_size, const ColorGeometry4f &color)
{
  GPUVertFormat *format = immVertexFormat();
  uint attr_pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  uint attr_size = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  uint attr_color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  GPU_program_point_size(true);
  immBindBuiltinProgram(GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR);
  immBegin(GPU_PRIM_POINTS, 1);
  immAttr1f(attr_size, point_size * M_SQRT2);
  immAttr4fv(attr_color, color);
  immVertex3fv(attr_pos, position);
  immEnd();
  immUnbindProgram();
  GPU_program_point_size(false);
}

void draw_polyline(const IndexRange indices,
                   Span<float3> positions,
                   const VArray<ColorGeometry4f> &colors,
                   const float4x4 &layer_to_world,
                   const bool cyclic,
                   const float line_width)
{
  GPUVertFormat *format = immVertexFormat();
  const uint attr_pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  const uint attr_color = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

  GPU_line_width(line_width);
  /* If cyclic the curve needs one more vertex. */
  const int cyclic_add = (cyclic && indices.size() > 2) ? 1 : 0;
  immBeginAtMost(GPU_PRIM_LINE_STRIP, indices.size() + cyclic_add);

  for (const int point_i : indices) {
    immAttr4fv(attr_color, colors[point_i]);
    immVertex3fv(attr_pos, math::transform_point(layer_to_world, positions[point_i]));
  }

  if (cyclic && indices.size() > 2) {
    const int point_i = indices[0];
    immAttr4fv(attr_color, colors[point_i]);
    immVertex3fv(attr_pos, math::transform_point(layer_to_world, positions[point_i]));
  }

  immEnd();
  immUnbindProgram();
}

static GPUUniformBuf *create_shader_ubo(const RegionView3D &rv3d,
                                        const int2 &win_size,
                                        const Object &object,
                                        const eGPDstroke_Caps cap_start,
                                        const eGPDstroke_Caps cap_end,
                                        const bool is_fill_stroke)
{
  GPencilStrokeData data;
  copy_v2_v2(data.viewport, float2(win_size));
  data.pixsize = rv3d.pixsize;
  data.objscale = math::average(float3(object.scale));
  /* TODO Was based on the GP_DATA_STROKE_KEEPTHICKNESS flag which is currently not converted. */
  data.keep_size = false;
  data.pixfactor = 1.0f;
  /* X-ray mode always to 3D space to avoid wrong Z-depth calculation (#60051). */
  data.xraymode = GP_XRAY_3DSPACE;
  data.caps_start = cap_start;
  data.caps_end = cap_end;
  data.fill_stroke = is_fill_stroke;

  return GPU_uniformbuf_create_ex(sizeof(GPencilStrokeData), &data, __func__);
}

void draw_grease_pencil_stroke(const RegionView3D &rv3d,
                               const int2 &win_size,
                               const Object &object,
                               const IndexRange indices,
                               Span<float3> positions,
                               const VArray<float> &radii,
                               const VArray<ColorGeometry4f> &colors,
                               const float4x4 &layer_to_world,
                               const bool cyclic,
                               const eGPDstroke_Caps cap_start,
                               const eGPDstroke_Caps cap_end,
                               const bool fill_stroke)
{
  if (indices.is_empty()) {
    return;
  }

  GPUVertFormat *format = immVertexFormat();
  const uint attr_pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  const uint attr_color = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  const uint attr_thickness = GPU_vertformat_attr_add(
      format, "thickness", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_GPENCIL_STROKE);
  GPUUniformBuf *ubo = create_shader_ubo(rv3d, win_size, object, cap_start, cap_end, fill_stroke);
  immBindUniformBuf("gpencil_stroke_data", ubo);

  /* If cyclic the curve needs one more vertex. */
  const int cyclic_add = (cyclic && indices.size() > 2) ? 1 : 0;

  immBeginAtMost(GPU_PRIM_LINE_STRIP_ADJ, indices.size() + cyclic_add + 2);

  auto draw_point = [&](const int point_i) {
    constexpr const float radius_to_pixel_factor =
        1.0f / bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR;
    const float thickness = radii[point_i] * radius_to_pixel_factor;
    constexpr const float min_thickness = 0.05f;

    immAttr4fv(attr_color, colors[point_i]);
    immAttr1f(attr_thickness, std::max(thickness, min_thickness));
    immVertex3fv(attr_pos, math::transform_point(layer_to_world, positions[point_i]));
  };

  /* First point for adjacency (not drawn). */
  if (cyclic && indices.size() > 2) {
    draw_point(indices.last() - 1);
  }
  else {
    draw_point(indices.first() + 1);
  }

  for (const int point_i : indices) {
    draw_point(point_i);
  }

  if (cyclic && indices.size() > 2) {
    draw_point(indices.first());
    draw_point(indices.first() + 1);
  }
  /* Last adjacency point (not drawn). */
  else {
    draw_point(indices.last() - 1);
  }

  immEnd();
  immUnbindProgram();

  GPU_uniformbuf_free(ubo);
}

void draw_dots(const IndexRange indices,
               Span<float3> positions,
               const VArray<float> &radii,
               const VArray<ColorGeometry4f> &colors,
               const float4x4 &layer_to_world)
{
  /* TODO */
  UNUSED_VARS(indices, positions, radii, colors, layer_to_world);
}

void draw_grease_pencil_strokes(const RegionView3D &rv3d,
                                const int2 &win_size,
                                const Object &object,
                                const bke::greasepencil::Drawing &drawing,
                                const IndexMask &strokes_mask,
                                const VArray<ColorGeometry4f> &colors,
                                const float4x4 &layer_to_world,
                                const int mode,
                                const bool use_xray,
                                const bool fill_strokes)
{
  GPU_program_point_size(true);

  /* Do not write to depth (avoid self-occlusion). */
  const bool prev_depth_mask = GPU_depth_mask_get();
  GPU_depth_mask(false);

  const bke::CurvesGeometry &curves = drawing.strokes();
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const Span<float3> positions = curves.positions();
  const bke::AttributeAccessor attributes = curves.attributes();
  const VArray<bool> cyclic = curves.cyclic();
  const VArray<float> &radii = drawing.radii();
  const VArray<int8_t> stroke_start_caps = *attributes.lookup_or_default<int8_t>(
      "start_cap", bke::AttrDomain::Curve, GP_STROKE_CAP_ROUND);
  const VArray<int8_t> stroke_end_caps = *attributes.lookup_or_default<int8_t>(
      "end_cap", bke::AttrDomain::Curve, GP_STROKE_CAP_ROUND);

  /* Note: Serial loop without GrainSize, since immediate mode drawing can't happen in worker
   * threads, has to be from the main thread. */
  strokes_mask.foreach_index([&](const int stroke_i) {
    const float stroke_radius = radii[stroke_i];
    if (stroke_radius <= 0) {
      return;
    }

    if (!use_xray) {
      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);

      /* First arg is normally rv3d->dist, but this isn't
       * available here and seems to work quite well without. */
      GPU_polygon_offset(1.0f, 1.0f);
    }

    switch (eMaterialGPencilStyle_Mode(mode)) {
      case GP_MATERIAL_MODE_LINE:
        draw_grease_pencil_stroke(rv3d,
                                  win_size,
                                  object,
                                  points_by_curve[stroke_i],
                                  positions,
                                  radii,
                                  colors,
                                  layer_to_world,
                                  cyclic[stroke_i],
                                  eGPDstroke_Caps(stroke_start_caps[stroke_i]),
                                  eGPDstroke_Caps(stroke_end_caps[stroke_i]),
                                  fill_strokes);
        break;
      case GP_MATERIAL_MODE_DOT:
      case GP_MATERIAL_MODE_SQUARE:
        draw_dots(points_by_curve[stroke_i], positions, radii, colors, layer_to_world);
        break;
    }

    if (!use_xray) {
      GPU_depth_test(GPU_DEPTH_NONE);

      GPU_polygon_offset(0.0f, 0.0f);
    }
  });

  GPU_depth_mask(prev_depth_mask);
  GPU_program_point_size(false);
}

}  // namespace blender::ed::greasepencil::image_render
