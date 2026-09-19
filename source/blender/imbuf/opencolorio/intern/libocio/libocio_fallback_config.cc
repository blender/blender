/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <sstream>

#include "BLI_assert.hh"

#include "error_handling.hh"
#include "libocio_config.hh"

namespace blender::ocio {

/* Minimal configuration with linear Rec.709, sRGB and non-color data. It uses only built-in
 * transforms, so that loading it depends on no file. */
static const char *fallback_config_source = R"(
ocio_profile_version: 2.3

name: Blender Fallback
description: Built-in configuration, used when no other configuration could be loaded
strictparsing: true
luma: [0.2126, 0.7152, 0.0722]

roles:
  scene_linear: Linear Rec.709
  rendering: Linear Rec.709
  default: Linear Rec.709
  default_byte: sRGB
  default_float: Linear Rec.709
  default_sequencer: sRGB
  color_picking: sRGB
  texture_paint: Linear Rec.709
  data: Non-Color
  aces_interchange: ACES2065-1
  cie_xyz_d65_interchange: Linear CIE-XYZ D65
  color_timing: Linear Rec.709
  compositing_log: Linear Rec.709

displays:
  sRGB:
    - !<View> {name: Standard, view_transform: Standard, display_colorspace: sRGB}

active_displays: [sRGB]
active_views: [Standard]
inactive_colorspaces: [ACES2065-1, Linear CIE-XYZ D65]

display_colorspaces:
  - !<ColorSpace>
    name: Linear CIE-XYZ D65
    aliases: [lin_ciexyzd65_scene]
    isdata: false
    encoding: display-linear

  - !<ColorSpace>
    name: sRGB
    aliases: [srgb_rec709_display]
    isdata: false
    from_display_reference: !<GroupTransform>
      children:
        - !<MatrixTransform> {matrix: [3.2409699, -1.5373832, -0.4986108, 0, -0.9692436, 1.8759675, 0.0415551, 0, 0.0556301, -0.2039770, 1.0569715, 0, 0, 0, 0, 1]}
        - !<ExponentWithLinearTransform> {gamma: 2.4, offset: 0.055, direction: inverse}

default_view_transform: Standard

view_transforms:
  - !<ViewTransform>
    name: Standard
    from_scene_reference: !<MatrixTransform> {matrix: [3.2409699, -1.5373832, -0.4986108, 0, -0.9692436, 1.8759675, 0.0415551, 0, 0.0556301, -0.2039770, 1.0569715, 0, 0, 0, 0, 1], direction: inverse}

colorspaces:
  - !<ColorSpace>
    name: Linear Rec.709
    aliases: [lin_rec709_scene, Linear]
    isdata: false

  - !<ColorSpace>
    name: ACES2065-1
    aliases: [lin_ap0_scene]
    isdata: false
    to_scene_reference: !<GroupTransform>
      children:
        - !<BuiltinTransform> {style: UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD}
        - !<MatrixTransform> {matrix: [3.2409699, -1.5373832, -0.4986108, 0, -0.9692436, 1.8759675, 0.0415551, 0, 0.0556301, -0.2039770, 1.0569715, 0, 0, 0, 0, 1]}

  - !<ColorSpace>
    name: Non-Color
    isdata: true
)";

std::unique_ptr<Config> LibOCIOConfig::create_fallback()
{
  try {
    std::istringstream stream(fallback_config_source);
    return std::unique_ptr<LibOCIOConfig>(
        new LibOCIOConfig(OCIO_NAMESPACE::Config::CreateFromStream(stream)));
  }
  catch (OCIO_NAMESPACE::Exception &exception) {
    report_exception(exception);
  }

  BLI_assert_unreachable();
  return nullptr;
}

}  // namespace blender::ocio
