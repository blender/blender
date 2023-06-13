/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "ply_import_data.hh"
#include "ply_data.hh"
#include "ply_import_buffer.hh"

#include "BLI_endian_switch.h"

#include "fast_float.h"

#include <algorithm>
#include <charconv>

static bool is_whitespace(char c)
{
  return c <= ' ';
}

static const char *drop_whitespace(const char *p, const char *end)
{
  while (p < end && is_whitespace(*p)) {
    ++p;
  }
  return p;
}

static const char *drop_non_whitespace(const char *p, const char *end)
{
  while (p < end && !is_whitespace(*p)) {
    ++p;
  }
  return p;
}

static const char *drop_plus(const char *p, const char *end)
{
  if (p < end && *p == '+') {
    ++p;
  }
  return p;
}

static const char *parse_float(const char *p, const char *end, float fallback, float &dst)
{
  p = drop_whitespace(p, end);
  p = drop_plus(p, end);
  fast_float::from_chars_result res = fast_float::from_chars(p, end, dst);
  if (ELEM(res.ec, std::errc::invalid_argument, std::errc::result_out_of_range)) {
    dst = fallback;
  }
  return res.ptr;
}

static const char *parse_int(const char *p, const char *end, int fallback, int &dst)
{
  p = drop_whitespace(p, end);
  p = drop_plus(p, end);
  std::from_chars_result res = std::from_chars(p, end, dst);
  if (ELEM(res.ec, std::errc::invalid_argument, std::errc::result_out_of_range)) {
    dst = fallback;
  }
  return res.ptr;
}

static void endian_switch(uint8_t *ptr, int type_size)
{
  if (type_size == 2) {
    BLI_endian_switch_uint16((uint16_t *)ptr);
  }
  else if (type_size == 4) {
    BLI_endian_switch_uint32((uint32_t *)ptr);
  }
  else if (type_size == 8) {
    BLI_endian_switch_uint64((uint64_t *)ptr);
  }
}

static void endian_switch_array(uint8_t *ptr, int type_size, int size)
{
  if (type_size == 2) {
    BLI_endian_switch_uint16_array((uint16_t *)ptr, size);
  }
  else if (type_size == 4) {
    BLI_endian_switch_uint32_array((uint32_t *)ptr, size);
  }
  else if (type_size == 8) {
    BLI_endian_switch_uint64_array((uint64_t *)ptr, size);
  }
}

namespace blender::io::ply {

static const int data_type_size[] = {0, 1, 1, 2, 2, 4, 4, 4, 8};
static_assert(std::size(data_type_size) == PLY_TYPE_COUNT, "PLY data type size table mismatch");

static const float data_type_normalizer[] = {
    1.0f, 127.0f, 255.0f, 32767.0f, 65535.0f, float(INT_MAX), float(UINT_MAX), 1.0f, 1.0f};
static_assert(std::size(data_type_normalizer) == PLY_TYPE_COUNT,
              "PLY data type normalization factor table mismatch");

void PlyElement::calc_stride()
{
  stride = 0;
  for (PlyProperty &p : properties) {
    if (p.count_type != PlyDataTypes::NONE) {
      stride = 0;
      return;
    }
    stride += data_type_size[p.type];
  }
}

static int get_index(const PlyElement &element, StringRef property)
{
  for (int i = 0, n = int(element.properties.size()); i != n; i++) {
    const PlyProperty &prop = element.properties[i];
    if (prop.name == property) {
      return i;
    }
  }
  return -1;
}

static const char *parse_row_ascii(PlyReadBuffer &file, Vector<float> &r_values)
{
  Span<char> line = file.read_line();

  /* Parse whole line as floats. */
  const char *p = line.data();
  const char *end = p + line.size();
  int value_idx = 0;
  while (p < end && value_idx < r_values.size()) {
    float val;
    p = parse_float(p, end, 0.0f, val);
    r_values[value_idx++] = val;
  }
  return nullptr;
}

template<typename T> static T get_binary_value(PlyDataTypes type, const uint8_t *&r_ptr)
{
  T val = 0;
  switch (type) {
    case NONE:
      break;
    case CHAR:
      val = *(int8_t *)r_ptr;
      r_ptr += 1;
      break;
    case UCHAR:
      val = *(uint8_t *)r_ptr;
      r_ptr += 1;
      break;
    case SHORT:
      val = *(int16_t *)r_ptr;
      r_ptr += 2;
      break;
    case USHORT:
      val = *(uint16_t *)r_ptr;
      r_ptr += 2;
      break;
    case INT:
      val = *(int32_t *)r_ptr;
      r_ptr += 4;
      break;
    case UINT:
      val = *(int32_t *)r_ptr;
      r_ptr += 4;
      break;
    case FLOAT:
      val = *(float *)r_ptr;
      r_ptr += 4;
      break;
    case DOUBLE:
      val = *(double *)r_ptr;
      r_ptr += 8;
      break;
    default:
      BLI_assert_msg(false, "Unknown property type");
  }
  return val;
}

static const char *parse_row_binary(PlyReadBuffer &file,
                                    const PlyHeader &header,
                                    const PlyElement &element,
                                    Vector<uint8_t> &r_scratch,
                                    Vector<float> &r_values)
{
  if (element.stride == 0) {
    return "Vertex/Edge element contains list properties, this is not supported";
  }
  BLI_assert(r_scratch.size() == element.stride);
  BLI_assert(r_values.size() == element.properties.size());
  if (!file.read_bytes(r_scratch.data(), r_scratch.size())) {
    return "Could not read row of binary property";
  }

  const uint8_t *ptr = r_scratch.data();
  if (header.type == PlyFormatType::BINARY_LE) {
    /* Little endian: just read/convert the values. */
    for (int i = 0, n = int(element.properties.size()); i != n; i++) {
      const PlyProperty &prop = element.properties[i];
      float val = get_binary_value<float>(prop.type, ptr);
      r_values[i] = val;
    }
  }
  else if (header.type == PlyFormatType::BINARY_BE) {
    /* Big endian: read, switch endian, convert the values. */
    for (int i = 0, n = int(element.properties.size()); i != n; i++) {
      const PlyProperty &prop = element.properties[i];
      endian_switch((uint8_t *)ptr, data_type_size[prop.type]);
      float val = get_binary_value<float>(prop.type, ptr);
      r_values[i] = val;
    }
  }
  else {
    return "Unknown binary ply format for vertex element";
  }
  return nullptr;
}

static const char *load_vertex_element(PlyReadBuffer &file,
                                       const PlyHeader &header,
                                       const PlyElement &element,
                                       PlyData *data)
{
  /* Figure out vertex component indices. */
  int3 vertex_index = {get_index(element, "x"), get_index(element, "y"), get_index(element, "z")};
  int3 color_index = {
      get_index(element, "red"), get_index(element, "green"), get_index(element, "blue")};
  int3 normal_index = {
      get_index(element, "nx"), get_index(element, "ny"), get_index(element, "nz")};
  int2 uv_index = {get_index(element, "s"), get_index(element, "t")};
  int alpha_index = get_index(element, "alpha");

  bool has_vertex = vertex_index.x >= 0 && vertex_index.y >= 0 && vertex_index.z >= 0;
  bool has_color = color_index.x >= 0 && color_index.y >= 0 && color_index.z >= 0;
  bool has_normal = normal_index.x >= 0 && normal_index.y >= 0 && normal_index.z >= 0;
  bool has_uv = uv_index.x >= 0 && uv_index.y >= 0;
  bool has_alpha = alpha_index >= 0;

  if (!has_vertex) {
    return "Vertex positions are not present in the file";
  }

  data->vertices.reserve(element.count);
  if (has_color) {
    data->vertex_colors.reserve(element.count);
  }
  if (has_normal) {
    data->vertex_normals.reserve(element.count);
  }
  if (has_uv) {
    data->uv_coordinates.reserve(element.count);
  }

  float4 color_norm = {1, 1, 1, 1};
  if (has_color) {
    color_norm.x = data_type_normalizer[element.properties[color_index.x].type];
    color_norm.y = data_type_normalizer[element.properties[color_index.y].type];
    color_norm.z = data_type_normalizer[element.properties[color_index.z].type];
  }
  if (has_alpha) {
    color_norm.w = data_type_normalizer[element.properties[alpha_index].type];
  }

  Vector<float> value_vec(element.properties.size());
  Vector<uint8_t> scratch;
  if (header.type != PlyFormatType::ASCII) {
    scratch.resize(element.stride);
  }

  for (int i = 0; i < element.count; i++) {

    const char *error = nullptr;
    if (header.type == PlyFormatType::ASCII) {
      error = parse_row_ascii(file, value_vec);
    }
    else {
      error = parse_row_binary(file, header, element, scratch, value_vec);
    }
    if (error != nullptr) {
      return error;
    }

    /* Vertex coord */
    float3 vertex3;
    vertex3.x = value_vec[vertex_index.x];
    vertex3.y = value_vec[vertex_index.y];
    vertex3.z = value_vec[vertex_index.z];
    data->vertices.append(vertex3);

    /* Vertex color */
    if (has_color) {
      float4 colors4;
      colors4.x = value_vec[color_index.x] / color_norm.x;
      colors4.y = value_vec[color_index.y] / color_norm.y;
      colors4.z = value_vec[color_index.z] / color_norm.z;
      if (has_alpha) {
        colors4.w = value_vec[alpha_index] / color_norm.w;
      }
      else {
        colors4.w = 1.0f;
      }
      data->vertex_colors.append(colors4);
    }

    /* If normals */
    if (has_normal) {
      float3 normals3;
      normals3.x = value_vec[normal_index.x];
      normals3.y = value_vec[normal_index.y];
      normals3.z = value_vec[normal_index.z];
      data->vertex_normals.append(normals3);
    }

    /* If uv */
    if (has_uv) {
      float2 uvmap;
      uvmap.x = value_vec[uv_index.x];
      uvmap.y = value_vec[uv_index.y];
      data->uv_coordinates.append(uvmap);
    }
  }
  return nullptr;
}

static uint32_t read_list_count(PlyReadBuffer &file,
                                const PlyProperty &prop,
                                Vector<uint8_t> &scratch,
                                bool big_endian)
{
  scratch.resize(8);
  file.read_bytes(scratch.data(), data_type_size[prop.count_type]);
  const uint8_t *ptr = scratch.data();
  if (big_endian)
    endian_switch((uint8_t *)ptr, data_type_size[prop.count_type]);
  uint32_t count = get_binary_value<uint32_t>(prop.count_type, ptr);
  return count;
}

static void skip_property(PlyReadBuffer &file,
                          const PlyProperty &prop,
                          Vector<uint8_t> &scratch,
                          bool big_endian)
{
  if (prop.count_type == PlyDataTypes::NONE) {
    scratch.resize(8);
    file.read_bytes(scratch.data(), data_type_size[prop.type]);
  }
  else {
    uint32_t count = read_list_count(file, prop, scratch, big_endian);
    scratch.resize(count * data_type_size[prop.type]);
    file.read_bytes(scratch.data(), scratch.size());
  }
}

static const char *load_face_element(PlyReadBuffer &file,
                                     const PlyHeader &header,
                                     const PlyElement &element,
                                     PlyData *data)
{
  int prop_index = get_index(element, "vertex_indices");
  if (prop_index < 0) {
    prop_index = get_index(element, "vertex_index");
  }
  if (prop_index < 0 && element.properties.size() == 1) {
    prop_index = 0;
  }
  if (prop_index < 0) {
    return "Face element does not contain vertex indices property";
  }
  const PlyProperty &prop = element.properties[prop_index];
  if (prop.count_type == PlyDataTypes::NONE) {
    return "Face element vertex indices property must be a list";
  }

  data->face_vertices.reserve(element.count * 3);
  data->face_sizes.reserve(element.count);

  if (header.type == PlyFormatType::ASCII) {
    for (int i = 0; i < element.count; i++) {
      /* Read line */
      Span<char> line = file.read_line();

      const char *p = line.data();
      const char *end = p + line.size();
      int count = 0;

      /* Skip any properties before vertex indices. */
      for (int j = 0; j < prop_index; j++) {
        p = drop_whitespace(p, end);
        if (element.properties[j].count_type == PlyDataTypes::NONE) {
          p = drop_non_whitespace(p, end);
        }
        else {
          p = parse_int(p, end, 0, count);
          for (int k = 0; k < count; ++k) {
            p = drop_whitespace(p, end);
            p = drop_non_whitespace(p, end);
          }
        }
      }

      /* Parse vertex indices list. */
      p = parse_int(p, end, 0, count);
      if (count < 1 || count > 255) {
        return "Invalid face size, must be between 1 and 255";
      }

      for (int j = 0; j < count; j++) {
        int index;
        p = parse_int(p, end, 0, index);
        data->face_vertices.append(index);
      }
      data->face_sizes.append(count);
    }
  }
  else {
    Vector<uint8_t> scratch(64);

    for (int i = 0; i < element.count; i++) {
      const uint8_t *ptr;

      /* Skip any properties before vertex indices. */
      for (int j = 0; j < prop_index; j++) {
        skip_property(
            file, element.properties[j], scratch, header.type == PlyFormatType::BINARY_BE);
      }

      /* Read vertex indices list. */
      uint32_t count = read_list_count(
          file, prop, scratch, header.type == PlyFormatType::BINARY_BE);
      if (count < 1 || count > 255) {
        return "Invalid face size, must be between 1 and 255";
      }

      scratch.resize(count * data_type_size[prop.type]);
      file.read_bytes(scratch.data(), scratch.size());
      ptr = scratch.data();
      if (header.type == PlyFormatType::BINARY_BE)
        endian_switch_array((uint8_t *)ptr, data_type_size[prop.type], count);
      for (int j = 0; j < count; ++j) {
        uint32_t index = get_binary_value<uint32_t>(prop.type, ptr);
        data->face_vertices.append(index);
      }
      data->face_sizes.append(count);

      /* Skip any properties after vertex indices. */
      for (int j = prop_index + 1; j < element.properties.size(); j++) {
        skip_property(
            file, element.properties[j], scratch, header.type == PlyFormatType::BINARY_BE);
      }
    }
  }
  return nullptr;
}

static const char *load_tristrips_element(PlyReadBuffer &file,
                                          const PlyHeader &header,
                                          const PlyElement &element,
                                          PlyData *data)
{
  if (element.count != 1) {
    return "Tristrips element should contain one row";
  }
  if (element.properties.size() != 1) {
    return "Tristrips element should contain one property";
  }
  const PlyProperty &prop = element.properties[0];
  if (prop.count_type == PlyDataTypes::NONE) {
    return "Tristrips element property must be a list";
  }

  Vector<int> strip;

  if (header.type == PlyFormatType::ASCII) {
    Span<char> line = file.read_line();

    const char *p = line.data();
    const char *end = p + line.size();
    int count = 0;
    p = parse_int(p, end, 0, count);

    strip.resize(count);
    for (int j = 0; j < count; j++) {
      int index;
      p = parse_int(p, end, 0, index);
      strip[j] = index;
    }
  }
  else {
    Vector<uint8_t> scratch(64);

    const uint8_t *ptr;

    uint32_t count = read_list_count(file, prop, scratch, header.type == PlyFormatType::BINARY_BE);

    strip.resize(count);
    scratch.resize(count * data_type_size[prop.type]);
    file.read_bytes(scratch.data(), scratch.size());
    ptr = scratch.data();
    if (header.type == PlyFormatType::BINARY_BE)
      endian_switch_array((uint8_t *)ptr, data_type_size[prop.type], count);
    for (int j = 0; j < count; ++j) {
      int index = get_binary_value<int>(prop.type, ptr);
      strip[j] = index;
    }
  }

  /* Decode triangle strip (with possible -1 restart indices) into faces. */
  size_t start = 0;

  for (size_t i = 0; i < strip.size(); i++) {
    if (strip[i] == -1) {
      /* Restart strip. */
      start = i + 1;
    }
    else if (i - start >= 2) {
      int a = strip[i - 2], b = strip[i - 1], c = strip[i];
      /* Flip odd triangles. */
      if ((i - start) & 1) {
        SWAP(int, a, b);
      }
      /* Add triangle if it's not degenerate. */
      if (a != b && a != c && b != c) {
        data->face_vertices.append(a);
        data->face_vertices.append(b);
        data->face_vertices.append(c);
        data->face_sizes.append(3);
      }
    }
  }
  return nullptr;
}

static const char *load_edge_element(PlyReadBuffer &file,
                                     const PlyHeader &header,
                                     const PlyElement &element,
                                     PlyData *data)
{
  int prop_vertex1 = get_index(element, "vertex1");
  int prop_vertex2 = get_index(element, "vertex2");
  if (prop_vertex1 < 0 || prop_vertex2 < 0) {
    return "Edge element does not contain vertex1 and vertex2 properties";
  }

  data->edges.reserve(element.count);

  Vector<float> value_vec(element.properties.size());
  Vector<uint8_t> scratch;
  if (header.type != PlyFormatType::ASCII) {
    scratch.resize(element.stride);
  }

  for (int i = 0; i < element.count; i++) {
    const char *error = nullptr;
    if (header.type == PlyFormatType::ASCII) {
      error = parse_row_ascii(file, value_vec);
    }
    else {
      error = parse_row_binary(file, header, element, scratch, value_vec);
    }
    if (error != nullptr) {
      return error;
    }
    int index1 = value_vec[prop_vertex1];
    int index2 = value_vec[prop_vertex2];
    data->edges.append(std::make_pair(index1, index2));
  }
  return nullptr;
}

static const char *skip_element(PlyReadBuffer &file,
                                const PlyHeader &header,
                                const PlyElement &element)
{
  if (header.type == PlyFormatType::ASCII) {
    for (int i = 0; i < element.count; i++) {
      Span<char> line = file.read_line();
      (void)line;
    }
  }
  else {
    Vector<uint8_t> scratch(64);
    for (int i = 0; i < element.count; i++) {
      for (const PlyProperty &prop : element.properties) {
        skip_property(file, prop, scratch, header.type == PlyFormatType::BINARY_BE);
      }
    }
  }
  return nullptr;
}

std::unique_ptr<PlyData> import_ply_data(PlyReadBuffer &file, PlyHeader &header)
{
  std::unique_ptr<PlyData> data = std::make_unique<PlyData>();

  bool got_vertex = false, got_face = false, got_tristrips = false, got_edge = false;
  for (const PlyElement &element : header.elements) {
    const char *error = nullptr;
    if (element.name == "vertex") {
      error = load_vertex_element(file, header, element, data.get());
      got_vertex = true;
    }
    else if (element.name == "face") {
      error = load_face_element(file, header, element, data.get());
      got_face = true;
    }
    else if (element.name == "tristrips") {
      error = load_tristrips_element(file, header, element, data.get());
      got_tristrips = true;
    }
    else if (element.name == "edge") {
      error = load_edge_element(file, header, element, data.get());
      got_edge = true;
    }
    else {
      error = skip_element(file, header, element);
    }
    if (error != nullptr) {
      data->error = error;
      return data;
    }
    if (got_vertex && got_face && got_tristrips && got_edge) {
      /* We have parsed all the elements we'd need, skip the rest. */
      break;
    }
  }

  return data;
}

}  // namespace blender::io::ply
