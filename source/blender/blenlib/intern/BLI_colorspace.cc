/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_colorspace.hh"

namespace blender {

namespace colorspace {

/* These are initialized in colormanagement.cc based on the OpenColorIO config,
 * but until then default to Rec.709 scene linear. */

float3 luma_coefficients(0.2126f, 0.7152f, 0.0722f);

float3x3 scene_linear_to_rec709 = float3x3::identity();
float3x3 rec709_to_scene_linear = float3x3::identity();
bool scene_linear_is_rec709 = true;

float3x3 scene_linear_to_xyz = float3x3::zero();
float3x3 xyz_to_scene_linear = float3x3::zero();
float3x3 scene_linear_to_rec2020 = float3x3::zero();
float3x3 rec2020_to_scene_linear = float3x3::zero();
float3x3 scene_linear_to_aces = float3x3::zero();
float3x3 aces_to_scene_linear = float3x3::zero();
float3x3 scene_linear_to_acescg = float3x3::zero();
float3x3 acescg_to_scene_linear = float3x3::zero();

};  // namespace colorspace

}  // namespace blender
