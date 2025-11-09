/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "DNA_brush_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_color.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_hash.h"
#include "BLI_math_color_blend.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"

#include "IMB_colormanagement.hh"

#include "mesh_brush_common.hh"
#include "sculpt_automask.hh"
#include "sculpt_color.hh"
#include "sculpt_intern.hh"
#include "sculpt_smooth.hh"

#include "IMB_imbuf.hh"

#include <cmath>

namespace blender::ed::sculpt_paint::color {

static void calc_local_positions(const float4x4 &mat,
                                 const Span<int> verts,
                                 const Span<float3> positions,
                                 const MutableSpan<float3> local_positions)
{
  for (const int i : verts.index_range()) {
    local_positions[i] = math::transform_point(mat, positions[verts[i]]);
  }
}

template<typename Func> inline void to_static_color_type(const CPPType &type, const Func &func)
{
  if (type.is<ColorGeometry4f>()) {
    func(MPropCol());
  }
  else if (type.is<ColorGeometry4b>()) {
    func(MLoopCol());
  }
}

template<typename T> float4 to_float(const T &src);

template<> float4 to_float(const MLoopCol &src)
{
  float4 dst;
  rgba_uchar_to_float(dst, reinterpret_cast<const uchar *>(&src));
  srgb_to_linearrgb_v3_v3(dst, dst);
  return dst;
}
template<> float4 to_float(const MPropCol &src)
{
  return src.color;
}

template<typename T> void from_float(const float4 &src, T &dst);

template<> void from_float(const float4 &src, MLoopCol &dst)
{
  float4 temp;
  linearrgb_to_srgb_v3_v3(temp, src);
  temp[3] = src[3];
  rgba_float_to_uchar(reinterpret_cast<uchar *>(&dst), temp);
}
template<> void from_float(const float4 &src, MPropCol &dst)
{
  copy_v4_v4(dst.color, src);
}

template<typename T>
static float4 color_vert_get(const OffsetIndices<int> faces,
                             const Span<int> corner_verts,
                             const GroupedSpan<int> vert_to_face_map,
                             const GSpan color_attribute,
                             const bke::AttrDomain color_domain,
                             const int vert)
{
  const T *colors_typed = static_cast<const T *>(color_attribute.data());
  if (color_domain == bke::AttrDomain::Corner) {
    float4 r_color(0.0f);
    for (const int face : vert_to_face_map[vert]) {
      const int corner = bke::mesh::face_find_corner_from_vert(faces[face], corner_verts, vert);
      r_color += to_float(colors_typed[corner]);
    }
    return r_color / float(vert_to_face_map[vert].size());
  }
  return to_float(colors_typed[vert]);
}

template<typename T>
static void color_vert_set(const OffsetIndices<int> faces,
                           const Span<int> corner_verts,
                           const GroupedSpan<int> vert_to_face_map,
                           const GMutableSpan color_attribute,
                           const bke::AttrDomain color_domain,
                           const int vert,
                           const float4 &color)
{
  if (color_domain == bke::AttrDomain::Corner) {
    for (const int i_face : vert_to_face_map[vert]) {
      const IndexRange face = faces[i_face];
      MutableSpan<T> colors{static_cast<T *>(color_attribute.data()) + face.start(), face.size()};
      Span<int> face_verts = corner_verts.slice(face);

      for (const int i : IndexRange(face.size())) {
        if (face_verts[i] == vert) {
          from_float(color, colors[i]);
        }
      }
    }
  }
  else {
    from_float(color, static_cast<T *>(color_attribute.data())[vert]);
  }
}

float4 color_vert_get(const OffsetIndices<int> faces,
                      const Span<int> corner_verts,
                      const GroupedSpan<int> vert_to_face_map,
                      const GSpan color_attribute,
                      const bke::AttrDomain color_domain,
                      const int vert)
{
  float4 color;
  to_static_color_type(color_attribute.type(), [&](auto dummy) {
    using T = decltype(dummy);
    color = color_vert_get<T>(
        faces, corner_verts, vert_to_face_map, color_attribute, color_domain, vert);
  });
  return color;
}

void color_vert_set(const OffsetIndices<int> faces,
                    const Span<int> corner_verts,
                    const GroupedSpan<int> vert_to_face_map,
                    const bke::AttrDomain color_domain,
                    const int vert,
                    const float4 &color,
                    const GMutableSpan color_attribute)
{
  to_static_color_type(color_attribute.type(), [&](auto dummy) {
    using T = decltype(dummy);
    color_vert_set<T>(
        faces, corner_verts, vert_to_face_map, color_attribute, color_domain, vert, color);
  });
}

void swap_gathered_colors(const Span<int> indices,
                          GMutableSpan color_attribute,
                          MutableSpan<float4> r_colors)
{
  to_static_color_type(color_attribute.type(), [&](auto dummy) {
    using T = decltype(dummy);
    T *colors_typed = static_cast<T *>(color_attribute.data());
    for (const int i : indices.index_range()) {
      T temp = colors_typed[indices[i]];
      from_float(r_colors[i], colors_typed[indices[i]]);
      r_colors[i] = to_float(temp);
    }
  });
}

void gather_colors(const GSpan color_attribute,
                   const Span<int> indices,
                   MutableSpan<float4> r_colors)
{
  to_static_color_type(color_attribute.type(), [&](auto dummy) {
    using T = decltype(dummy);
    const T *colors_typed = static_cast<const T *>(color_attribute.data());
    for (const int i : indices.index_range()) {
      r_colors[i] = to_float(colors_typed[indices[i]]);
    }
  });
}

void gather_colors_vert(const OffsetIndices<int> faces,
                        const Span<int> corner_verts,
                        const GroupedSpan<int> vert_to_face_map,
                        const GSpan color_attribute,
                        const bke::AttrDomain color_domain,
                        const Span<int> verts,
                        const MutableSpan<float4> r_colors)
{
  if (color_domain == bke::AttrDomain::Point) {
    gather_colors(color_attribute, verts, r_colors);
  }
  else {
    to_static_color_type(color_attribute.type(), [&](auto dummy) {
      using T = decltype(dummy);
      for (const int i : verts.index_range()) {
        r_colors[i] = color_vert_get<T>(
            faces, corner_verts, vert_to_face_map, color_attribute, color_domain, verts[i]);
      }
    });
  }
}

bke::GAttributeReader active_color_attribute(const Mesh &mesh)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  const StringRef name = mesh.active_color_attribute;
  const bke::GAttributeReader colors = attributes.lookup(name);
  if (!colors) {
    return {};
  }
  const bke::AttrType data_type = bke::cpp_type_to_attribute_type(colors.varray.type());
  if (!bke::mesh::is_color_attribute({colors.domain, data_type})) {
    return {};
  }
  return colors;
}

bke::GSpanAttributeWriter active_color_attribute_for_write(Mesh &mesh)
{
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  const StringRef name = mesh.active_color_attribute;
  bke::GSpanAttributeWriter colors = attributes.lookup_for_write_span(name);
  if (!colors) {
    return {};
  }
  const bke::AttrType data_type = bke::cpp_type_to_attribute_type(colors.span.type());
  if (!bke::mesh::is_color_attribute({colors.domain, data_type})) {
    colors.finish();
    return {};
  }
  return colors;
}

struct ColorPaintLocalData {
  Vector<float> factors;
  Vector<float> auto_mask;
  Vector<float3> positions;
  Vector<float> distances;
  Vector<float4> colors;
  Vector<float4> new_colors;
  Vector<float4> mix_colors;
  Vector<int> neighbor_offsets;
  Vector<int> neighbor_data;
};

static void do_color_smooth_task(const Depsgraph &depsgraph,
                                 const Object &object,
                                 const Span<float3> vert_positions,
                                 const Span<float3> vert_normals,
                                 const OffsetIndices<int> faces,
                                 const Span<int> corner_verts,
                                 const GroupedSpan<int> vert_to_face_map,
                                 const MeshAttributeData &attribute_data,
                                 const Brush &brush,
                                 const bke::pbvh::MeshNode &node,
                                 ColorPaintLocalData &tls,
                                 bke::GSpanAttributeWriter &color_attribute)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const Span<int> verts = node.verts();

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, vert_positions, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, vert_positions, verts, factors);
  scale_factors(factors, cache.bstrength);

  tls.colors.resize(verts.size());
  MutableSpan<float4> colors = tls.colors;
  for (const int i : verts.index_range()) {
    colors[i] = color_vert_get(faces,
                               corner_verts,
                               vert_to_face_map,
                               color_attribute.span,
                               color_attribute.domain,
                               verts[i]);
  }

  const GroupedSpan<int> neighbors = calc_vert_neighbors(faces,
                                                         corner_verts,
                                                         vert_to_face_map,
                                                         attribute_data.hide_poly,
                                                         verts,
                                                         tls.neighbor_offsets,
                                                         tls.neighbor_data);

  tls.new_colors.resize(verts.size());
  MutableSpan<float4> new_colors = tls.new_colors;
  smooth::neighbor_color_average(faces,
                                 corner_verts,
                                 vert_to_face_map,
                                 color_attribute.span,
                                 color_attribute.domain,
                                 neighbors,
                                 new_colors);

  for (const int i : colors.index_range()) {
    blend_color_interpolate_float(new_colors[i], colors[i], new_colors[i], factors[i]);
  }

  for (const int i : verts.index_range()) {
    color_vert_set(faces,
                   corner_verts,
                   vert_to_face_map,
                   color_attribute.domain,
                   verts[i],
                   new_colors[i],
                   color_attribute.span);
  }
}

static void do_paint_brush_task(const Depsgraph &depsgraph,
                                Object &object,
                                const Span<float3> vert_positions,
                                const Span<float3> vert_normals,
                                const OffsetIndices<int> faces,
                                const Span<int> corner_verts,
                                const GroupedSpan<int> vert_to_face_map,
                                const MeshAttributeData &attribute_data,
                                const Paint &paint,
                                const Brush &brush,
                                const float4x4 &mat,
                                const float4 wet_mix_sampled_color,
                                bke::pbvh::MeshNode &node,
                                ColorPaintLocalData &tls,
                                const MutableSpan<float4> mix_colors,
                                bke::GSpanAttributeWriter &color_attribute)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;

  const float bstrength = fabsf(ss.cache->bstrength);
  const float alpha = BKE_brush_alpha_get(&paint, &brush);

  const Span<int> verts = node.verts();

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, vert_positions, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  float radius;

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  if (brush.tip_roundness < 1.0f) {
    tls.positions.resize(verts.size());
    calc_local_positions(mat, verts, vert_positions, tls.positions);
    calc_brush_cube_distances<float3>(brush, tls.positions, distances);
    radius = 1.0f;
  }
  else {
    calc_brush_distances(
        ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
    radius = cache.radius;
  }
  filter_distances_with_radius(radius, distances, factors);
  apply_hardness_to_distances(radius, cache.hardness, distances);
  BKE_brush_calc_curve_factors(eBrushCurvePreset(brush.curve_distance_falloff_preset),
                               brush.curve_distance_falloff,
                               distances,
                               radius,
                               factors);

  MutableSpan<float> auto_mask;
  if (cache.automasking) {
    tls.auto_mask.resize(verts.size());
    auto_mask = tls.auto_mask;
    auto_mask.fill(1.0f);
    auto_mask::calc_vert_factors(depsgraph, object, *cache.automasking, node, verts, auto_mask);
    scale_factors(factors, auto_mask);
  }

  calc_brush_texture_factors(ss, brush, vert_positions, verts, factors);
  scale_factors(factors, bstrength);

  const float density = ss.cache->paint_brush.density;
  if (density < 1.0f) {
    BLI_assert(ss.cache->paint_brush.density_seed);
    const float seed = ss.cache->paint_brush.density_seed.value_or(0.0f);
    for (const int i : verts.index_range()) {
      const float hash_noise = BLI_hash_int_01(seed * 1000 * verts[i]);
      if (hash_noise > density) {
        const float noise = density * hash_noise;
        factors[i] *= noise;
      }
    }
  }

  float3 brush_color_rgb = ss.cache->invert ? BKE_brush_secondary_color_get(&paint, &brush) :
                                              BKE_brush_color_get(&paint, &brush);

  const std::optional<BrushColorJitterSettings> color_jitter_settings =
      BKE_brush_color_jitter_get_settings(&paint, &brush);
  if (color_jitter_settings) {
    brush_color_rgb = BKE_paint_randomize_color(*color_jitter_settings,
                                                *ss.cache->initial_hsv_jitter,
                                                ss.cache->stroke_distance,
                                                ss.cache->pressure,
                                                brush_color_rgb);
  }

  float4 brush_color(brush_color_rgb, 1.0f);

  const Span<float4> orig_colors = orig_color_data_get_mesh(object, node);

  MutableSpan<float4> color_buffer = gather_data_mesh(mix_colors.as_span(), verts, tls.mix_colors);

  if (brush.flag & BRUSH_USE_GRADIENT) {
    switch (brush.gradient_stroke_mode) {
      case BRUSH_GRADIENT_PRESSURE:
        BKE_colorband_evaluate(brush.gradient, ss.cache->pressure, brush_color);
        break;
      case BRUSH_GRADIENT_SPACING_REPEAT: {
        float coord = fmod(ss.cache->stroke_distance / brush.gradient_spacing, 1.0);
        BKE_colorband_evaluate(brush.gradient, coord, brush_color);
        break;
      }
      case BRUSH_GRADIENT_SPACING_CLAMP: {
        BKE_colorband_evaluate(
            brush.gradient, ss.cache->stroke_distance / brush.gradient_spacing, brush_color);
        break;
      }
    }
  }

  tls.new_colors.resize(verts.size());
  MutableSpan<float4> new_colors = tls.new_colors;
  for (const int i : verts.index_range()) {
    new_colors[i] = color_vert_get(faces,
                                   corner_verts,
                                   vert_to_face_map,
                                   color_attribute.span,
                                   color_attribute.domain,
                                   verts[i]);
  }

  for (const int i : verts.index_range()) {
    /* Brush paint color, brush test falloff and flow. */
    float4 paint_color = brush_color * factors[i] * ss.cache->paint_brush.flow;
    float4 wet_mix_color = wet_mix_sampled_color * factors[i] * ss.cache->paint_brush.flow;

    /* Interpolate with the wet_mix color for wet paint mixing. */
    blend_color_interpolate_float(
        paint_color, paint_color, wet_mix_color, ss.cache->paint_brush.wet_mix);
    blend_color_mix_float(color_buffer[i], color_buffer[i], paint_color);

    /* Final mix over the original color using brush alpha. We apply auto-making again
     * at this point to avoid washing out non-binary masking modes like cavity masking. */
    float automasking = auto_mask.is_empty() ? 1.0f : auto_mask[i];
    const float4 buffer_color = float4(color_buffer[i]) * alpha * automasking;

    IMB_blend_color_float(new_colors[i], orig_colors[i], buffer_color, IMB_BlendMode(brush.blend));
    new_colors[i] = math::clamp(new_colors[i], 0.0f, 1.0f);
  }

  scatter_data_mesh(color_buffer.as_span(), verts, mix_colors);

  for (const int i : verts.index_range()) {
    color_vert_set(faces,
                   corner_verts,
                   vert_to_face_map,
                   color_attribute.domain,
                   verts[i],
                   new_colors[i],
                   color_attribute.span);
  }
}

struct SampleWetPaintData {
  int tot_samples;
  float4 color;
};

static void do_sample_wet_paint_task(const Object &object,
                                     const Span<float3> vert_positions,
                                     const OffsetIndices<int> faces,
                                     const Span<int> corner_verts,
                                     const GroupedSpan<int> vert_to_face_map,
                                     const Span<bool> hide_vert,
                                     const GSpan color_attribute,
                                     const bke::AttrDomain color_domain,
                                     const Brush &brush,
                                     const bke::pbvh::MeshNode &node,
                                     ColorPaintLocalData &tls,
                                     SampleWetPaintData &swptd)
{
  const SculptSession &ss = *object.sculpt;
  const float radius = ss.cache->radius * brush.wet_paint_radius_factor;

  const Span<int> verts = node.verts();

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide(hide_vert, verts, factors);

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(radius, distances, factors);

  for (const int i : verts.index_range()) {
    if (factors[i] > 0.0f) {
      swptd.color += color_vert_get(
          faces, corner_verts, vert_to_face_map, color_attribute, color_domain, verts[i]);
      swptd.tot_samples++;
    }
  }
}

void do_paint_brush(const Depsgraph &depsgraph,
                    PaintModeSettings &paint_mode_settings,
                    const Sculpt &sd,
                    Object &ob,
                    const IndexMask &node_mask,
                    const IndexMask &texnode_mask)
{
  if (SCULPT_use_image_paint_brush(paint_mode_settings, ob)) {
    SCULPT_do_paint_brush_image(depsgraph, paint_mode_settings, sd, ob, texnode_mask);
    return;
  }

  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  SculptSession &ss = *ob.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();

  if (!ss.cache->paint_brush.density_seed) {
    ss.cache->paint_brush.density_seed = BLI_hash_int_01(ss.cache->location_symm[0] * 1000);
  }

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(*ss.cache)) {
    return;
  }

  BKE_curvemapping_init(brush.curve_distance_falloff);

  float4x4 mat;

  /* If the brush is round the tip does not need to be aligned to the surface, so this saves a
   * whole iteration over the affected nodes. */
  if (brush.tip_roundness < 1.0f) {
    SCULPT_cube_tip_init(sd, ob, brush, mat.ptr());

    if (is_zero_m4(mat.ptr())) {
      return;
    }
  }

  Mesh &mesh = *static_cast<Mesh *>(ob.data);
  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const MeshAttributeData attribute_data(mesh);
  bke::GSpanAttributeWriter color_attribute = active_color_attribute_for_write(mesh);
  if (!color_attribute) {
    return;
  }

  if (ss.cache->alt_smooth) {
    threading::EnumerableThreadSpecific<ColorPaintLocalData> all_tls;
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      ColorPaintLocalData &tls = all_tls.local();
      do_color_smooth_task(depsgraph,
                           ob,
                           vert_positions,
                           vert_normals,
                           faces,
                           corner_verts,
                           vert_to_face_map,
                           attribute_data,
                           brush,
                           nodes[i],
                           tls,
                           color_attribute);
    });
    pbvh.tag_attribute_changed(node_mask, mesh.active_color_attribute);
    color_attribute.finish();
    return;
  }

  /* Regular Paint mode. */

  /* Wet paint color sampling. */
  float4 wet_color(0);
  if (ss.cache->paint_brush.wet_mix > 0.0f) {
    threading::EnumerableThreadSpecific<ColorPaintLocalData> all_tls;
    const SampleWetPaintData swptd = threading::parallel_reduce(
        node_mask.index_range(),
        1,
        SampleWetPaintData{},
        [&](const IndexRange range, SampleWetPaintData swptd) {
          ColorPaintLocalData &tls = all_tls.local();
          node_mask.slice(range).foreach_index([&](const int i) {
            do_sample_wet_paint_task(ob,
                                     vert_positions,
                                     faces,
                                     corner_verts,
                                     vert_to_face_map,
                                     attribute_data.hide_vert,
                                     color_attribute.span,
                                     color_attribute.domain,
                                     brush,
                                     nodes[i],
                                     tls,
                                     swptd);
          });
          return swptd;
        },
        [](const SampleWetPaintData &a, const SampleWetPaintData &b) {
          SampleWetPaintData joined{};
          joined.color = a.color + b.color;
          joined.tot_samples = a.tot_samples + b.tot_samples;
          return joined;
        });

    if (swptd.tot_samples > 0 && is_finite_v4(swptd.color)) {
      wet_color = math::clamp(swptd.color / float(swptd.tot_samples), 0.0f, 1.0f);

      if (ss.cache->first_time) {
        ss.cache->paint_brush.wet_mix_prev_color = wet_color;
      }
      blend_color_interpolate_float(wet_color,
                                    wet_color,
                                    ss.cache->paint_brush.wet_mix_prev_color,
                                    ss.cache->paint_brush.wet_persistence);
      ss.cache->paint_brush.wet_mix_prev_color = math::clamp(wet_color, 0.0f, 1.0f);
    }
  }

  if (ss.cache->paint_brush.mix_colors.is_empty()) {
    ss.cache->paint_brush.mix_colors = Array<float4>(mesh.verts_num, float4(0));
  }

  threading::EnumerableThreadSpecific<ColorPaintLocalData> all_tls;
  node_mask.foreach_index(GrainSize(1), [&](const int i) {
    ColorPaintLocalData &tls = all_tls.local();
    do_paint_brush_task(depsgraph,
                        ob,
                        vert_positions,
                        vert_normals,
                        faces,
                        corner_verts,
                        vert_to_face_map,
                        attribute_data,
                        sd.paint,
                        brush,
                        mat,
                        wet_color,
                        nodes[i],
                        tls,
                        ss.cache->paint_brush.mix_colors,
                        color_attribute);
  });
  pbvh.tag_attribute_changed(node_mask, mesh.active_color_attribute);
  color_attribute.finish();
}

static void do_smear_brush_task(const Depsgraph &depsgraph,
                                Object &object,
                                const Span<float3> vert_positions,
                                const Span<float3> vert_normals,
                                const OffsetIndices<int> faces,
                                const Span<int> corner_verts,
                                const GroupedSpan<int> vert_to_face_map,
                                const MeshAttributeData &attribute_data,
                                const Brush &brush,
                                bke::pbvh::MeshNode &node,
                                ColorPaintLocalData &tls,
                                bke::GSpanAttributeWriter &color_attribute)
{
  const SculptSession &ss = *object.sculpt;
  const StrokeCache &cache = *ss.cache;
  const float strength = ss.cache->bstrength;

  const Span<int> verts = node.verts();

  tls.factors.resize(verts.size());
  const MutableSpan<float> factors = tls.factors;
  fill_factor_from_hide_and_mask(attribute_data.hide_vert, attribute_data.mask, verts, factors);
  filter_region_clip_factors(ss, vert_positions, verts, factors);
  if (brush.flag & BRUSH_FRONTFACE) {
    calc_front_face(cache.view_normal_symm, vert_normals, verts, factors);
  }

  tls.distances.resize(verts.size());
  const MutableSpan<float> distances = tls.distances;
  calc_brush_distances(
      ss, vert_positions, verts, eBrushFalloffShape(brush.falloff_shape), distances);
  filter_distances_with_radius(cache.radius, distances, factors);
  apply_hardness_to_distances(cache, distances);
  calc_brush_strength_factors(cache, brush, distances, factors);

  auto_mask::calc_vert_factors(depsgraph, object, cache.automasking.get(), node, verts, factors);

  calc_brush_texture_factors(ss, brush, vert_positions, verts, factors);
  scale_factors(factors, strength);

  float3 brush_delta;
  if (brush.flag & BRUSH_ANCHORED) {
    brush_delta = ss.cache->grab_delta_symm;
  }
  else {
    brush_delta = ss.cache->location_symm - ss.cache->last_location_symm;
  }

  Vector<int> neighbors;
  Vector<int> neighbor_neighbors;

  for (const int i : verts.index_range()) {
    if (factors[i] == 0.0f) {
      continue;
    }
    const int vert = verts[i];
    const float3 &no = vert_normals[vert];

    float3 current_disp;
    switch (brush.smear_deform_type) {
      case BRUSH_SMEAR_DEFORM_DRAG:
        current_disp = brush_delta;
        break;
      case BRUSH_SMEAR_DEFORM_PINCH:
        current_disp = ss.cache->location_symm - vert_positions[vert];
        break;
      case BRUSH_SMEAR_DEFORM_EXPAND:
        current_disp = vert_positions[vert] - ss.cache->location_symm;
        break;
    }

    /* Project into vertex plane. */
    current_disp += no * -math::dot(current_disp, no);

    const float3 current_disp_norm = math::normalize(current_disp);

    current_disp = current_disp_norm * strength;

    float4 accum(0);
    float totw = 0.0f;

    /*
     * NOTE: we have to do a nested iteration here to avoid
     * blocky artifacts on quad topologies.  The runtime cost
     * is not as bad as it seems due to neighbor iteration
     * in the sculpt code being cache bound; once the data is in
     * the cache iterating over it a few more times is not terribly
     * costly.
     */

    for (const int neighbor : vert_neighbors_get_mesh(
             faces, corner_verts, vert_to_face_map, attribute_data.hide_poly, vert, neighbors))
    {
      const float3 &nco = vert_positions[neighbor];
      for (const int neighbor_neighbor : vert_neighbors_get_mesh(faces,
                                                                 corner_verts,
                                                                 vert_to_face_map,
                                                                 attribute_data.hide_poly,
                                                                 neighbor,
                                                                 neighbor_neighbors))
      {
        if (neighbor_neighbor == vert) {
          continue;
        }

        float3 vert_disp = vert_positions[neighbor_neighbor] - vert_positions[vert];

        /* Weight by how close we are to our target distance from vd.co. */
        float w = (1.0f + fabsf(math::length(vert_disp) / strength - 1.0f));

        /* TODO: use cotangents (or at least face areas) here. */
        float len = math::distance(vert_positions[neighbor_neighbor], nco);
        if (len > 0.0f) {
          len = strength / len;
        }
        else { /* Coincident point. */
          len = 1.0f;
        }

        /* Multiply weight with edge lengths (in the future this will be
         * cotangent weights or face areas). */
        w *= len;

        /* Build directional weight. */

        /* Project into vertex plane. */
        vert_disp += no * -math::dot(no, vert_disp);
        const float3 vert_disp_norm = math::normalize(vert_disp);

        if (math::dot(current_disp_norm, vert_disp_norm) >= 0.0f) {
          continue;
        }

        const float4 &neighbor_color = ss.cache->paint_brush.prev_colors[neighbor_neighbor];
        float color_interp = -math::dot(current_disp_norm, vert_disp_norm);

        /* Square directional weight to get a somewhat sharper result. */
        w *= color_interp * color_interp;

        accum += neighbor_color * w;
        totw += w;
      }
    }

    if (totw != 0.0f) {
      accum /= totw;
    }

    float4 col = color_vert_get(
        faces, corner_verts, vert_to_face_map, color_attribute.span, color_attribute.domain, vert);
    blend_color_interpolate_float(col, ss.cache->paint_brush.prev_colors[vert], accum, factors[i]);
    color_vert_set(faces,
                   corner_verts,
                   vert_to_face_map,
                   color_attribute.domain,
                   vert,
                   col,
                   color_attribute.span);
  }
}

void do_smear_brush(const Depsgraph &depsgraph,
                    const Sculpt &sd,
                    Object &ob,
                    const IndexMask &node_mask)
{
  const Brush &brush = *BKE_paint_brush_for_read(&sd.paint);
  SculptSession &ss = *ob.sculpt;
  bke::pbvh::Tree &pbvh = *bke::object::pbvh_get(ob);
  MutableSpan<bke::pbvh::MeshNode> nodes = pbvh.nodes<bke::pbvh::MeshNode>();

  Mesh &mesh = *static_cast<Mesh *>(ob.data);
  if (ss.cache->bstrength == 0.0f) {
    return;
  }

  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const Span<float3> vert_positions = bke::pbvh::vert_positions_eval(depsgraph, ob);
  const Span<float3> vert_normals = bke::pbvh::vert_normals_eval(depsgraph, ob);
  const MeshAttributeData attribute_data(mesh);

  bke::GSpanAttributeWriter color_attribute = active_color_attribute_for_write(mesh);
  if (!color_attribute) {
    return;
  }

  if (ss.cache->paint_brush.prev_colors.is_empty()) {
    ss.cache->paint_brush.prev_colors = Array<float4>(mesh.verts_num);
    threading::parallel_for(IndexRange(mesh.verts_num), 1024, [&](const IndexRange range) {
      for (const int vert : range) {
        ss.cache->paint_brush.prev_colors[vert] = color_vert_get(faces,
                                                                 corner_verts,
                                                                 vert_to_face_map,
                                                                 color_attribute.span,
                                                                 color_attribute.domain,
                                                                 vert);
      }
    });
  }

  BKE_curvemapping_init(brush.curve_distance_falloff);

  /* Smooth colors mode. */
  if (ss.cache->alt_smooth) {
    threading::EnumerableThreadSpecific<ColorPaintLocalData> all_tls;
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      ColorPaintLocalData &tls = all_tls.local();
      do_color_smooth_task(depsgraph,
                           ob,
                           vert_positions,
                           vert_normals,
                           faces,
                           corner_verts,
                           vert_to_face_map,
                           attribute_data,
                           brush,
                           nodes[i],
                           tls,
                           color_attribute);
    });
  }
  else {
    /* Smear mode. */
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      for (const int vert : nodes[i].verts()) {
        ss.cache->paint_brush.prev_colors[vert] = color_vert_get(faces,
                                                                 corner_verts,
                                                                 vert_to_face_map,
                                                                 color_attribute.span,
                                                                 color_attribute.domain,
                                                                 vert);
      }
    });
    threading::EnumerableThreadSpecific<ColorPaintLocalData> all_tls;
    node_mask.foreach_index(GrainSize(1), [&](const int i) {
      ColorPaintLocalData &tls = all_tls.local();
      do_smear_brush_task(depsgraph,
                          ob,
                          vert_positions,
                          vert_normals,
                          faces,
                          corner_verts,
                          vert_to_face_map,
                          attribute_data,
                          brush,
                          nodes[i],
                          tls,
                          color_attribute);
    });
  }
  pbvh.tag_attribute_changed(node_mask, mesh.active_color_attribute);
  color_attribute.finish();
}

}  // namespace blender::ed::sculpt_paint::color
