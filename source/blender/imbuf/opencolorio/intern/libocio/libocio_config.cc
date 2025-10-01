/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_config.hh"

#if defined(WITH_OPENCOLORIO)

#  include <algorithm>
#  include <numeric>

#  include <fmt/format.h>

#  include "BLI_array.hh"
#  include "BLI_assert.h"
#  include "BLI_index_range.hh"
#  include "BLI_math_matrix.hh"

#  include "OCIO_matrix.hh"
#  include "OCIO_role_names.hh"

#  include "error_handling.hh"
#  include "libocio_colorspace.hh"
#  include "libocio_cpu_processor.hh"
#  include "libocio_display_processor.hh"
#  include "libocio_processor.hh"

namespace blender::ocio {

/* -------------------------------------------------------------------- */
/** \name Construction
 * \{ */

std::unique_ptr<Config> LibOCIOConfig::create_from_environment()
{
  try {
    OCIO_NAMESPACE::ConstConfigRcPtr ocio_config = OCIO_NAMESPACE::Config::CreateFromEnv();
    if (!ocio_config) {
      return nullptr;
    }

    return std::unique_ptr<LibOCIOConfig>(new LibOCIOConfig(ocio_config));
  }
  catch (OCIO_NAMESPACE::Exception &exception) {
    report_exception(exception);
  }

  return nullptr;
}

std::unique_ptr<Config> LibOCIOConfig::create_from_file(const StringRefNull filename)
{
  try {
    OCIO_NAMESPACE::ConstConfigRcPtr ocio_config = OCIO_NAMESPACE::Config::CreateFromFile(
        filename.c_str());
    if (!ocio_config) {
      return nullptr;
    }

    return std::unique_ptr<LibOCIOConfig>(new LibOCIOConfig(ocio_config));
  }
  catch (OCIO_NAMESPACE::Exception &exception) {
    report_exception(exception);
  }

  return nullptr;
}

LibOCIOConfig::LibOCIOConfig(const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config)
{
  BLI_assert(ocio_config);

  /* Set the global OpenColorIO configuration so that other parts of Blender can access it. For
   * example, Cycles uses for own color management of textures.
   *
   * Acquire a pointer to the configuration and pass it around explicitly to avoid unneeded shared
   * pointer acquisition. */
  OCIO_NAMESPACE::SetCurrentConfig(ocio_config);
  ocio_config_ = OCIO_NAMESPACE::GetCurrentConfig();

  initialize_active_color_spaces();
  initialize_inactive_color_spaces();
  initialize_hdr_color_spaces();
  initialize_looks();
  initialize_displays();
}

LibOCIOConfig::~LibOCIOConfig() {}

void LibOCIOConfig::initialize_active_color_spaces()
{
  OCIO_NAMESPACE::ColorSpaceSetRcPtr ocio_color_spaces;

  try {
    ocio_color_spaces = ocio_config_->getColorSpaces(nullptr);
  }
  catch (OCIO_NAMESPACE::Exception &exception) {
    report_exception(exception);
    return;
  }

  if (!ocio_color_spaces) {
    report_error("Invalid OpenColorIO configuration: color spaces set is nullptr");
    return;
  }

  const int num_color_spaces = ocio_color_spaces->getNumColorSpaces();
  if (num_color_spaces < 0) {
    report_error(fmt::format(
        "Invalid OpenColorIO configuration: invalid number of color spaces {}", num_color_spaces));
    return;
  }

  color_spaces_.reserve(num_color_spaces);

  for (const int i : IndexRange(num_color_spaces)) {
    const OCIO_NAMESPACE::ConstColorSpaceRcPtr ocio_color_space =
        ocio_color_spaces->getColorSpaceByIndex(i);
    color_spaces_.append_as(i, ocio_config_, ocio_color_space);
  }

  /* Create index array for access to the color space in alphabetic order. */
  sorted_color_space_index_.resize(num_color_spaces);
  std::iota(sorted_color_space_index_.begin(), sorted_color_space_index_.end(), 0);
  std::sort(sorted_color_space_index_.begin(), sorted_color_space_index_.end(), [&](int a, int b) {
    return color_spaces_[a].name() < color_spaces_[b].name();
  });
}

void LibOCIOConfig::initialize_inactive_color_spaces()
{
  const int num_inactive_color_spaces = ocio_config_->getNumColorSpaces(
      OCIO_NAMESPACE::SEARCH_REFERENCE_SPACE_ALL, OCIO_NAMESPACE::COLORSPACE_INACTIVE);
  if (num_inactive_color_spaces < 0) {
    report_error(fmt::format(
        "Invalid OpenColorIO configuration: invalid number of inactive color spaces {}",
        num_inactive_color_spaces));
    return;
  }

  for (const int i : IndexRange(num_inactive_color_spaces)) {
    const char *colorspace_name = ocio_config_->getColorSpaceNameByIndex(
        OCIO_NAMESPACE::SEARCH_REFERENCE_SPACE_ALL, OCIO_NAMESPACE::COLORSPACE_INACTIVE, i);

    OCIO_NAMESPACE::ConstColorSpaceRcPtr ocio_color_space;
    try {
      ocio_color_space = ocio_config_->getColorSpace(colorspace_name);
    }
    catch (OCIO_NAMESPACE::Exception &exception) {
      report_exception(exception);
      continue;
    }

    inactive_color_spaces_.append_as(i, ocio_config_, ocio_color_space);
  }
}

void LibOCIOConfig::initialize_looks()
{
  const int num_looks = ocio_config_->getNumLooks();

  looks_.reserve(num_looks + 1);

  /* Add entry for look None. */
  looks_.append_as(0, nullptr);

  for (const int i : IndexRange(num_looks)) {
    const StringRefNull view_name = ocio_config_->getLookNameByIndex(i);

    /* Look None is built-in and always exists. Skip it from the configuration. */
    if (view_name == "None") {
      continue;
    }

    const OCIO_NAMESPACE::ConstLookRcPtr ocio_look = ocio_config_->getLook(view_name.c_str());
    looks_.append_as(i + 1, ocio_look);
  }
}

void LibOCIOConfig::initialize_displays()
{
  const int num_displays = ocio_config_->getNumDisplays();
  if (num_displays < 0) {
    report_error(fmt::format("Invalid OpenColorIO configuration: invalid number of displays {}",
                             num_displays));
    return;
  }

  displays_.reserve(num_displays);

  for (const int i : IndexRange(num_displays)) {
    displays_.append_as(i, *this);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color space information
 * \{ */

float3 LibOCIOConfig::get_default_luma_coefs() const
{
  try {
    double rgb_double[3];
    ocio_config_->getDefaultLumaCoefs(rgb_double);

    return float3(rgb_double[0], rgb_double[1], rgb_double[2]);
  }
  catch (OCIO_NAMESPACE::Exception &exception) {
    report_exception(exception);
  }

  /* Fallback to the older Blender assumed primaries of ITU-BT.709 / sRGB, matching the
   * coefficients used in the fallback implementation. */
  return float3(0.2126f, 0.7152f, 0.0722f);
}

static bool to_scene_linear_matrix(const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
                                   const StringRefNull colorspace,
                                   float3x3 &to_scene_linear)
{
  const OCIO_NAMESPACE::ConstProcessorRcPtr processor = create_ocio_processor(
      ocio_config, colorspace.c_str(), OCIO_NAMESPACE::ROLE_SCENE_LINEAR);
  if (!processor) {
    return false;
  }

  const OCIO_NAMESPACE::ConstCPUProcessorRcPtr cpu_processor = processor->getDefaultCPUProcessor();
  to_scene_linear = float3x3::identity();
  cpu_processor->applyRGB(to_scene_linear[0]);
  cpu_processor->applyRGB(to_scene_linear[1]);
  cpu_processor->applyRGB(to_scene_linear[2]);

  return true;
}

float3x3 LibOCIOConfig::get_xyz_to_scene_linear_matrix() const
{
  /* Default to ITU-BT.709 in case no appropriate transform found.
   * Note XYZ is defined here as having a D65 white point. */
  float3x3 xyz_to_scene_linear = XYZ_TO_REC709;

  /* Get from OpenColorO config if it has the required roles. */
  if (!ocio_config_->hasRole(OCIO_NAMESPACE::ROLE_SCENE_LINEAR)) {
    return xyz_to_scene_linear;
  }

  if (ocio_config_->hasRole("aces_interchange")) {
    /* Standard OpenColorIO role, defined as ACES AP0 (ACES2065-1). */
    float3x3 aces_to_scene_linear;
    if (to_scene_linear_matrix(ocio_config_, "aces_interchange", aces_to_scene_linear)) {
      float3x3 xyz_to_aces = math::invert(ACES_TO_XYZ);
      xyz_to_scene_linear = aces_to_scene_linear * xyz_to_aces;
    }
  }
  else if (ocio_config_->hasRole("XYZ")) {
    /* Custom role used before the standard existed. */
    to_scene_linear_matrix(ocio_config_, "XYZ", xyz_to_scene_linear);
  }

  return xyz_to_scene_linear;
}

const char *LibOCIOConfig::get_color_space_from_filepath(const char *filepath) const
{
  /* Ignore the default rule, same behavior as for example OpenImageIO and xStudio.
   * The ACES studio config has only a default rule set to ACES2065-1, which works
   * poorly if we assign it to every file as default.
   *
   * It's unclear if the default rule should be used for anything, and if not why
   * it even exists. */
  if (ocio_config_->filepathOnlyMatchesDefaultRule(filepath)) {
    return nullptr;
  }

  return ocio_config_->getColorSpaceFromFilepath(filepath);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color space API
 * \{ */

const ColorSpace *LibOCIOConfig::get_color_space(const StringRefNull name) const
{
  OCIO_NAMESPACE::ConstColorSpaceRcPtr ocio_color_space;

  try {
    /* Lookup color space in the OpenColorIO, letting it resolve role name or an alias. */
    ocio_color_space = ocio_config_->getColorSpace(name.c_str());
  }
  catch (OCIO_NAMESPACE::Exception &exception) {
    report_exception(exception);
    return nullptr;
  }

  if (!ocio_color_space) {
    return nullptr;
  }

  /* TODO(sergey): Is there faster way to lookup Blender-side color space?
   * It does not seem that pointer in ConstColorSpaceRcPtr is unique enough to use for
   * comparison. */
  for (const LibOCIOColorSpace &color_space : color_spaces_) {
    if (color_space.name() == ocio_color_space->getName()) {
      return &color_space;
    }
  }

  /* Also lookup in the inactive color space, as the requested space might be coming from the
   * display and marked as inactive to prevent it from showing up in the application menu. */
  for (const LibOCIOColorSpace &color_space : inactive_color_spaces_) {
    if (color_space.name() == ocio_color_space->getName()) {
      return &color_space;
    }
  }

  if (!ocio_config_->isInactiveColorSpace(ocio_color_space->getName())) {
    report_error(
        fmt::format("Invalid OpenColorIO configuration: color space {} not found on Blender side",
                    ocio_color_space->getName()));
  }

  return nullptr;
}

int LibOCIOConfig::get_num_color_spaces() const
{
  return color_spaces_.size();
}

const ColorSpace *LibOCIOConfig::get_color_space_by_index(int const index) const
{
  if (index < 0 || index >= color_spaces_.size()) {
    return nullptr;
  }
  return &color_spaces_[index];
}

const ColorSpace *LibOCIOConfig::get_sorted_color_space_by_index(const int index) const
{
  BLI_assert(color_spaces_.size() == sorted_color_space_index_.size());
  if (index < 0 || index >= color_spaces_.size()) {
    return nullptr;
  }
  return get_color_space_by_index(sorted_color_space_index_[index]);
}

const ColorSpace *LibOCIOConfig::get_color_space_by_interop_id(StringRefNull interop_id) const
{
  for (const LibOCIOColorSpace &color_space : color_spaces_) {
    if (color_space.interop_id() == interop_id) {
      return &color_space;
    }
  }

  for (const LibOCIOColorSpace &color_space : inactive_color_spaces_) {
    if (color_space.interop_id() == interop_id) {
      return &color_space;
    }
  }

  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name HDR image API
 * \{ */

const ColorSpace *LibOCIOConfig::get_color_space_for_hdr_image(StringRefNull name) const
{
  /* Based on emperical testing,  ideo works with 100 nits diffuse white, while
   * images need 203 nits diffuse whites to show matching results. */
  const ColorSpace *colorspece = get_color_space(name);
  if (colorspece->interop_id() == "pq_rec2020_display") {
    return get_color_space("blender:pq_rec2020_display_203nits");
  }
  if (colorspece->interop_id() == "hlg_rec2020_display") {
    return get_color_space("blender:hlg_rec2020_display_203nits");
  }
  return nullptr;
}

void LibOCIOConfig::initialize_hdr_color_spaces()
{
  for (StringRefNull interop_id : {"pq_rec2020_display", "hlg_rec2020_display"}) {
    const auto *colorspace = static_cast<const LibOCIOColorSpace *>(
        get_color_space_by_interop_id(interop_id));
    if (!colorspace || !colorspace->is_display_referred()) {
      continue;
    }

    /* Create colorspace that uses 203 nits diffuse white instead of 100 nits. */
    const auto hdr_100_colorspace = ocio_config_->getColorSpace(colorspace->name().c_str());
    const auto hdr_colorspace = OCIO_NAMESPACE::ColorSpace::Create(
        OCIO_NAMESPACE::REFERENCE_SPACE_DISPLAY);
    const auto group = OCIO_NAMESPACE::GroupTransform::Create();

    hdr_colorspace->setName(("blender:" + interop_id + "_203nits").c_str());

    const auto to_203_nits = OCIO_NAMESPACE::MatrixTransform::Create();
    to_203_nits->setMatrix(double4x4(double3x3::diagonal(203.0 / 100.0)).base_ptr());
    group->appendTransform(to_203_nits);

    const auto to_display = hdr_100_colorspace
                                ->getTransform(OCIO_NAMESPACE::COLORSPACE_DIR_FROM_REFERENCE)
                                ->createEditableCopy();
    group->appendTransform(to_display);

    hdr_colorspace->setTransform(group, OCIO_NAMESPACE::COLORSPACE_DIR_FROM_REFERENCE);

    OCIO_NAMESPACE::Config *mutable_ocio_config = const_cast<OCIO_NAMESPACE::Config *>(
        ocio_config_.get());
    mutable_ocio_config->addColorSpace(hdr_colorspace);

    inactive_color_spaces_.append_as(inactive_color_spaces_.size(), ocio_config_, hdr_colorspace);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Working space API
 * \{ */

void LibOCIOConfig::set_scene_linear_role(StringRefNull name)
{
  if (ocio_config_->getRoleColorSpace(OCIO_NAMESPACE::ROLE_SCENE_LINEAR) == name) {
    return;
  }

  /* This is a bad const cast, but seems to work ok, and reloading the whole config is
   * something we don't support yet. When we do this could be changed. */
  OCIO_NAMESPACE::Config *mutable_ocio_config = const_cast<OCIO_NAMESPACE::Config *>(
      ocio_config_.get());
  mutable_ocio_config->setRole(OCIO_NAMESPACE::ROLE_SCENE_LINEAR, name.c_str());

  for (LibOCIOColorSpace &color_space : color_spaces_) {
    color_space.clear_caches();
  }
  for (LibOCIOColorSpace &color_space : inactive_color_spaces_) {
    color_space.clear_caches();
  }
  for (LibOCIODisplay &display : displays_) {
    display.clear_caches();
  }
  gpu_shader_binder_.clear_caches();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Display API
 * \{ */

const Display *LibOCIOConfig::get_default_display() const
{
  if (displays_.is_empty()) {
    return nullptr;
  }
  /* Matches the behavior of OpenColorIO, but avoids using API which potentially throws exception
   * and requires string lookups. */
  return &displays_[0];
}

const Display *LibOCIOConfig::get_display_by_name(const StringRefNull name) const
{
  /* TODO(@sergey): Is there faster way to lookup Blender-side display? */
  for (const LibOCIODisplay &display : displays_) {
    if (display.name() == name) {
      return &display;
    }
  }
  return nullptr;
}

int LibOCIOConfig::get_num_displays() const
{
  return displays_.size();
}

const Display *LibOCIOConfig::get_display_by_index(int index) const
{
  if (index < 0 || index >= displays_.size()) {
    return nullptr;
  }
  return &displays_[index];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Display colorspace API
 * \{ */

const ColorSpace *LibOCIOConfig::get_display_view_color_space(const StringRefNull display,
                                                              const StringRefNull view) const
{
  StringRefNull display_color_space;

  try {
    display_color_space = ocio_config_->getDisplayViewColorSpaceName(display.c_str(),
                                                                     view.c_str());
    /* OpenColorIO does not resolve this token for us, so do it ourselves. */
    if (strcasecmp(display_color_space.c_str(), "<USE_DISPLAY_NAME>") == 0) {
      display_color_space = display;
    }
  }
  catch (OCIO_NAMESPACE::Exception &exception) {
    report_exception(exception);
    display_color_space = display;
  }

  return get_color_space(display_color_space);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Look API
 * \{ */

const Look *LibOCIOConfig::get_look_by_name(const StringRefNull name) const
{
  /* TODO(sergey): Is there faster way to lookup Blender-side look? */
  for (const LibOCIOLook &look : looks_) {
    if (look.name() == name) {
      return &look;
    }
  }
  return nullptr;
}

int LibOCIOConfig::get_num_looks() const
{
  return looks_.size();
}

const Look *LibOCIOConfig::get_look_by_index(const int index) const
{
  if (index < 0 || index >= looks_.size()) {
    return nullptr;
  }
  return &looks_[index];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Processor API
 * \{ */

std::shared_ptr<const CPUProcessor> LibOCIOConfig::get_display_cpu_processor(
    const DisplayParameters &display_parameters) const
{
  OCIO_NAMESPACE::ConstProcessorRcPtr processor = create_ocio_display_processor(
      *this, display_parameters);
  if (!processor) {
    return nullptr;
  }
  return std::make_shared<LibOCIOCPUProcessor>(processor->getDefaultCPUProcessor());
}

std::shared_ptr<const CPUProcessor> LibOCIOConfig::get_cpu_processor(
    const StringRefNull from_colorspace, const StringRefNull to_colorspace) const
{
  const OCIO_NAMESPACE::ConstProcessorRcPtr processor = create_ocio_processor(
      ocio_config_, from_colorspace.c_str(), to_colorspace.c_str());
  if (!processor) {
    return nullptr;
  }
  return std::make_shared<LibOCIOCPUProcessor>(processor->getDefaultCPUProcessor());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Processor API
 * \{ */

const GPUShaderBinder &LibOCIOConfig::get_gpu_shader_binder() const
{
  return gpu_shader_binder_;
}

/** \} */

}  // namespace blender::ocio

#endif
