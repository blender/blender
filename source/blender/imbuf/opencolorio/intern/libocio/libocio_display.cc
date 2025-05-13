/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_display.hh"

#if defined(WITH_OPENCOLORIO)

#  include "BLI_index_range.hh"

#  include "OCIO_config.hh"

#  include "../opencolorio.hh"

#  include "error_handling.hh"
#  include "libocio_config.hh"
#  include "libocio_cpu_processor.hh"
#  include "libocio_display_processor.hh"

namespace blender::ocio {

LibOCIODisplay::LibOCIODisplay(const int index, const LibOCIOConfig &config) : config_(&config)
{
  const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config = config.get_ocio_config();

  this->index = index;

  name_ = ocio_config->getDisplay(index);

  /* Initialize views. */
  const int num_views = ocio_config->getNumViews(name_.c_str());
  if (num_views < 0) {
    report_error("Invalid OpenColorIO configuration: negative number of views");
    return;
  }
  views_.reserve(num_views);
  for (const int view_index : IndexRange(num_views)) {
    const char *view_name = ocio_config->getView(name_.c_str(), view_index);
    views_.append_as(view_index, view_name);
  }
}

const View *LibOCIODisplay::get_view_by_name(const StringRefNull name) const
{
  /* TODO(sergey): Is there faster way to lookup Blender-side view? */
  for (const LibOCIOView &view : views_) {
    if (view.name() == name) {
      return &view;
    }
  }
  return nullptr;
}

int LibOCIODisplay::get_num_views() const
{
  return views_.size();
}

const View *LibOCIODisplay::get_view_by_index(const int index) const
{
  if (index < 0 || index >= views_.size()) {
    return nullptr;
  }
  return &views_[index];
}

const CPUProcessor *LibOCIODisplay::get_to_scene_linear_cpu_processor() const
{
  return to_scene_linear_cpu_processor_.get([&]() -> std::unique_ptr<CPUProcessor> {
    DisplayParameters display_parameters;
    display_parameters.from_colorspace = OCIO_NAMESPACE::ROLE_SCENE_LINEAR;
    display_parameters.view = get_default_view()->name();
    display_parameters.display = name_;
    display_parameters.inverse = true;
    OCIO_NAMESPACE::ConstProcessorRcPtr ocio_processor = create_ocio_display_processor(
        *config_, display_parameters);
    if (!ocio_processor) {
      return nullptr;
    }

    OCIO_NAMESPACE::ConstCPUProcessorRcPtr ocio_cpu_processor =
        ocio_processor->getDefaultCPUProcessor();
    if (!ocio_cpu_processor) {
      return nullptr;
    }

    return std::make_unique<LibOCIOCPUProcessor>(ocio_cpu_processor);
  });
}

const CPUProcessor *LibOCIODisplay::get_from_scene_linear_cpu_processor() const
{
  return from_scene_linear_cpu_processor_.get([&]() -> std::unique_ptr<CPUProcessor> {
    DisplayParameters display_parameters;
    display_parameters.from_colorspace = OCIO_NAMESPACE::ROLE_SCENE_LINEAR;
    display_parameters.view = get_default_view()->name();
    display_parameters.display = name_;
    OCIO_NAMESPACE::ConstProcessorRcPtr ocio_processor = create_ocio_display_processor(
        *config_, display_parameters);
    if (!ocio_processor) {
      return nullptr;
    }

    OCIO_NAMESPACE::ConstCPUProcessorRcPtr ocio_cpu_processor =
        ocio_processor->getDefaultCPUProcessor();
    if (!ocio_cpu_processor) {
      return nullptr;
    }

    return std::make_unique<LibOCIOCPUProcessor>(ocio_cpu_processor);
  });
}

}  // namespace blender::ocio

#endif
