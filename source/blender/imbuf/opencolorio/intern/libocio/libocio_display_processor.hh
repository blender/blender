/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include "BLI_string_ref.hh"

#  include "../opencolorio.hh"

namespace blender::ocio {

class LibOCIOConfig;
struct DisplayParameters;

OCIO_NAMESPACE::ConstProcessorRcPtr create_ocio_display_processor(
    const LibOCIOConfig &config, const DisplayParameters &display_parameters);

OCIO_NAMESPACE::TransformRcPtr create_ocio_display_transform(
    const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
    StringRefNull display,
    StringRefNull view,
    StringRefNull look,
    StringRefNull from_colorspace);

}  // namespace blender::ocio

#endif
