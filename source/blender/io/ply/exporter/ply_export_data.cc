/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "ply_export_data.hh"
#include "ply_data.hh"
#include "ply_file_buffer.hh"

#include "BLI_math_color_c.hh"

#include "BKE_attribute.hh"

namespace blender::io::ply {

std::optional<PlyCustomAttributeLayout> custom_attribute_layout(const bke::AttrType type)
{
  static const StringRef scalar[] = {""};
  static const StringRef xy[] = {"_x", "_y"};
  static const StringRef xyz[] = {"_x", "_y", "_z"};
  static const StringRef xyzw[] = {"_x", "_y", "_z", "_w"};
  static const StringRef rgba[] = {"_r", "_g", "_b", "_a"};
  switch (type) {
    case bke::AttrType::Bool:
      return PlyCustomAttributeLayout{UCHAR, scalar};
    case bke::AttrType::Int8:
      return PlyCustomAttributeLayout{CHAR, scalar};
    case bke::AttrType::Int32:
      return PlyCustomAttributeLayout{INT, scalar};
    case bke::AttrType::Int16_2D:
      return PlyCustomAttributeLayout{SHORT, xy};
    case bke::AttrType::Int32_2D:
      return PlyCustomAttributeLayout{INT, xy};
    case bke::AttrType::Float:
      return PlyCustomAttributeLayout{FLOAT, scalar};
    case bke::AttrType::Float2:
      return PlyCustomAttributeLayout{FLOAT, xy};
    case bke::AttrType::Float3:
      return PlyCustomAttributeLayout{FLOAT, xyz};
    case bke::AttrType::Quaternion:
      return PlyCustomAttributeLayout{FLOAT, xyzw};
    case bke::AttrType::ColorByte:
      return PlyCustomAttributeLayout{UCHAR, rgba};
    case bke::AttrType::ColorFloat:
      return PlyCustomAttributeLayout{FLOAT, rgba};
    default:
      return std::nullopt;
  }
}

Vector<PlyCustomAttributeProperty> custom_attribute_properties(const PlyData &ply_data)
{
  Vector<PlyCustomAttributeProperty> properties;
  for (const PlyCustomAttribute &attr : ply_data.vertex_custom_attr) {
    const bke::AttrType type = bke::cpp_type_to_attribute_type(attr.type());
    const std::optional<PlyCustomAttributeLayout> layout = custom_attribute_layout(type);
    BLI_assert(layout);
    const int scalar_size = data_type_size[layout->scalar_type];
    const char *data = static_cast<const char *>(attr.data().data());
    for (const int component : layout->suffixes.index_range()) {
      /* Note: blender quaternions store w,x,y,z. */
      const int index = type == bke::AttrType::Quaternion ? (component + 1) % 4 : component;
      properties.append({attr.name + layout->suffixes[component],
                         layout->scalar_type,
                         data + index * scalar_size,
                         int(attr.type().size)});
    }
  }
  return properties;
}

void write_vertices(FileBuffer &buffer, const PlyData &ply_data)
{
  const Vector<PlyCustomAttributeProperty> properties = custom_attribute_properties(ply_data);
  for (int i = 0; i < ply_data.vertices.size(); i++) {
    buffer.write_vertex(ply_data.vertices[i].x, ply_data.vertices[i].y, ply_data.vertices[i].z);

    if (!ply_data.vertex_normals.is_empty()) {
      buffer.write_vertex_normal(ply_data.vertex_normals[i].x,
                                 ply_data.vertex_normals[i].y,
                                 ply_data.vertex_normals[i].z);
    }

    if (!ply_data.vertex_colors.is_empty()) {
      const float4 vcolor = ply_data.vertex_colors[i];
      if (ply_data.vertex_colors_are_float) {
        buffer.write_data(vcolor[0]);
        buffer.write_data(vcolor[1]);
        buffer.write_data(vcolor[2]);
        buffer.write_data(vcolor[3]);
      }
      else {
        uchar color[4];
        rgba_float_to_uchar(color, vcolor);
        buffer.write_vertex_color(color[0], color[1], color[2], color[3]);
      }
    }

    if (!ply_data.uv_coordinates.is_empty()) {
      buffer.write_UV(ply_data.uv_coordinates[i].x, ply_data.uv_coordinates[i].y);
    }

    for (const PlyCustomAttributeProperty &property : properties) {
      buffer.write_data(property.data + int64_t(i) * property.stride, property.type);
    }

    buffer.write_vertex_end();
  }
  buffer.write_to_file();
}

void write_faces(FileBuffer &buffer, const PlyData &ply_data)
{
  const uint32_t *indices = ply_data.face_vertices.data();
  for (uint32_t face_size : ply_data.face_sizes) {
    buffer.write_face(char(face_size), Span<uint32_t>(indices, face_size));
    indices += face_size;
  }
  buffer.write_to_file();
}
void write_edges(FileBuffer &buffer, const PlyData &ply_data)
{
  for (const std::pair<int, int> &edge : ply_data.edges) {
    buffer.write_edge(edge.first, edge.second);
  }
  buffer.write_to_file();
}
}  // namespace blender::io::ply
