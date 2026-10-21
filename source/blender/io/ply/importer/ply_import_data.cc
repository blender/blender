/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "ply_import_data.hh"
#include "ply_data.hh"
#include "ply_import_buffer.hh"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"

#include "BLI_endian_switch.hh"
#include "BLI_string_ref.hh"

#include "fast_float.h"

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG = {"io.ply"};

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

static const char *parse_int(const char *p, const char *end, int fallback, int &dst)
{
  fast_float::from_chars_result res = fast_float::from_chars_advanced(
      p,
      end,
      dst,
      fast_float::parse_options(fast_float::chars_format::skip_white_space |
                                fast_float::chars_format::allow_leading_plus));
  if (ELEM(res.ec, std::errc::invalid_argument, std::errc::result_out_of_range)) {
    dst = fallback;
  }
  return res.ptr;
}

static void endian_switch(uint8_t *ptr, int type_size)
{
  if (type_size == 2) {
    BLI_endian_switch_uint16(reinterpret_cast<uint16_t *>(ptr));
  }
  else if (type_size == 4) {
    BLI_endian_switch_uint32(reinterpret_cast<uint32_t *>(ptr));
  }
  else if (type_size == 8) {
    BLI_endian_switch_uint64(reinterpret_cast<uint64_t *>(ptr));
  }
}

static void endian_switch_array(uint8_t *ptr, int type_size, int size)
{
  if (type_size == 2) {
    BLI_endian_switch_uint16_array(reinterpret_cast<uint16_t *>(ptr), size);
  }
  else if (type_size == 4) {
    BLI_endian_switch_uint32_array(reinterpret_cast<uint32_t *>(ptr), size);
  }
  else if (type_size == 8) {
    BLI_endian_switch_uint64_array(reinterpret_cast<uint64_t *>(ptr), size);
  }
}

namespace io::ply {

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

/* Parse float or double, with 0 as fallback for invalid values. */
template<typename T> static const char *parse_ascii_value(const char *p, const char *end, T &dst)
{
  const fast_float::from_chars_result res = fast_float::from_chars(
      p,
      end,
      dst,
      fast_float::chars_format::general | fast_float::chars_format::skip_white_space |
          fast_float::chars_format::allow_leading_plus);
  if (res.ec != std::errc()) {
    dst = T(0);
  }
  return res.ptr;
}

template<typename T>
static const char *parse_row_ascii(PlyReadBuffer &file,
                                   const PlyElement &element,
                                   Vector<T> &r_values)
{
  Span<char> line = file.read_line();
  if (line.is_empty()) {
    return "Could not read row of ASCII property";
  }

  const char *p = line.begin();
  const char *end = line.end();
  for (const int64_t i : element.properties.index_range()) {
    if (element.properties[i].count_type == PlyDataTypes::NONE) {
      p = parse_ascii_value(p, end, r_values[i]);
      continue;
    }

    /* Skip list values, which are not supported for vertices and edges. */
    int count = 0;
    p = parse_int(p, end, 0, count);
    for (int j = 0; j < count && p < end; j++) {
      p = drop_non_whitespace(drop_whitespace(p, end), end);
    }
    r_values[i] = T(0);
  }
  return nullptr;
}

/* Convert floating point to T. Values that don't fit an integer type or that are
 * NaN become the maximum value, so they fail later count and index checks. */
template<typename T, typename F> static T float_to_value(const F value)
{
  if constexpr (std::is_integral_v<T>) {
    if (!(value >= F(std::numeric_limits<T>::min()) && value < F(std::numeric_limits<T>::max()))) {
      return std::numeric_limits<T>::max();
    }
  }
  return T(value);
}

template<typename T> static T get_binary_value(PlyDataTypes type, const uint8_t *&r_ptr)
{
  T val = 0;
  switch (type) {
    case NONE:
      break;
    case CHAR:
      val = *reinterpret_cast<int8_t *>(const_cast<uint8_t *>(r_ptr));
      r_ptr += 1;
      break;
    case UCHAR:
      val = *const_cast<uint8_t *>(r_ptr);
      r_ptr += 1;
      break;
    case SHORT:
      val = *reinterpret_cast<int16_t *>(const_cast<uint8_t *>(r_ptr));
      r_ptr += 2;
      break;
    case USHORT:
      val = *reinterpret_cast<uint16_t *>(const_cast<uint8_t *>(r_ptr));
      r_ptr += 2;
      break;
    case INT:
      val = *reinterpret_cast<int32_t *>(const_cast<uint8_t *>(r_ptr));
      r_ptr += 4;
      break;
    case UINT:
      val = *reinterpret_cast<uint32_t *>(const_cast<uint8_t *>(r_ptr));
      r_ptr += 4;
      break;
    case FLOAT:
      val = float_to_value<T>(*reinterpret_cast<float *>(const_cast<uint8_t *>(r_ptr)));
      r_ptr += 4;
      break;
    case DOUBLE:
      val = float_to_value<T>(*reinterpret_cast<double *>(const_cast<uint8_t *>(r_ptr)));
      r_ptr += 8;
      break;
    default:
      BLI_assert_msg(false, "Unknown property type");
  }
  return val;
}

template<typename T>
static const char *parse_row_binary(PlyReadBuffer &file,
                                    const PlyHeader &header,
                                    const PlyElement &element,
                                    Vector<uint8_t> &r_scratch,
                                    Vector<T> &r_values)
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
      T val = get_binary_value<T>(prop.type, ptr);
      r_values[i] = val;
    }
  }
  else if (header.type == PlyFormatType::BINARY_BE) {
    /* Big endian: read, switch endian, convert the values. */
    for (int i = 0, n = int(element.properties.size()); i != n; i++) {
      const PlyProperty &prop = element.properties[i];
      endian_switch(const_cast<uint8_t *>(ptr), data_type_size[prop.type]);
      T val = get_binary_value<T>(prop.type, ptr);
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

  Vector<int64_t> custom_attr_indices;
  for (const int64_t prop_idx : element.properties.index_range()) {
    const PlyProperty &prop = element.properties[prop_idx];
    bool is_standard = ELEM(
        prop.name, "x", "y", "z", "nx", "ny", "nz", "red", "green", "blue", "alpha", "s", "t");
    if (is_standard || prop.count_type != PlyDataTypes::NONE) {
      continue;
    }
    /* Skip names that are empty or reserved for internal attributes. */
    if (prop.name.empty() || !bke::allow_procedural_attribute_access(prop.name) ||
        bke::attribute_name_is_anonymous(prop.name))
    {
      continue;
    }

    custom_attr_indices.append(prop_idx);
    PlyCustomAttribute attr(prop.name, element.count);
    data->vertex_custom_attr.append(attr);
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
      error = parse_row_ascii(file, element, value_vec);
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

    /* Custom attributes */
    for (const int64_t ci : custom_attr_indices.index_range()) {
      float value = value_vec[custom_attr_indices[ci]];
      data->vertex_custom_attr[ci].data[i] = value;
    }
  }
  return nullptr;
}

static bool read_list_count(PlyReadBuffer &file,
                            const PlyProperty &prop,
                            Vector<uint8_t> &scratch,
                            bool big_endian,
                            uint32_t &r_count)
{
  scratch.resize(8);
  if (!file.read_bytes(scratch.data(), data_type_size[prop.count_type])) {
    return false;
  }
  const uint8_t *ptr = scratch.data();
  if (big_endian) {
    endian_switch(const_cast<uint8_t *>(ptr), data_type_size[prop.count_type]);
  }
  r_count = get_binary_value<uint32_t>(prop.count_type, ptr);
  return true;
}

/* Read list values into scratch, returns false on unexpected end of file. */
static bool read_list_values(PlyReadBuffer &file,
                             const PlyProperty &prop,
                             const uint32_t count,
                             Vector<uint8_t> &scratch)
{
  scratch.resize(int64_t(count) * data_type_size[prop.type]);
  return file.read_bytes(scratch.data(), scratch.size());
}

static bool skip_property(PlyReadBuffer &file,
                          const PlyProperty &prop,
                          Vector<uint8_t> &scratch,
                          bool big_endian)
{
  if (prop.count_type == PlyDataTypes::NONE) {
    scratch.resize(8);
    return file.read_bytes(scratch.data(), data_type_size[prop.type]);
  }
  uint32_t count;
  if (!read_list_count(file, prop, scratch, big_endian, count)) {
    return false;
  }
  return read_list_values(file, prop, count, scratch);
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

  data->face_vertices.reserve(int64_t(element.count) * 3);
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
          for (int k = 0; k < count && p < end; ++k) {
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
      /* Previous python based importer was accepting faces with fewer
       * than 3 vertices, and silently dropping them. */
      if (count < 3) {
        CLOG_WARN(&LOG, "PLY Importer: ignoring face %i (%i vertices)", i, count);
        continue;
      }

      /* Skip faces with fewer indices than their size, instead of using index 0. */
      const int64_t face_start = data->face_vertices.size();
      bool is_complete = true;
      for (int j = 0; j < count; j++) {
        p = drop_whitespace(p, end);
        if (p == end) {
          is_complete = false;
          break;
        }
        int index;
        p = parse_int(p, end, 0, index);
        data->face_vertices.append(index);
      }
      if (!is_complete) {
        CLOG_WARN(&LOG, "PLY Importer: ignoring face %i (missing vertex indices)", i);
        data->face_vertices.resize(face_start);
        continue;
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
        if (!skip_property(
                file, element.properties[j], scratch, header.type == PlyFormatType::BINARY_BE))
        {
          return "Could not read face property";
        }
      }

      /* Read vertex indices list. */
      uint32_t count;
      if (!read_list_count(file, prop, scratch, header.type == PlyFormatType::BINARY_BE, count)) {
        return "Could not read face size";
      }
      if (count < 1 || count > 255) {
        return "Invalid face size, must be between 1 and 255";
      }

      if (!read_list_values(file, prop, count, scratch)) {
        return "Could not read face vertex indices";
      }
      /* Previous python based importer was accepting faces with fewer
       * than 3 vertices, and silently dropping them. */
      if (count < 3) {
        CLOG_WARN(&LOG, "PLY Importer: ignoring face %i (%u vertices)", i, count);
      }
      else {
        ptr = scratch.data();
        if (header.type == PlyFormatType::BINARY_BE) {
          endian_switch_array(const_cast<uint8_t *>(ptr), data_type_size[prop.type], count);
        }
        for (int j = 0; j < count; ++j) {
          uint32_t index = get_binary_value<uint32_t>(prop.type, ptr);
          data->face_vertices.append(index);
        }
        data->face_sizes.append(count);
      }

      /* Skip any properties after vertex indices. */
      for (int j = prop_index + 1; j < element.properties.size(); j++) {
        if (!skip_property(
                file, element.properties[j], scratch, header.type == PlyFormatType::BINARY_BE))
        {
          return "Could not read face property";
        }
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
    if (count < 0) {
      return "Invalid tristrips size";
    }

    for (int j = 0; j < count; j++) {
      p = drop_whitespace(p, end);
      if (p == end) {
        return "Could not read tristrips";
      }
      int index;
      const char *next = parse_int(p, end, 0, index);
      if (next == p) {
        /* Not a number, can't advance properly so stop. */
        return "Could not read tristrips";
      }
      p = next;
      strip.append(index);
    }
  }
  else {
    Vector<uint8_t> scratch(64);

    const uint8_t *ptr;

    uint32_t count;
    if (!read_list_count(file, prop, scratch, header.type == PlyFormatType::BINARY_BE, count)) {
      return "Could not read tristrips size";
    }
    if (count > INT_MAX) {
      return "Invalid tristrips size";
    }
    if (!read_list_values(file, prop, count, scratch)) {
      return "Could not read tristrips";
    }

    strip.resize(count);
    ptr = scratch.data();
    if (header.type == PlyFormatType::BINARY_BE) {
      endian_switch_array(const_cast<uint8_t *>(ptr), data_type_size[prop.type], count);
    }
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
        std::swap(a, b);
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

  /* Double to represent all int and uint indices exactly, float has only 24 bits.
   * TODO: Ideally should parse as int directly, but would need to account for
   * other edge data that may be float. */
  Vector<double> value_vec(element.properties.size());
  Vector<uint8_t> scratch;
  if (header.type != PlyFormatType::ASCII) {
    scratch.resize(element.stride);
  }

  for (int i = 0; i < element.count; i++) {
    const char *error = nullptr;
    if (header.type == PlyFormatType::ASCII) {
      error = parse_row_ascii(file, element, value_vec);
    }
    else {
      error = parse_row_binary(file, header, element, scratch, value_vec);
    }
    if (error != nullptr) {
      return error;
    }
    const int index1 = float_to_value<int>(value_vec[prop_vertex1]);
    const int index2 = float_to_value<int>(value_vec[prop_vertex2]);
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
      /* Empty lines are valid rows for elements without properties. */
      if (!file.try_read_line()) {
        return "Could not read element";
      }
    }
  }
  else {
    Vector<uint8_t> scratch(64);
    for (int i = 0; i < element.count; i++) {
      for (const PlyProperty &prop : element.properties) {
        if (!skip_property(file, prop, scratch, header.type == PlyFormatType::BINARY_BE)) {
          return "Could not read element";
        }
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
    /* Binary rows can't be read with unknown property types, as their size is unknown.
     * Elements without rows read nothing, so they don't matter. */
    const bool has_unknown_type = header.type != PlyFormatType::ASCII && element.count > 0 &&
                                  std::any_of(element.properties.begin(),
                                              element.properties.end(),
                                              [](const PlyProperty &prop) {
                                                return prop.type == PlyDataTypes::NONE;
                                              });
    const bool is_known_element = ELEM(element.name, "vertex", "face", "tristrips", "edge");
    if (has_unknown_type && is_known_element) {
      data->error = "Unknown property type";
      return data;
    }

    const char *error = nullptr;
    if (!is_known_element) {
      /* Elements that are skipped don't affect the geometry. When they can't be
       * read, stop reading and keep the elements read so far. */
      if (has_unknown_type) {
        data->warning = "Unknown property type in element \"" + element.name +
                        "\", skipping remaining elements";
        break;
      }
      if (skip_element(file, header, element) != nullptr) {
        data->warning = "Could not read element \"" + element.name +
                        "\", skipping remaining elements";
        break;
      }
    }
    else if (element.name == "vertex") {
      if (got_vertex) {
        /* Vertex arrays are sized per element, so multiple vertex elements would mismatch. */
        data->error = "Multiple vertex elements are not supported";
        return data;
      }
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

}  // namespace io::ply
}  // namespace blender
