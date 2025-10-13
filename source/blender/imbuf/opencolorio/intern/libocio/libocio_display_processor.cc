/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#if defined(WITH_OPENCOLORIO)

#  include <cfloat>
#  include <optional>
#  include <sstream>

#  include "BLI_colorspace.hh"
#  include "BLI_math_matrix.hh"

#  include "OCIO_config.hh"
#  include "OCIO_matrix.hh"
#  include "OCIO_view.hh"

#  include "error_handling.hh"
#  include "libocio_config.hh"
#  include "libocio_display.hh"
#  include "libocio_display_processor.hh"

#  include "../white_point.hh"

#  include "CLG_log.h"

static CLG_LogRef LOG = {"color_management"};

namespace blender::ocio {

static TransferFunction system_extended_srgb_transfer_function(const LibOCIOView *view,
                                                               const bool use_hdr_buffer)
{
#  ifdef __APPLE__
  /* The Metal backend always uses sRGB or extended sRGB buffer.
   *
   * How this will be decoded depends on the macOS display preset, but from testing
   * on a MacBook P3 M3 it appears:
   * - Apple XDR Display (P3 - 1600 nits): Decode with gamma 2.2
   * - HDR Video (P3-ST 2084): Decode with sRGB. As we encode with the sRGB transfer
   *   function, this will be cancelled out, and linear values will be passed on
   *   effectively unmodified.
   */
  UNUSED_VARS(use_hdr_buffer, view);
  return TransferFunction::sRGB;
#  elif defined(_WIN32)
  /* The Vulkan backend uses either sRGB for SDR, or linear extended sRGB for HDR.
   *
   * - Windows HDR mode off: use_hdr_buffer will be false, and we encode with sRGB.
   *   By default Windows will decode with gamma 2.2.
   * - Windows HDR mode on: use_hdr_buffer will be true, and we encode with sRGB.
   *   The Vulkan HDR swapchain blitting will decode with sRGB to cancel this out
   *   exactly, meaning we effectively pass on linear values unmodified.
   *
   * Note this means that both the user interface and SDR content will not be
   * displayed the same in HDR mode off and on. However it is consistent with other
   * software. To match, gamma 2.2 would have to be used.
   */
  UNUSED_VARS(use_hdr_buffer, view);
  return TransferFunction::sRGB;
#  else
  /* The Vulkan backend uses either sRGB for SDR, or linear extended sRGB for HDR.
   *
   * - When using a HDR swapchain and the display + view is HDR, ensure we pass on
   *   values linearly by doing gamma 2.2 encode here + gamma 2.2 decode in the
   *   Vulkan HDR swapchain blitting.
   * - When using HDR swapain and the display + view is SDR, use sRGB encode to
   *   emulate what happens on a typical SDR monitor.
   * - When using an SDR swapchain, the buffer is always sRGB.
   */
  return (use_hdr_buffer && view && view->is_hdr()) ? TransferFunction::Gamma22 :
                                                      TransferFunction::sRGB;
#  endif
}

static OCIO_NAMESPACE::TransformRcPtr create_extended_srgb_transform(
    const TransferFunction transfer_function)
{
  if (transfer_function == TransferFunction::sRGB) {
    /* Piecewise sRGB transfer function. */
    auto to_ui = OCIO_NAMESPACE::ExponentWithLinearTransform::Create();
    to_ui->setGamma({2.4, 2.4, 2.4, 1.0});
    to_ui->setOffset({0.055, 0.055, 0.055, 0.0});
    /* Mirrored for negative as specified by scRGB and extended sRGB. */
    to_ui->setNegativeStyle(OCIO_NAMESPACE::NEGATIVE_MIRROR);
    to_ui->setDirection(OCIO_NAMESPACE::TRANSFORM_DIR_INVERSE);
    return to_ui;
  }

  /* Pure gamma 2.2 function. */
  auto to_ui = OCIO_NAMESPACE::ExponentTransform::Create();
  to_ui->setValue({2.2, 2.2, 2.2, 1.0});
  /* Mirrored for negative as specified by scRGB and extended sRGB. */
  to_ui->setNegativeStyle(OCIO_NAMESPACE::NEGATIVE_MIRROR);
  to_ui->setDirection(OCIO_NAMESPACE::TRANSFORM_DIR_INVERSE);
  return to_ui;
}

static void adjust_for_hdr_image_file(const LibOCIOConfig &config,
                                      OCIO_NAMESPACE::GroupTransformRcPtr &group,
                                      StringRefNull display_name,
                                      StringRefNull view_name)
{
  /* Convert HDR PQ and HLG images from 100 nits to 203 nits convention. */
  const LibOCIODisplay *display = static_cast<const LibOCIODisplay *>(
      config.get_display_by_name(display_name));
  const LibOCIOView *view = (display) ? static_cast<const LibOCIOView *>(
                                            display->get_view_by_name(view_name)) :
                                        nullptr;
  const LibOCIOColorSpace *display_colorspace = static_cast<const LibOCIOColorSpace *>(
      view->display_colorspace());

  if (display_colorspace == nullptr || !display_colorspace->is_display_referred()) {
    return;
  }

  const ColorSpace *image_display_colorspace = config.get_color_space_for_hdr_image(
      display_colorspace->name());
  if (ELEM(image_display_colorspace, nullptr, display_colorspace)) {
    return;
  }

  auto to_display_linear = OCIO_NAMESPACE::ColorSpaceTransform::Create();
  to_display_linear->setSrc(display_colorspace->name().c_str());
  to_display_linear->setDst(image_display_colorspace->name().c_str());
  group->appendTransform(to_display_linear);
}

static void display_as_extended_srgb(const LibOCIOConfig &config,
                                     OCIO_NAMESPACE::GroupTransformRcPtr &group,
                                     StringRefNull display_name,
                                     StringRefNull view_name,
                                     const bool use_hdr_buffer)
{
  /* Emulate the user specified display on an extended sRGB display, conceptually:
   * - Apply the view and display transform
   * - Clamp colors to be within gamut
   * - Convert to cie_xyz_d65_interchange
   * - Convert to extended sRGB or gamma 2.2 scRGB
   *
   * When possible, we do equivalent but faster transforms. */

  /* TODO: Optimization: Often the view transform will already clamp. Maybe we can have a
   * few hardcoded checks for known view transforms? This helps eliminate a clamp and
   * in some cases a matrix multiplication. */

  const LibOCIODisplay *display = static_cast<const LibOCIODisplay *>(
      config.get_display_by_name(display_name));
  const LibOCIOView *view = (display) ? static_cast<const LibOCIOView *>(
                                            display->get_view_by_name(view_name)) :
                                        nullptr;
  if (view == nullptr) {
    CLOG_WARN(&LOG,
              "Unable to find display '%s' and view '%s', display may be incorrect",
              display_name.c_str(),
              view_name.c_str());
    return;
  }

  const TransferFunction target_transfer_function = system_extended_srgb_transfer_function(
      view, use_hdr_buffer);

  /* If we are already in the desired display colorspace, all we have to do is clamp. */
  if ((view->transfer_function() == target_transfer_function ||
       (view->transfer_function() == TransferFunction::ExtendedsRGB &&
        target_transfer_function == TransferFunction::sRGB)) &&
      view->gamut() == Gamut::Rec709)
  {
    auto clamp = OCIO_NAMESPACE::RangeTransform::Create();
    clamp->setStyle(OCIO_NAMESPACE::RANGE_CLAMP);
    clamp->setMinInValue(0.0);
    clamp->setMinOutValue(0.0);
    if (view->transfer_function() != TransferFunction::ExtendedsRGB) {
      clamp->setMaxInValue(1.0);
      clamp->setMaxOutValue(1.0);
    }
    group->appendTransform(clamp);
    return;
  }

  const LibOCIOColorSpace *lin_cie_xyz_d65 = static_cast<const LibOCIOColorSpace *>(
      config.get_color_space(OCIO_NAMESPACE::ROLE_INTERCHANGE_DISPLAY));
  const LibOCIOColorSpace *display_colorspace = static_cast<const LibOCIOColorSpace *>(
      view->display_colorspace());

  /* Verify if all conditions are met to do automatic display color management. */
  if (lin_cie_xyz_d65 == nullptr) {
    CLOG_DEBUG(&LOG,
               "Failed to find %s colorspace, disabling automatic display color management",
               OCIO_NAMESPACE::ROLE_INTERCHANGE_DISPLAY);
    return;
  }
  if (display_colorspace == nullptr) {
    CLOG_DEBUG(&LOG,
               "Failed to find display colorspace for view %s, disabling automatic display color "
               "management",
               view_name.c_str());
    return;
  }
  if (!display_colorspace->is_display_referred()) {
    CLOG_DEBUG(&LOG,
               "Color space %s is not a display color space, disabling automatic display color "
               "management",
               display_colorspace->name().c_str());
    return;
  }

  /* Find the matrix to convert to linear colorspace with gamut of the display colorspace. */
  std::optional<double4x4> xyz_to_display_gamut;
  switch (view->gamut()) {
    case Gamut::Rec709: {
      xyz_to_display_gamut = OCIO_XYZ_TO_REC709;
      break;
    }
    case Gamut::P3D65: {
      xyz_to_display_gamut = OCIO_XYZ_TO_P3;
      break;
    }
    case Gamut::Rec2020: {
      xyz_to_display_gamut = OCIO_XYZ_TO_REC2020;
      break;
    }
    case Gamut::Unknown: {
      break;
    }
  }

  if (xyz_to_display_gamut.has_value() && view->transfer_function() != TransferFunction::Unknown) {
    /* Optimized path for known gamut and transfer function. We want OpenColorIO to cancel out
     * out the transfer function of the chosen display, but this is not possible when clamping
     * happens in the middle of it.
     *
     * So here we transform to the linear colorspace with the gamut of the display colorspace,
     * and clamp there. This means there will be only matrix multiplications, or nothing at
     * all for Rec.709. */
    auto to_cie_xyz_d65 = OCIO_NAMESPACE::ColorSpaceTransform::Create();
    to_cie_xyz_d65->setSrc(display_colorspace->name().c_str());
    to_cie_xyz_d65->setDst(lin_cie_xyz_d65->name().c_str());
    group->appendTransform(to_cie_xyz_d65);

    auto to_lin_gamut = OCIO_NAMESPACE::MatrixTransform::Create();
    to_lin_gamut->setMatrix(math::transpose(xyz_to_display_gamut.value()).base_ptr());
    group->appendTransform(to_lin_gamut);

    /* Clamp colors to the chosen display colorspace, to emulate it on the actual display that
     * may have a wider gamut or HDR. */
    double clamp_max = 0.0;
    switch (view->transfer_function()) {
      case TransferFunction::sRGB:
      case TransferFunction::Gamma18:
      case TransferFunction::Gamma22:
      case TransferFunction::Gamma24:
      case TransferFunction::Gamma26:
        clamp_max = 1.0;
        break;
      case TransferFunction::PQ:
        clamp_max = 100.0; /* 10000 peak nits / 100 nits. */
        break;
      case TransferFunction::HLG:
        clamp_max = 10.0; /* 1000 peak nits / 100 nits. */
        break;
      case TransferFunction::ExtendedsRGB:
        clamp_max = DBL_MAX; /* Allow HDR > 1.0. */
        break;
      case TransferFunction::Unknown:
        break;
    }

    auto clamp = OCIO_NAMESPACE::RangeTransform::Create();
    clamp->setStyle(OCIO_NAMESPACE::RANGE_CLAMP);
    clamp->setMinInValue(0.0);
    clamp->setMinOutValue(0.0);
    if (clamp_max != DBL_MAX) {
      clamp->setMaxInValue(clamp_max);
      clamp->setMaxOutValue(clamp_max);
    }
    group->appendTransform(clamp);

    /* Transform to linear Rec.709. */
    if (view->gamut() != Gamut::Rec709) {
      auto to_rec709 = OCIO_NAMESPACE::MatrixTransform::Create();
      to_rec709->setMatrix(
          math::transpose(OCIO_XYZ_TO_REC709 * math::invert(xyz_to_display_gamut.value()))
              .base_ptr());
      group->appendTransform(to_rec709);
    }
  }
  else {
    /* Clamp colors to the chosen display colorspace, to emulate it on the actual display that
     * may have a wider gamut or HDR. Only do it for transfer functions where we know it's
     * correct, if unknown we hope the view transform already did it. */
    if (view->transfer_function() != TransferFunction::Unknown) {
      auto clamp = OCIO_NAMESPACE::RangeTransform::Create();
      clamp->setStyle(OCIO_NAMESPACE::RANGE_CLAMP);
      clamp->setMinInValue(0.0);
      clamp->setMinOutValue(0.0);
      if (view->transfer_function() != TransferFunction::ExtendedsRGB) {
        clamp->setMaxInValue(1.0);
        clamp->setMaxOutValue(1.0);
      }
      group->appendTransform(clamp);
    }

    /* Convert from display colorspace to linear Rec.709. */
    auto to_cie_xyz_d65 = OCIO_NAMESPACE::ColorSpaceTransform::Create();
    to_cie_xyz_d65->setSrc(display_colorspace->name().c_str());
    to_cie_xyz_d65->setDst(lin_cie_xyz_d65->name().c_str());
    group->appendTransform(to_cie_xyz_d65);

    auto to_rec709 = OCIO_NAMESPACE::MatrixTransform::Create();
    to_rec709->setMatrix(math::transpose(OCIO_XYZ_TO_REC709).base_ptr());
    group->appendTransform(to_rec709);
  }

  group->appendTransform(create_extended_srgb_transform(target_transfer_function));
}

OCIO_NAMESPACE::TransformRcPtr create_ocio_display_transform(
    const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
    StringRefNull display,
    StringRefNull view,
    StringRefNull look,
    StringRefNull from_colorspace)
{
  OCIO_NAMESPACE::GroupTransformRcPtr group = OCIO_NAMESPACE::GroupTransform::Create();

  /* Add look transform. */
  bool use_look = (look != nullptr && look[0] != '\0' && look != "None");
  if (use_look) {
    const char *look_output = nullptr;

    try {
      look_output = OCIO_NAMESPACE::LookTransform::GetLooksResultColorSpace(
          ocio_config, ocio_config->getCurrentContext(), look.c_str());
    }
    catch (OCIO_NAMESPACE::Exception &exception) {
      report_exception(exception);
      return nullptr;
    }

    if (look_output != nullptr && look_output[0] != 0) {
      OCIO_NAMESPACE::LookTransformRcPtr lt = OCIO_NAMESPACE::LookTransform::Create();
      lt->setSrc(from_colorspace.c_str());
      lt->setDst(look_output);
      lt->setLooks(look.c_str());
      group->appendTransform(lt);

      /* Make further transforms aware of the color space change. */
      from_colorspace = look_output;
    }
    else {
      /* For empty looks, no output color space is returned. */
      use_look = false;
    }
  }

  /* Add view and display transform. */
  OCIO_NAMESPACE::DisplayViewTransformRcPtr dvt = OCIO_NAMESPACE::DisplayViewTransform::Create();
  dvt->setSrc(from_colorspace.c_str());
  dvt->setLooksBypass(use_look);
  dvt->setView(view.c_str());
  dvt->setDisplay(display.c_str());
  group->appendTransform(dvt);

  return group;
}

static OCIO_NAMESPACE::TransformRcPtr create_untonemapped_ocio_display_transform(
    const LibOCIOConfig &config,
    StringRefNull display_name,
    StringRefNull from_colorspace,
    bool use_hdr_buffer)
{
  /* Convert to extended sRGB without any tone mapping. */
  const auto group = OCIO_NAMESPACE::GroupTransform::Create();

  const auto to_scene_linear = OCIO_NAMESPACE::ColorSpaceTransform::Create();
  to_scene_linear->setSrc(from_colorspace.c_str());
  to_scene_linear->setDst(OCIO_NAMESPACE::ROLE_SCENE_LINEAR);
  group->appendTransform(to_scene_linear);

  const auto to_rec709 = OCIO_NAMESPACE::MatrixTransform::Create();
  to_rec709->setMatrix(math::transpose(double4x4(colorspace::scene_linear_to_rec709)).base_ptr());
  group->appendTransform(to_rec709);

  const LibOCIODisplay *display = static_cast<const LibOCIODisplay *>(
      config.get_display_by_name(display_name));
  const LibOCIOView *view = (display) ? static_cast<const LibOCIOView *>(
                                            display->get_untonemapped_view()) :
                                        nullptr;
  group->appendTransform(create_extended_srgb_transform(
      system_extended_srgb_transfer_function(view, use_hdr_buffer)));
  return group;
}

OCIO_NAMESPACE::ConstProcessorRcPtr create_ocio_display_processor(
    const LibOCIOConfig &config, const DisplayParameters &display_parameters)
{
  using namespace OCIO_NAMESPACE;

  const ConstConfigRcPtr &ocio_config = config.get_ocio_config();

  GroupTransformRcPtr group = GroupTransform::Create();

  const char *from_colorspace = display_parameters.from_colorspace.c_str();

  /* Linear transforms. */
  if (display_parameters.scale != 1.0f || display_parameters.use_white_balance) {
    /* Always apply exposure and/or white balance in scene linear. */
    ColorSpaceTransformRcPtr ct = ColorSpaceTransform::Create();
    ct->setSrc(from_colorspace);
    ct->setDst(ROLE_SCENE_LINEAR);
    group->appendTransform(ct);

    /* Make further transforms aware of the color space change. */
    from_colorspace = ROLE_SCENE_LINEAR;

    /* Apply scale. */
    MatrixTransformRcPtr mt = MatrixTransform::Create();
    float3x3 matrix = float3x3::identity() * display_parameters.scale;

    /* Apply white balance. */
    if (display_parameters.use_white_balance) {
      matrix *= calculate_white_point_matrix(
          config, display_parameters.temperature, display_parameters.tint);
    }

    mt->setMatrix(double4x4(math::transpose(matrix)).base_ptr());
    group->appendTransform(mt);
  }

  if (!display_parameters.view.is_empty()) {
    /* Core display processor. */
    group->appendTransform(create_ocio_display_transform(ocio_config,
                                                         display_parameters.display,
                                                         display_parameters.view,
                                                         display_parameters.look,
                                                         from_colorspace));
    /* Gamma. */
    if (display_parameters.exponent != 1.0f) {
      ExponentTransformRcPtr et = ExponentTransform::Create();
      const double value[4] = {display_parameters.exponent,
                               display_parameters.exponent,
                               display_parameters.exponent,
                               1.0};
      et->setValue(value);
      group->appendTransform(et);
    }

    if (display_parameters.is_image_output) {
      adjust_for_hdr_image_file(
          config, group, display_parameters.display, display_parameters.view);
    }

    /* Convert to extended sRGB to match the system graphics buffer. */
    if (display_parameters.use_display_emulation) {
      display_as_extended_srgb(config,
                               group,
                               display_parameters.display,
                               display_parameters.view,
                               display_parameters.use_hdr_buffer);
    }
  }
  else {
    /* Untonemapped case, directly to extended sRGB. */
    group->appendTransform(create_untonemapped_ocio_display_transform(
        config, display_parameters.display, from_colorspace, display_parameters.use_hdr_buffer));
  }

  if (display_parameters.inverse) {
    group->setDirection(TRANSFORM_DIR_INVERSE);
  }

  if (CLOG_CHECK(&LOG, CLG_LEVEL_TRACE)) {
    std::stringstream sstream;
    sstream << *group;
    CLOG_TRACE(&LOG, "Creating display transform:\n%s", sstream.str().c_str());
  }

  /* Create processor from transform. This is the moment were OCIO validates the entire transform,
   * no need to check for the validity of inputs above. */
  ConstProcessorRcPtr p;
  try {
    p = ocio_config->getProcessor(group);
  }
  catch (Exception &exception) {
    report_exception(exception);
    return nullptr;
  }

  return p;
}

}  // namespace blender::ocio

#endif
