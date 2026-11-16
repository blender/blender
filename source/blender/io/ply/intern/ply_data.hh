/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include "BLI_generic_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::io::ply {

enum PlyDataTypes { NONE, CHAR, UCHAR, SHORT, USHORT, INT, UINT, FLOAT, DOUBLE, PLY_TYPE_COUNT };

inline constexpr int data_type_size[] = {0, 1, 1, 2, 2, 4, 4, 4, 8};
static_assert(std::size(data_type_size) == PLY_TYPE_COUNT, "PLY data type size table mismatch");

struct PlyCustomAttribute {
  PlyCustomAttribute(const StringRef name_,
                     const int64_t size,
                     const CPPType &type = CPPType::get<float>())
      : name(name_), buffer_(type, size, NoInitialization())
  {
    type.value_initialize_n(buffer_.data(), size);
    size_ = size;
  }

  std::string name;

  const CPPType &type() const
  {
    return buffer_.type();
  }
  int64_t size() const
  {
    return size_;
  }
  GSpan data() const
  {
    return buffer_.as_span().take_front(size_);
  }
  GMutableSpan data_for_write()
  {
    return buffer_.as_mutable_span().take_front(size_);
  }

  /* Resize, filling new elements with zero. Geometric capacity growth. */
  void resize(const int64_t new_size)
  {
    const CPPType &type = buffer_.type();
    if (new_size > buffer_.size()) {
      const int64_t capacity = std::max(new_size, buffer_.size() * 2);
      GArray<> new_buffer(type, capacity, NoInitialization());
      type.copy_construct_n(buffer_.data(), new_buffer.data(), size_);
      buffer_ = std::move(new_buffer);
    }
    if (new_size > size_) {
      type.value_initialize_n(buffer_[size_], new_size - size_);
    }
    size_ = new_size;
  }

 private:
  GArray<> buffer_;
  int64_t size_ = 0;
};

struct PlyData {
  Vector<float3> vertices;
  Vector<float3> vertex_normals;
  Vector<float4> vertex_colors;
  /* Export colors as floats when their precision or range requires it. */
  bool vertex_colors_are_float = false;
  Vector<PlyCustomAttribute> vertex_custom_attr;
  Vector<std::pair<int, int>> edges;
  Vector<uint32_t> face_vertices;
  Vector<uint32_t> face_sizes;
  Vector<float2> uv_coordinates;
  /* Fatal error in import. */
  std::string error;
  /* Geometry was imported, but some data could not be read. */
  std::string warning;
};

enum PlyFormatType { ASCII, BINARY_LE, BINARY_BE };

struct PlyProperty {
  std::string name;
  PlyDataTypes type = PlyDataTypes::NONE;
  PlyDataTypes count_type = PlyDataTypes::NONE; /* NONE means it's not a list property */
};

struct PlyElement {
  std::string name;
  int count = 0;
  Vector<PlyProperty> properties;
  int stride = 0;

  void calc_stride();
};

struct PlyHeader {
  Vector<PlyElement> elements;
  PlyFormatType type = PlyFormatType::ASCII;
};

}  // namespace blender::io::ply
