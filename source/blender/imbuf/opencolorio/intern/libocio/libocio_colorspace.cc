/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_colorspace.hh"
#include "OCIO_cpu_processor.hh"
#include "error_handling.hh"
#include "intern/cpu_processor_cache.hh"

#include "CLG_log.h"

#include "../description.hh"
#include "libocio_cpu_processor.hh"
#include "libocio_processor.hh"

namespace blender {

static CLG_LogRef LOG = {"color_management"};

namespace ocio {

LibOCIOColorSpace::LibOCIOColorSpace(const int index,
                                     const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
                                     const OCIO_NAMESPACE::ConstColorSpaceRcPtr &ocio_color_space)
    : ocio_config_(ocio_config),
      ocio_color_space_(ocio_color_space),
      clean_description_(cleanup_description(ocio_color_space->getDescription()))
{
  const char *family = ocio_color_space->getFamily();
  this->family_ = (family) ? family : "";
  this->index = index;

#if OCIO_VERSION_HEX >= 0x02050000
  interop_id_ = ocio_color_space->getInteropID();
#endif

  if (interop_id_.is_empty()) {
    /* For older configs and older OpenColorIO versions, check the aliases as fallback.
     * This is a convention used in the Blender and ACES 2.0 configs. */
    const int num_aliases = ocio_color_space->getNumAliases();
    for (int i = 0; interop_id_.is_empty() && i < num_aliases; i++) {
      StringRefNull alias = ocio_color_space->getAlias(i);
      if (alias == "srgb_display") {
        interop_id_ = "srgb_rec709_display";
      }
      else if (alias == "displayp3_display") {
        interop_id_ = "srgb_p3d65_display";
      }
      else if (alias == "displayp3_hdr_display") {
        interop_id_ = "srgbe_p3d65_display";
      }
      else if (alias == "p3d65_display") {
        interop_id_ = "g26_p3d65_display";
      }
      else if (alias == "rec1886_rec709_display") {
        interop_id_ = "g24_rec709_display";
      }
      else if (alias == "g24_rec2020_display") {
        interop_id_ = "blender:g24_rec2020_display";
      }
      else if (alias == "rec2100_pq_display") {
        interop_id_ = "pq_rec2020_display";
      }
      else if (alias == "rec2100_hlg_display") {
        interop_id_ = "hlg_rec2020_display";
      }
      else if (alias == "st2084_p3d65_display") {
        interop_id_ = "pq_p3d65_display";
      }
      else if (ELEM(alias, "lin_rec709_srgb", "lin_rec709")) {
        interop_id_ = "lin_rec709_scene";
      }
      else if (alias == "lin_rec2020") {
        interop_id_ = "lin_rec2020_scene";
      }
      else if (ELEM(alias, "lin_p3d65", "lin_displayp3")) {
        interop_id_ = "lin_p3d65_scene";
      }
      else if ((alias.startswith("lin_") || alias.startswith("srgb_") ||
                alias.startswith("g18_") || alias.startswith("g22_") || alias.startswith("g24_") ||
                alias.startswith("g26_") || alias.startswith("pq_") || alias.startswith("hlg_")) &&
               (alias.endswith("_scene") || alias.endswith("_display")))
      {
        interop_id_ = alias;
      }
    }
    is_primary_interop_id_ = !interop_id_.is_empty();
  }
  else {
    is_primary_interop_id_ = (interop_id_ == name());
    if (!is_primary_interop_id_) {
      const int num_aliases = ocio_color_space->getNumAliases();
      for (int i = 0; i < num_aliases; i++) {
        if (interop_id_ == ocio_color_space_->getAlias(i)) {
          is_primary_interop_id_ = true;
          break;
        }
      }
    }
  }

  /* Special case that we can not handle as an alias, because it's a role too. */
  if (interop_id_.is_empty()) {
    const char *data_name = ocio_config->getRoleColorSpace(OCIO_NAMESPACE::ROLE_DATA);
    if (data_name && STREQ(ocio_color_space->getName(), data_name)) {
      interop_id_ = "data";
    }
  }

  CLOG_TRACE(&LOG,
             "Add colorspace: %s (interop ID: %s)",
             name().c_str(),
             interop_id_.is_empty() ? "<none>" : interop_id_.c_str());
}

bool LibOCIOColorSpace::is_primary_interop_id() const
{
  return is_primary_interop_id_;
}

std::string LibOCIOColorSpace::icc_profile_path() const
{
#if OCIO_VERSION_HEX >= 0x02050000
  try {
    /* Both these methods can throw exceptions. */
    const char *profile_name = ocio_color_space_->getInterchangeAttribute("icc_profile_name");
    if (profile_name && profile_name[0]) {
      return ocio_config_->getCurrentContext()->resolveFileLocation(profile_name);
    }
    return profile_name;
  }
  catch (OCIO_NAMESPACE::Exception &exception) {
    report_exception(exception);
  }
#endif

  return "";
}

bool LibOCIOColorSpace::is_scene_linear() const
{
  /* The color space is the scene linear working space when the conversion is a no-op. */
  const CPUProcessor *cpu_processor = get_to_scene_linear_cpu_processor();
  return cpu_processor && cpu_processor->is_noop();
}

bool LibOCIOColorSpace::is_srgb() const
{
  /* Detected from the primary interop ID. For additional interop IDs it's not necessarily
   * sRGB, but rather a color space that can be saved as sRGB. */
  return is_primary_interop_id_ && ELEM(interop_id_, "srgb_rec709_scene", "srgb_rec709_display");
}

const CPUProcessor *LibOCIOColorSpace::get_to_scene_linear_cpu_processor() const
{
  return to_scene_linear_cpu_processor_.get([&]() -> std::unique_ptr<CPUProcessor> {
    OCIO_NAMESPACE::ConstProcessorRcPtr ocio_processor = create_ocio_processor(
        ocio_config_, ocio_color_space_->getName(), OCIO_NAMESPACE::ROLE_SCENE_LINEAR);
    if (!ocio_processor) {
      return nullptr;
    }
    return std::make_unique<LibOCIOCPUProcessor>(ocio_processor->getDefaultCPUProcessor());
  });
}

const CPUProcessor *LibOCIOColorSpace::get_from_scene_linear_cpu_processor() const
{
  return from_scene_linear_cpu_processor_.get([&]() -> std::unique_ptr<CPUProcessor> {
    OCIO_NAMESPACE::ConstProcessorRcPtr ocio_processor = create_ocio_processor(
        ocio_config_, OCIO_NAMESPACE::ROLE_SCENE_LINEAR, ocio_color_space_->getName());
    if (!ocio_processor) {
      return nullptr;
    }
    return std::make_unique<LibOCIOCPUProcessor>(ocio_processor->getDefaultCPUProcessor());
  });
}

void LibOCIOColorSpace::clear_caches()
{
  from_scene_linear_cpu_processor_ = CPUProcessorCache();
  to_scene_linear_cpu_processor_ = CPUProcessorCache();
}

}  // namespace ocio
}  // namespace blender
