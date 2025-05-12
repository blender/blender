/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_processor.hh"

#if defined(WITH_OPENCOLORIO)

#  include "error_handling.hh"

namespace blender::ocio {

OCIO_NAMESPACE::ConstProcessorRcPtr create_ocio_processor(
    const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
    const StringRefNull from_colorspace,
    const StringRefNull to_colorspace)
{
  try {
    OCIO_NAMESPACE::ConstProcessorRcPtr processor = ocio_config->getProcessor(
        from_colorspace.c_str(), to_colorspace.c_str());
    return processor;
  }
  catch (OCIO_NAMESPACE::Exception &exception) {
    report_exception(exception);
  }
  return nullptr;
}

OCIO_NAMESPACE::ConstProcessorRcPtr create_ocio_processor_silent(
    const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
    const StringRefNull from_colorspace,
    const StringRefNull to_colorspace)
{
  try {
    OCIO_NAMESPACE::ConstProcessorRcPtr processor = ocio_config->getProcessor(
        from_colorspace.c_str(), to_colorspace.c_str());
    return processor;
  }
  catch (OCIO_NAMESPACE::Exception & /*exception*/) {
  }
  return nullptr;
}

}  // namespace blender::ocio

#endif
