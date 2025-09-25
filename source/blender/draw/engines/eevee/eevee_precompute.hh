/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * LUT generation module.
 */

#pragma once

#include "BLI_math_vector_types.hh"

#include "eevee_precompute_shared.hh"

#include "draw_manager.hh"

#include <fstream>

namespace blender::eevee {

using namespace draw;

/**
 * Create a look-up table of the specified type using GPU compute.
 * Not to be used at runtime in final release.
 * Usage example: `Precompute(manager, LUT_GGX_BRDF_SPLIT_SUM, {64, 64, 1}).data<float2>()`
 */
class Precompute {
 private:
  int3 table_extent_;
  float4 *raw_data_ = nullptr;

 public:
  Precompute(draw::Manager &manager, PrecomputeType type, int3 table_extent);
  ~Precompute();

  /* Cast each pixel data to type `T`. */
  template<typename T> Vector<T> data()
  {
    int64_t table_len = table_extent_.x * table_extent_.y * table_extent_.z;
    Vector<T> out_data(table_len);
    for (auto i : IndexRange(table_len)) {
      out_data[i] = T(raw_data_[i]);
    }
    return out_data;
  }

  /**
   * Write the content of a texture to a PFM image file for inspection.
   * OpenGL texture coordinate convention with Y up is respected.
   */
  template<typename VecT>
  static void write_to_pfm(StringRefNull name,
                           Span<VecT> pixels,
                           int64_t n_x,
                           int64_t n_y = 1,
                           int64_t n_z = 1,
                           int64_t n_w = 1)
  {
    BLI_STATIC_ASSERT(VecT::type_length < 4, "4 component PFM are not possible");

    std::ofstream file;

    /* Write PFM header. */
    file.open(std::string(name) + ".pfm");
    file << "PF\n";
    file << n_x * n_z << " " << n_y * n_w << "\n";
    /* NOTE: this is endianness-sensitive.
     * Big endian system would have needed `1.0` value instead. */
    file << "-1.0\n";
    file.close();

    /* Write binary float content. */
    file.open(std::string(name) + ".pfm", std::ios_base::app | std::ios::out | std::ios::binary);
    /* Iterate over destination pixels. */
    for (int64_t y : IndexRange(n_y * n_w)) {
      for (int64_t x : IndexRange(n_x * n_z)) {
        int64_t src_w = y / n_y;
        int64_t src_z = x / n_x;
        int64_t src_y = y % n_y;
        int64_t src_x = x % n_x;
        int64_t src = (n_x * n_y * n_z * src_w) + (n_x * n_y * src_z) + (n_x * src_y) + src_x;
        float3 data(0.0f);
        for (int c : IndexRange(VecT::type_length)) {
          data[c] = pixels[src][c];
        }
        file.write(reinterpret_cast<char *>(&data), sizeof(float3));
      }
    }
    file.close();
  }

  /**
   * Write the content of a texture as a C++ header file array.
   * The content is to be copied to `eevee_lut.cc` and formatted with `make format`.
   */
  template<typename VecT>
  static void write_to_header(StringRefNull name,
                              Span<VecT> pixels,
                              int64_t n_x,
                              int64_t n_y = 1,
                              int64_t n_z = 1,
                              int64_t n_w = 1)
  {
    std::ofstream file;

    file.open(std::string(name) + ".hh");
    file << "const float " << name;
    if (n_w > 1) {
      file << "[" << n_w << "]";
    }
    if (n_z > 1) {
      file << "[" << n_z << "]";
    }
    if (n_y > 1) {
      file << "[" << n_y << "]";
    }
    if (n_x > 1) {
      file << "[" << n_x << "]";
    }
    file << "[" << VecT::type_length << "]";
    file << " = {\n";
    /* Print data formatted as C++ array. */
    for (auto w : IndexRange(n_w)) {
      if (n_w > 1) {
        file << "{\n";
      }
      for (auto z : IndexRange(n_z)) {
        if (n_z > 1 || n_w > 1) {
          file << "{\n";
        }
        for (auto y : IndexRange(n_y)) {
          if (n_y > 1 || n_z > 1 || n_w > 1) {
            file << "{\n";
          }
          for (auto x : IndexRange(n_x)) {
            if (n_x > 1 || n_y > 1 || n_z > 1 || n_w > 1) {
              file << "{";
            }
            int64_t pixel_index = (n_x * n_y * n_z * w) + (n_x * n_y * z) + (n_x * y) + x;
            for (auto c : IndexRange(VecT::type_length)) {
              file << std::to_string(pixels[pixel_index][c]);
              if (c + 1 < VecT::type_length) {
                file << "f, ";
              }
              else {
                file << "f";
              }
            }
            if (n_x > 1 || n_y > 1 || n_z > 1 || n_w > 1) {
              file << (x + 1 < n_x ? "}, " : "}");
            }
          }
          if (n_y > 1 || n_z > 1 || n_w > 1) {
            file << (y + 1 < n_y ? "},\n" : "}\n");
          }
        }
        if (n_z > 1 || n_w > 1) {
          file << (z + 1 < n_z ? "},\n" : "}\n");
        }
      }
      if (n_w > 1) {
        file << (w + 1 < n_w ? "},\n" : "}\n");
      }
    }
    file << "};\n";
    file.close();
  }
};

}  // namespace blender::eevee
