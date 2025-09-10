/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

namespace blender {

namespace colorspace {

/* Coefficients to compute luma from RGB. */
extern float3 luma_coefficients;

/* Conversion between scene linear and common linear spaces. */
extern float3x3 scene_linear_to_xyz;
extern float3x3 xyz_to_scene_linear;
extern float3x3 scene_linear_to_aces;
extern float3x3 aces_to_scene_linear;
extern float3x3 scene_linear_to_acescg;
extern float3x3 acescg_to_scene_linear;
extern float3x3 scene_linear_to_rec709;
extern float3x3 rec709_to_scene_linear;
extern float3x3 scene_linear_to_rec2020;
extern float3x3 rec2020_to_scene_linear;

/* For fast path when converting to/from sRGB. */
extern bool scene_linear_is_rec709;

};  // namespace colorspace

}  // namespace blender
