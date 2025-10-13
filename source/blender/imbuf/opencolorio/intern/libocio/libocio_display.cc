/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_display.hh"
#include "OCIO_display.hh"

#if defined(WITH_OPENCOLORIO)

#  include "BLI_index_range.hh"

#  include "OCIO_config.hh"

#  include "CLG_log.h"

#  include "../opencolorio.hh"

#  include "error_handling.hh"
#  include "libocio_config.hh"
#  include "libocio_cpu_processor.hh"
#  include "libocio_display_processor.hh"

static CLG_LogRef LOG = {"color_management"};

namespace blender::ocio {

static OCIO_NAMESPACE::ConstColorSpaceRcPtr get_display_view_colorspace(
    const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config, const char *display, const char *view)
{
  const char *display_colorspace = ocio_config->getDisplayViewColorSpaceName(display, view);
  if (display_colorspace == nullptr) {
    return nullptr;
  }

  /* Shared view transforms can use this special display name to indicate
   * the display colorspace name is the same as the display name. */
  if (STREQ(display_colorspace, "<USE_DISPLAY_NAME>")) {
    return ocio_config->getColorSpace(display);
  }

  return ocio_config->getColorSpace(display_colorspace);
}

LibOCIODisplay::LibOCIODisplay(const int index, const LibOCIOConfig &config) : config_(&config)
{
  const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config = config.get_ocio_config();

  this->index = index;

  name_ = ocio_config->getDisplay(index);

  CLOG_TRACE(&LOG, "Add display: %s", name_.c_str());

  /* Initialize views. */
  const int num_views = ocio_config->getNumViews(name_.c_str());
  if (num_views < 0) {
    report_error("Invalid OpenColorIO configuration: negative number of views");
    return;
  }

  /* Try to assign a display colorspace to every view even if missing. In particular for
   * Raw we still want to set the colorspace. */
  OCIO_NAMESPACE::ConstColorSpaceRcPtr ocio_fallback_display_colorspace;
  for (const int view_index : IndexRange(num_views)) {
    const char *view_name = ocio_config->getView(name_.c_str(), view_index);
    ocio_fallback_display_colorspace = get_display_view_colorspace(
        ocio_config, name_.c_str(), view_name);
    if (ocio_fallback_display_colorspace &&
        ocio_fallback_display_colorspace->getReferenceSpaceType() ==
            OCIO_NAMESPACE::REFERENCE_SPACE_DISPLAY)
    {
      break;
    }
    ocio_fallback_display_colorspace.reset();
  }

  bool support_emulation = config.get_color_space(OCIO_NAMESPACE::ROLE_INTERCHANGE_DISPLAY) !=
                           nullptr;

  views_.reserve(num_views);
  for (const int view_index : IndexRange(num_views)) {
    const char *view_name = ocio_config->getView(name_.c_str(), view_index);

    OCIO_NAMESPACE::ConstColorSpaceRcPtr ocio_display_colorspace = get_display_view_colorspace(
        ocio_config, name_.c_str(), view_name);
    if (!ocio_display_colorspace) {
      ocio_display_colorspace = ocio_fallback_display_colorspace;
    }

    /* There does not exist a description for displays, if there is an associated display
     * colorspace it's likely to be a useful description. */
    if (description_.is_empty() && ocio_display_colorspace &&
        ocio_display_colorspace->getReferenceSpaceType() ==
            OCIO_NAMESPACE::REFERENCE_SPACE_DISPLAY)
    {
      description_ = ocio_display_colorspace->getDescription();
    }

    const char *view_description = nullptr;
    const char *view_transform_name = ocio_config->getDisplayViewTransformName(name_.c_str(),
                                                                               view_name);
    if (view_transform_name) {
      const OCIO_NAMESPACE::ConstViewTransformRcPtr view_transform = ocio_config->getViewTransform(
          view_transform_name);
      if (view_transform) {
        view_description = view_transform->getDescription();
      }
    }
    if (view_description == nullptr) {
      view_description = "";
    }

    /* Detect if view is HDR, through encoding of display colorspace. */
    bool view_is_hdr = false;
    if (ocio_display_colorspace) {
      StringRefNull encoding = ocio_display_colorspace->getEncoding();
      view_is_hdr = encoding == "hdr-video" || encoding == "edr-video";
      is_hdr_ |= view_is_hdr;
    }

    /* Detect if display emulation is supported. */
    bool view_support_emulation = support_emulation && ocio_display_colorspace &&
                                  ocio_display_colorspace->getReferenceSpaceType() ==
                                      OCIO_NAMESPACE::REFERENCE_SPACE_DISPLAY;

    /* Detect gamut and transfer function through interop ID. When unknown, things
     * should still work correctly but may miss optimizations. */
    Gamut gamut = Gamut::Unknown;
    TransferFunction transfer_function = TransferFunction::Unknown;

    const LibOCIOColorSpace *display_colorspace =
        (ocio_display_colorspace) ? static_cast<const LibOCIOColorSpace *>(config.get_color_space(
                                        ocio_display_colorspace->getName())) :
                                    nullptr;
    StringRefNull display_interop_id = (display_colorspace) ? display_colorspace->interop_id() :
                                                              "";

    if (!display_interop_id.is_empty()) {
      if (display_interop_id.endswith("_rec709_display") ||
          display_interop_id.endswith("_rec709_scene"))
      {
        gamut = Gamut::Rec709;
      }
      else if (display_interop_id.endswith("_p3d65_display") ||
               display_interop_id.endswith("_p3d65_scene"))
      {
        gamut = Gamut::P3D65;
      }
      else if (display_interop_id.endswith("_rec2020_display") ||
               display_interop_id.endswith("_rec2020_scene"))
      {
        gamut = Gamut::Rec2020;
      }

      if (display_interop_id.startswith("srgb_")) {
        transfer_function = TransferFunction::sRGB;
      }
      else if (display_interop_id.startswith("srgbe_")) {
        transfer_function = TransferFunction::ExtendedsRGB;
      }
      else if (display_interop_id.startswith("pq_")) {
        transfer_function = TransferFunction::PQ;
      }
      else if (display_interop_id.startswith("hlg_")) {
        transfer_function = TransferFunction::HLG;
      }
      else if (display_interop_id.startswith("g18_")) {
        transfer_function = TransferFunction::Gamma18;
      }
      else if (display_interop_id.startswith("g22_")) {
        transfer_function = TransferFunction::Gamma22;
      }
      else if (display_interop_id.startswith("g24_")) {
        transfer_function = TransferFunction::Gamma24;
      }
      else if (display_interop_id.startswith("g26_")) {
        transfer_function = TransferFunction::Gamma26;
      }
    }

    CLOG_TRACE(&LOG,
               "  Add view: %s (colorspace: %s, %s)",
               view_name,
               display_colorspace ? display_colorspace->name().c_str() : "<none>",
               view_is_hdr ? "HDR" : "SDR");

    views_.append_as(view_index,
                     view_name,
                     view_description,
                     view_is_hdr,
                     view_support_emulation,
                     gamut,
                     transfer_function,
                     display_colorspace);
  }

  /* Detect untonemppaed view transform. */
  if (untonemapped_view_ == nullptr) {
    /* Use Blender config and ACES config naming conventions. */
    for (const LibOCIOView &view : views_) {
      if (view.name() == "Un-tone-mapped" || view.name() == "Standard") {
        untonemapped_view_ = &view;
        break;
      }
    }
    if (untonemapped_view_ == nullptr) {
      /* Use config wide default view transform between reference and display spaces.
       * Note this is not always the same as the default view transform of the display. */
      const char *default_view_transform = ocio_config->getDefaultViewTransformName();
      for (const LibOCIOView &view : views_) {
        if (view.name() == default_view_transform) {
          untonemapped_view_ = &view;
          break;
        }
      }
    }
  }

  /* Hide redundant suffix from ACES config. */
  if (name_.endswith(" - Display")) {
    ui_name_ = StringRef(name_).drop_known_suffix(" - Display");
  }
}

const View *LibOCIODisplay::get_untonemapped_view() const
{
  return untonemapped_view_;
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

std::unique_ptr<LibOCIOCPUProcessor> LibOCIODisplay::create_scene_linear_cpu_processor(
    const bool use_display_emulation, const bool inverse) const
{
  const View *view = get_untonemapped_view();
  if (view == nullptr) {
    view = get_default_view();
  }

  DisplayParameters display_parameters;
  display_parameters.from_colorspace = OCIO_NAMESPACE::ROLE_SCENE_LINEAR;
  display_parameters.view = view->name();
  display_parameters.display = name_;
  display_parameters.inverse = inverse;
  display_parameters.use_display_emulation = use_display_emulation;
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
}

const CPUProcessor *LibOCIODisplay::get_to_scene_linear_cpu_processor(
    const bool use_display_emulation) const
{
  const CPUProcessorCache &cache = (use_display_emulation) ?
                                       to_scene_linear_emulation_cpu_processor_ :
                                       to_scene_linear_cpu_processor_;
  return cache.get([&] { return create_scene_linear_cpu_processor(use_display_emulation, true); });
}

const CPUProcessor *LibOCIODisplay::get_from_scene_linear_cpu_processor(
    const bool use_display_emulation) const
{
  const CPUProcessorCache &cache = (use_display_emulation) ?
                                       from_scene_linear_emulation_cpu_processor_ :
                                       from_scene_linear_cpu_processor_;
  return cache.get(
      [&] { return create_scene_linear_cpu_processor(use_display_emulation, false); });
}

void LibOCIODisplay::clear_caches()
{
  to_scene_linear_cpu_processor_ = CPUProcessorCache();
  to_scene_linear_emulation_cpu_processor_ = CPUProcessorCache();
  from_scene_linear_cpu_processor_ = CPUProcessorCache();
  from_scene_linear_emulation_cpu_processor_ = CPUProcessorCache();
}

}  // namespace blender::ocio

#endif
