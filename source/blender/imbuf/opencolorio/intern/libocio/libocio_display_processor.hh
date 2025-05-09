/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include "../opencolorio.hh"

namespace blender::ocio {

class LibOCIOConfig;
struct DisplayParameters;

OCIO_NAMESPACE::ConstProcessorRcPtr create_ocio_display_processor(
    const LibOCIOConfig &config, const DisplayParameters &display_parameters);

}  // namespace blender::ocio

#endif
