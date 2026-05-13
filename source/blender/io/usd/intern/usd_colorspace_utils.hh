/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_color.hh"
#include "BLI_span.hh"

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdShade/shader.h>

namespace blender {
struct Image;
}

namespace blender::io::usd {

/** Get the interop ID for tagging exported USD stages. */
pxr::TfToken colorspace_scene_linear_interop_id();

/** Tag a prim with the scene linear color space. */
void colorspace_apply_to_prim(const pxr::UsdPrim &prim);

/** Convert an imported USD color to scene linear. */
void colorspace_attr_to_scene_linear(const pxr::UsdAttribute &attr, pxr::GfVec3f &color);
void colorspace_attr_to_scene_linear(const pxr::UsdAttribute &attr, ColorGeometry4f &color);

/** Convert imported USD color array to scene linear. */
void colorspace_attr_to_scene_linear(const pxr::UsdAttribute &attr,
                                     MutableSpan<ColorGeometry4f> colors);

/** Set the colorspace on an exported USD texture shader from a Blender image. */
void colorspace_from_image_texture(const Image *image, pxr::UsdShadeShader &shader);

/** Set the Blender image colorspace from an imported USD texture shader. */
void colorspace_to_image_texture(const pxr::UsdShadeShader &usd_shader,
                                 const pxr::UsdShadeInput &file_input,
                                 bool is_data,
                                 Image *image);

}  // namespace blender::io::usd
