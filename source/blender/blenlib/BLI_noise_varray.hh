/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * This contains various utilities to evaluate noise functions on many elements efficiently.
 */

#include "BLI_color_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_virtual_array.hh"

namespace blender::noise {

void white_noise_float_1d(const IndexMask &mask,
                          const VArray<float> &ks,
                          MutableSpan<float> r_values);
void white_noise_float_2d(const IndexMask &mask,
                          const VArray<float3> &ks,
                          MutableSpan<float> r_values);
void white_noise_float_3d(const IndexMask &mask,
                          const VArray<float3> &ks,
                          MutableSpan<float> r_values);
void white_noise_float_4d(const IndexMask &mask,
                          const VArray<float3> &vectors,
                          const VArray<float> &ws,
                          MutableSpan<float> r_values);

void white_noise_color_1d(const IndexMask &mask,
                          const VArray<float> &ks,
                          MutableSpan<ColorGeometry4f> r_values);
void white_noise_color_2d(const IndexMask &mask,
                          const VArray<float3> &ks,
                          MutableSpan<ColorGeometry4f> r_values);
void white_noise_color_3d(const IndexMask &mask,
                          const VArray<float3> &ks,
                          MutableSpan<ColorGeometry4f> r_values);
void white_noise_color_4d(const IndexMask &mask,
                          const VArray<float3> &vectors,
                          const VArray<float> &ws,
                          MutableSpan<ColorGeometry4f> r_values);

struct CommonPerlinVArrayParams {
  int type;
  bool normalize;
  const IndexMask &mask;
  const VArray<float> &scale;
  const VArray<float> &detail;
  const VArray<float> &roughness;
  const VArray<float> &lacunarity;
  const VArray<float> &offset;
  const VArray<float> &gain;
  const VArray<float> &distortion;
};

/**
 * Compute perlin noise function on many elements at once. Note that #perlin_split_mask has to be
 * used with these functions.
 */
void perlin_float_fractal_distorted_varray_1d(const CommonPerlinVArrayParams &params,
                                              const VArray<float> &position,
                                              MutableSpan<float> r_values);
void perlin_float_fractal_distorted_varray_2d(const CommonPerlinVArrayParams &params,
                                              const VArray<float3> &position,
                                              MutableSpan<float> r_values);
void perlin_float_fractal_distorted_varray_3d(const CommonPerlinVArrayParams &params,
                                              const VArray<float3> &position,
                                              MutableSpan<float> r_values);
void perlin_float_fractal_distorted_varray_4d(const CommonPerlinVArrayParams &params,
                                              const VArray<float3> &position_3d,
                                              const VArray<float> &position_1d,
                                              MutableSpan<float> r_values);
void perlin_color_fractal_distorted_varray_1d(const CommonPerlinVArrayParams &params,
                                              const VArray<float> &position,
                                              MutableSpan<ColorGeometry4f> r_values);
void perlin_color_fractal_distorted_varray_2d(const CommonPerlinVArrayParams &params,
                                              const VArray<float3> &position,
                                              MutableSpan<ColorGeometry4f> r_values);
void perlin_color_fractal_distorted_varray_3d(const CommonPerlinVArrayParams &params,
                                              const VArray<float3> &position,
                                              MutableSpan<ColorGeometry4f> r_values);
void perlin_color_fractal_distorted_varray_4d(const CommonPerlinVArrayParams &params,
                                              const VArray<float3> &position_3d,
                                              const VArray<float> &position_1d,
                                              MutableSpan<ColorGeometry4f> r_values);

/**
 * The perlin noise functions can optimize better when they know that all elements use the same
 * code path (e.g. have the same number of octaves). This function splits up a mask such that all
 * indices in the same mask can be evaluated together.
 *
 * Ideally the detail and distortion values are constant, in which case the mask does not have to
 * be split.
 */
Vector<IndexMask> perlin_split_mask(LinearAllocator<> &allocator,
                                    const IndexMask &mask,
                                    const VArray<float> &detail,
                                    const VArray<float> &distortion);

}  // namespace blender::noise
