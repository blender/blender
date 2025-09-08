/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BLI_color.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_string_utf8.h"

#include "DNA_curve_types.h"
#include "DNA_pointcloud_types.h"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"

#include "DRW_render.hh"

#include "ED_view3d.hh"

#include "UI_interface_c.hh"
#include "UI_resources.hh"

#include "draw_manager_text.hh"

#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Displays geometry node viewer output.
 * Values are displayed as text on top of the active object.
 */
class AttributeTexts : Overlay {
 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = !res.is_selection() && state.show_attribute_viewer_text();
  }

  void object_sync(Manager & /*manager*/,
                   const ObjectRef &ob_ref,
                   Resources & /*res*/,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    const Object &object = *ob_ref.object;
    const bool is_preview = ob_ref.preview_base_geometry() != nullptr;
    if (!is_preview) {
      return;
    }

    DRWTextStore *dt = state.dt;
    const float4x4 &object_to_world = object.object_to_world();

    if (ob_ref.preview_instance_index() >= 0) {
      const bke::Instances *instances = ob_ref.preview_base_geometry()->get_instances();
      if (instances->attributes().contains(".viewer")) {
        add_instance_attributes_to_text_cache(
            dt, instances->attributes(), object_to_world, ob_ref.preview_instance_index());

        return;
      }
    }

    switch (object.type) {
      case OB_MESH: {
        const Mesh &mesh = DRW_object_get_data_for_drawing<Mesh>(object);
        add_mesh_attributes_to_text_cache(state, mesh, object_to_world);
        break;
      }
      case OB_POINTCLOUD: {
        const PointCloud &pointcloud = DRW_object_get_data_for_drawing<PointCloud>(object);
        add_attributes_to_text_cache(dt, pointcloud.attributes(), object_to_world);
        break;
      }
      case OB_CURVES_LEGACY: {
        const Curve &curve = DRW_object_get_data_for_drawing<Curve>(object);
        if (curve.curve_eval) {
          const bke::CurvesGeometry &curves = curve.curve_eval->geometry.wrap();
          add_attributes_to_text_cache(dt, curves.attributes(), object_to_world);
        }
        break;
      }
      case OB_CURVES: {
        const Curves &curves_id = DRW_object_get_data_for_drawing<Curves>(object);
        const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
        add_attributes_to_text_cache(dt, curves.attributes(), object_to_world);
        break;
      }
    }
  }

 private:
  void add_attributes_to_text_cache(DRWTextStore *dt,
                                    bke::AttributeAccessor attribute_accessor,
                                    const float4x4 &object_to_world)
  {
    if (!attribute_accessor.contains(".viewer")) {
      return;
    }

    const bke::GAttributeReader attribute = attribute_accessor.lookup(".viewer");
    const VArraySpan<float3> positions = *attribute_accessor.lookup<float3>("position",
                                                                            attribute.domain);

    add_values_to_text_cache(dt, attribute.varray, positions, object_to_world);
  }

  void add_mesh_attributes_to_text_cache(const State &state,
                                         const Mesh &mesh,
                                         const float4x4 &object_to_world)
  {
    const bke::AttributeAccessor attributes = mesh.attributes();
    if (!attributes.contains(".viewer")) {
      return;
    }

    const bke::GAttributeReader attribute = attributes.lookup(".viewer");
    const bke::AttrDomain domain = attribute.domain;
    const VArraySpan<float3> positions = *attributes.lookup<float3>("position", domain);

    if (domain == bke::AttrDomain::Corner) {
      const CPPType &type = attribute.varray.type();
      float offset_by_type = 1.0f;
      if (type.is<int2>() || type.is<float2>() || type.is<float3>() ||
          type.is<ColorGeometry4b>() || type.is<ColorGeometry4f>() || type.is<math::Quaternion>())
      {
        offset_by_type = 1.5f;
      }
      else if (type.is<float4x4>()) {
        offset_by_type = 3.0f;
      }

      Array<float3> corner_positions(positions.size());
      const Span<float3> positions = mesh.vert_positions();
      const OffsetIndices<int> faces = mesh.faces();
      const Span<int> corner_verts = mesh.corner_verts();
      const Span<float3> face_normals = mesh.face_normals();

      threading::parallel_for(faces.index_range(), 512, [&](const IndexRange range) {
        for (const int face_index : range) {
          const float3 &face_normal = face_normals[face_index];
          const IndexRange face = faces[face_index];
          for (const int corner : face) {
            const int corner_prev = bke::mesh::face_corner_prev(face, corner);
            const int corner_next = bke::mesh::face_corner_next(face, corner);
            corner_positions[corner] = calc_corner_text_position(
                positions[corner_verts[corner]],
                positions[corner_verts[corner_prev]],
                positions[corner_verts[corner_next]],
                face_normal,
                state.rv3d,
                object_to_world,
                offset_by_type);
          }
        }
      });
      add_values_to_text_cache(
          state.dt, attribute.varray, corner_positions.as_span(), object_to_world);
    }
    else {
      add_values_to_text_cache(state.dt, attribute.varray, positions, object_to_world);
    }
  }

  void add_instance_attributes_to_text_cache(DRWTextStore *dt,
                                             bke::AttributeAccessor attribute_accessor,
                                             const float4x4 &object_to_world,
                                             int instance_index)
  {
    /* Data from instances are read as a single value from a given index. The data is converted
     * back to an array so one function can handle both instance and object data. */
    const GVArray attribute = attribute_accessor.lookup(".viewer").varray.slice(
        IndexRange(instance_index, 1));

    add_values_to_text_cache(dt, attribute, {float3(0, 0, 0)}, object_to_world);
  }

  static void add_text_to_cache(DRWTextStore *dt,
                                const float3 &position,
                                const StringRef text,
                                const uchar4 &color)
  {
    DRW_text_cache_add(dt,
                       position,
                       text.data(),
                       text.size(),
                       0,
                       0,
                       DRW_TEXT_CACHE_GLOBALSPACE,
                       color,
                       true,
                       true);
  }

  static void add_lines_to_cache(DRWTextStore *dt,
                                 const float3 &position,
                                 const Span<StringRef> lines,
                                 const uchar4 &color)
  {
    const float text_size = UI_style_get()->widget.points;
    const float line_height = text_size * 1.1f * UI_SCALE_FAC;
    const float center_offset = (lines.size() - 1) / 2.0f;
    for (const int i : lines.index_range()) {
      const StringRef line = lines[i];
      DRW_text_cache_add(dt,
                         position,
                         line.data(),
                         line.size(),
                         0,
                         (center_offset - i) * line_height,
                         DRW_TEXT_CACHE_GLOBALSPACE,
                         color,
                         true,
                         true);
    }
  }

  void add_values_to_text_cache(DRWTextStore *dt,
                                const GVArray &values,
                                const Span<float3> positions,
                                const float4x4 &object_to_world)
  {
    uchar col[4];
    UI_GetThemeColor4ubv(TH_TEXT_HI, col);

    bke::attribute_math::convert_to_static_type(values.type(), [&](auto dummy) {
      using T = decltype(dummy);
      const VArray<T> &values_typed = values.typed<T>();
      for (const int i : values.index_range()) {
        const float3 position = math::transform_point(object_to_world, positions[i]);
        const T &value = values_typed[i];

        if constexpr (std::is_same_v<T, bool>) {
          char numstr[64];
          const size_t numstr_len = STRNCPY_UTF8_RLEN(numstr, value ? "True" : "False");
          add_text_to_cache(dt, position, StringRef(numstr, numstr_len), col);
        }
        else if constexpr (std::is_same_v<T, int8_t>) {
          char numstr[64];
          const size_t numstr_len = SNPRINTF_UTF8_RLEN(numstr, "%d", int(value));
          add_text_to_cache(dt, position, StringRef(numstr, numstr_len), col);
        }
        else if constexpr (std::is_same_v<T, int>) {
          char numstr[64];
          const size_t numstr_len = SNPRINTF_UTF8_RLEN(numstr, "%d", value);
          add_text_to_cache(dt, position, StringRef(numstr, numstr_len), col);
        }
        else if constexpr (std::is_same_v<T, int2>) {
          char x_str[64], y_str[64];
          const size_t x_str_len = SNPRINTF_UTF8_RLEN(x_str, "X: %d", value.x);
          const size_t y_str_len = SNPRINTF_UTF8_RLEN(y_str, "Y: %d", value.y);
          add_lines_to_cache(
              dt, position, {StringRef(x_str, x_str_len), StringRef(y_str, y_str_len)}, col);
        }
        else if constexpr (std::is_same_v<T, float>) {
          char numstr[64];
          const size_t numstr_len = SNPRINTF_UTF8_RLEN(numstr, "%g", value);
          add_text_to_cache(dt, position, StringRef(numstr, numstr_len), col);
        }
        else if constexpr (std::is_same_v<T, float2>) {
          char x_str[64], y_str[64];
          const size_t x_str_len = SNPRINTF_UTF8_RLEN(x_str, "X: %g", value.x);
          const size_t y_str_len = SNPRINTF_UTF8_RLEN(y_str, "Y: %g", value.y);
          add_lines_to_cache(
              dt, position, {StringRef(x_str, x_str_len), StringRef(y_str, y_str_len)}, col);
        }
        else if constexpr (std::is_same_v<T, float3>) {
          char x_str[64], y_str[64], z_str[64];
          const size_t x_str_len = SNPRINTF_UTF8_RLEN(x_str, "X: %g", value.x);
          const size_t y_str_len = SNPRINTF_UTF8_RLEN(y_str, "Y: %g", value.y);
          const size_t z_str_len = SNPRINTF_UTF8_RLEN(z_str, "Z: %g", value.z);
          add_lines_to_cache(dt,
                             position,
                             {StringRef(x_str, x_str_len),
                              StringRef(y_str, y_str_len),
                              StringRef(z_str, z_str_len)},
                             col);
        }
        else if constexpr (std::is_same_v<T, ColorGeometry4b>) {
          const ColorGeometry4f color = color::decode(value);
          char r_str[64], g_str[64], b_str[64], a_str[64];
          const size_t r_str_len = SNPRINTF_UTF8_RLEN(r_str, "R: %.3f", color.r);
          const size_t g_str_len = SNPRINTF_UTF8_RLEN(g_str, "G: %.3f", color.g);
          const size_t b_str_len = SNPRINTF_UTF8_RLEN(b_str, "B: %.3f", color.b);
          const size_t a_str_len = SNPRINTF_UTF8_RLEN(a_str, "A: %.3f", color.a);
          add_lines_to_cache(dt,
                             position,
                             {StringRef(r_str, r_str_len),
                              StringRef(g_str, g_str_len),
                              StringRef(b_str, b_str_len),
                              StringRef(a_str, a_str_len)},
                             col);
        }
        else if constexpr (std::is_same_v<T, ColorGeometry4f>) {
          char r_str[64], g_str[64], b_str[64], a_str[64];
          const size_t r_str_len = SNPRINTF_UTF8_RLEN(r_str, "R: %.3f", value.r);
          const size_t g_str_len = SNPRINTF_UTF8_RLEN(g_str, "G: %.3f", value.g);
          const size_t b_str_len = SNPRINTF_UTF8_RLEN(b_str, "B: %.3f", value.b);
          const size_t a_str_len = SNPRINTF_UTF8_RLEN(a_str, "A: %.3f", value.a);
          add_lines_to_cache(dt,
                             position,
                             {StringRef(r_str, r_str_len),
                              StringRef(g_str, g_str_len),
                              StringRef(b_str, b_str_len),
                              StringRef(a_str, a_str_len)},
                             col);
        }
        else if constexpr (std::is_same_v<T, math::Quaternion>) {
          char w_str[64], x_str[64], y_str[64], z_str[64];
          const size_t w_str_len = SNPRINTF_UTF8_RLEN(w_str, "W: %.3f", value.w);
          const size_t x_str_len = SNPRINTF_UTF8_RLEN(x_str, "X: %.3f", value.x);
          const size_t y_str_len = SNPRINTF_UTF8_RLEN(y_str, "Y: %.3f", value.y);
          const size_t z_str_len = SNPRINTF_UTF8_RLEN(z_str, "Z: %.3f", value.z);
          add_lines_to_cache(dt,
                             position,
                             {StringRef(w_str, w_str_len),
                              StringRef(x_str, x_str_len),
                              StringRef(y_str, y_str_len),
                              StringRef(z_str, z_str_len)},
                             col);
        }
        else if constexpr (std::is_same_v<T, float4x4>) {
          float3 location;
          math::EulerXYZ rotation;
          float3 scale;
          math::to_loc_rot_scale_safe<true>(value, location, rotation, scale);

          char location_str[64];
          const size_t location_str_len = SNPRINTF_UTF8_RLEN(
              location_str, "Location: %.3f, %.3f, %.3f", location.x, location.y, location.z);
          char rotation_str[64];
          const size_t rotation_str_len = SNPRINTF_UTF8_RLEN(rotation_str,
                                                             "Rotation: %.3f°, %.3f°, %.3f°",
                                                             rotation.x().degree(),
                                                             rotation.y().degree(),
                                                             rotation.z().degree());
          char scale_str[64];
          const size_t scale_str_len = SNPRINTF_UTF8_RLEN(
              scale_str, "Scale: %.3f, %.3f, %.3f", scale.x, scale.y, scale.z);
          add_lines_to_cache(dt,
                             position,
                             {StringRef(location_str, location_str_len),
                              StringRef(rotation_str, rotation_str_len),
                              StringRef(scale_str, scale_str_len)},
                             col);
        }
        else {
          BLI_assert_unreachable();
        }
      }
    });
  }

  static float3 calc_corner_text_position(const float3 &corner_pos,
                                          const float3 &prev_corner_pos,
                                          const float3 &next_corner_pos,
                                          const float3 &face_normal,
                                          const RegionView3D *rv3d,
                                          const float4x4 &object_to_world,
                                          const float offset_scale = 1.0f)
  {
    const float3 prev_edge_vec = prev_corner_pos - corner_pos;
    const float3 next_edge_vec = next_corner_pos - corner_pos;
    const float3 prev_edge_dir = math::normalize(prev_edge_vec);
    const float3 next_edge_dir = math::normalize(next_edge_vec);

    const float pre_edge_len = math::length(prev_edge_vec);
    const float next_edge_len = math::length(next_edge_vec);
    const float max_offset = math::min(pre_edge_len, next_edge_len) / 2;

    const float3 corner_normal = math::cross(next_edge_dir, prev_edge_dir);
    const float concavity_check = math::dot(corner_normal, face_normal);
    const float direction_correct = concavity_check > 0.0f ? 1.0f : -1.0f;
    const float3 bisector_dir = (prev_edge_dir + next_edge_dir) / 2 * direction_correct;

    const float sharp_factor = std::clamp(math::dot(prev_edge_dir, next_edge_dir), 0.0f, 1.0f);
    const float sharp_multiplier = math::pow(sharp_factor, 4.0f) * 2 + 1;

    const float3 pos_o_world = math::transform_point(object_to_world, corner_pos);
    const float pixel_size = ED_view3d_pixel_size(rv3d, pos_o_world);
    const float pixel_offset = UI_style_get()->widget.points * 7.0f * UI_SCALE_FAC;
    const float screen_space_offset = pixel_size * pixel_offset;

    const float offset_distance = std::clamp(
        screen_space_offset * sharp_multiplier * offset_scale, 0.0f, max_offset);

    return corner_pos + bisector_dir * offset_distance;
  }
};

}  // namespace blender::draw::overlay
