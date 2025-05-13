/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include "BLI_string_ref.hh"

#  include "../opencolorio.hh"

namespace blender::ocio {

class LibOCIOConfig;

/**
 * Create OpenColorIO processor between frames.
 * If the processor can not be created returns nullptr.
 *
 * The silent version does not print any errors if the processor creation has failed.
 */
OCIO_NAMESPACE::ConstProcessorRcPtr create_ocio_processor(
    const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
    StringRefNull from_colorspace,
    StringRefNull to_colorspace);
OCIO_NAMESPACE::ConstProcessorRcPtr create_ocio_processor_silent(
    const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
    StringRefNull from_colorspace,
    StringRefNull to_colorspace);

}  // namespace blender::ocio

#endif
