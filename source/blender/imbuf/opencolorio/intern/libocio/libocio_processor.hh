/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_string_ref.hh"

#include "../opencolorio.hh"

namespace blender::ocio {

class LibOCIOConfig;

/**
 * Create OpenColorIO processor between color spaces.
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

/**
 * Create OpenColorIO processor that converts between color spaces in different
 * configs, through standard interchange roles.
 * If the processor can not be created returns nullptr.
 */
OCIO_NAMESPACE::ConstProcessorRcPtr create_ocio_processor_between_configs(
    const OCIO_NAMESPACE::ConstConfigRcPtr &from_config,
    StringRefNull from_colorspace,
    const OCIO_NAMESPACE::ConstConfigRcPtr &to_config,
    StringRefNull to_colorspace);

}  // namespace blender::ocio
