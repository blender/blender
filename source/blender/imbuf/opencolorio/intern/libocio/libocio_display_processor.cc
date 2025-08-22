/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_display_processor.hh"

#if defined(WITH_OPENCOLORIO)

#  include <cfloat>
#  include <sstream>

#  include "BLI_math_matrix.hh"

#  include "OCIO_config.hh"
#  include "OCIO_view.hh"

#  include "error_handling.hh"
#  include "libocio_config.hh"
#  include "libocio_display.hh"

#  include "../white_point.hh"

#  include "CLG_log.h"

static CLG_LogRef LOG = {"color_management"};

namespace blender::ocio {

static void display_as_extended_srgb(const LibOCIOConfig &config,
                                     OCIO_NAMESPACE::GroupTransformRcPtr &group,
                                     StringRefNull display_name,
                                     StringRefNull view_name,
                                     const bool use_hdr_buffer)
{
  /* Emulate the user specified display on an extended sRGB display, conceptually:
   * - Apply the view and display transform
   * - Clamp colors to be within gamut
   * - Apply the inverse untonemapped display transform
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
  const TransferFunction target_transfer_function = TransferFunction::sRGB;
  UNUSED_VARS(use_hdr_buffer);
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
  const TransferFunction target_transfer_function = TransferFunction::sRGB;
  UNUSED_VARS(use_hdr_buffer);
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
  const TransferFunction target_transfer_function = (use_hdr_buffer && view->is_hdr()) ?
                                                        TransferFunction::Gamma22 :
                                                        TransferFunction::sRGB;
#  endif

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

  /* Find linear Rec.709 colorspace, needed for the sRGB gamut. */
  const ColorSpace *lin_rec709 = config.get_color_space_by_interop_id("lin_rec709_scene");
  if (lin_rec709 == nullptr) {
    CLOG_WARN(&LOG, "Failed to find lin_rec709_scene colorspace, display may be incorrect");
    return;
  }

  /* Find the untonemapped view.
   * TODO: It would be better to do this based on cie_xyz_d65_interchange for configs
   * that support it. */
  const View *untonemapped_view = display->get_untonemapped_view();
  if (untonemapped_view == nullptr) {
    CLOG_WARN(&LOG,
              "Failed to find untonemapped view for \"%s\", display may be incorrect",
              display->name().c_str());
    return;
  }

  const ColorSpace *display_colorspace = config.get_display_view_color_space(
      display->name().c_str(), untonemapped_view->name().c_str());

  /* Find the linear colorspace with the gamut of the display colorspace. */
  const ColorSpace *lin_gamut = nullptr;
  switch (view->gamut()) {
    case Gamut::Rec709: {
      lin_gamut = config.get_color_space_by_interop_id("lin_rec709_scene");
      break;
    }
    case Gamut::P3D65: {
      lin_gamut = config.get_color_space_by_interop_id("lin_p3d65_scene");
      break;
    }
    case Gamut::Rec2020: {
      lin_gamut = config.get_color_space_by_interop_id("lin_rec2020_scene");
      break;
    }
    case Gamut::Unknown: {
      break;
    }
  }

  if (lin_gamut && view->transfer_function() != TransferFunction::Unknown) {
    /* Optimized path for known gamut and transfer function. We want OpenColorIO to cancel out
     * out the transfer function of the chosen display, but this is not possible when clamping
     * happens in the middle of it.
     *
     * So here we transform to the linear colorspace with the gamut of the display colorspace,
     * and clamp there. This means there will be only matrix multiplications, or nothing at
     * all for Rec.709. */
    auto to_lin_gamut = OCIO_NAMESPACE::ColorSpaceTransform::Create();
    to_lin_gamut->setSrc(display_colorspace->name().c_str());
    to_lin_gamut->setDst(lin_gamut->name().c_str());
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
    if (lin_gamut != lin_rec709) {
      auto to_rec709 = OCIO_NAMESPACE::ColorSpaceTransform::Create();
      to_rec709->setSrc(lin_gamut->name().c_str());
      to_rec709->setDst(lin_rec709->name().c_str());
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
    auto to_rec709 = OCIO_NAMESPACE::ColorSpaceTransform::Create();
    to_rec709->setSrc(display_colorspace->name().c_str());
    to_rec709->setDst(lin_rec709->name().c_str());
    group->appendTransform(to_rec709);
  }

  if (target_transfer_function == TransferFunction::sRGB) {
    /* Piecewise sRGB transfer function. */
    auto to_ui = OCIO_NAMESPACE::ExponentWithLinearTransform::Create();
    to_ui->setGamma({2.4, 2.4, 2.4, 1.0});
    to_ui->setOffset({0.055, 0.055, 0.055, 0.0});
    /* Mirrored for negative as specified by scRGB and extended sRGB. */
    to_ui->setNegativeStyle(OCIO_NAMESPACE::NEGATIVE_MIRROR);
    to_ui->setDirection(OCIO_NAMESPACE::TRANSFORM_DIR_INVERSE);
    group->appendTransform(to_ui);
  }
  else {
    /* Pure gamma 2.2 function. */
    auto to_ui = OCIO_NAMESPACE::ExponentTransform::Create();
    to_ui->setValue({2.2, 2.2, 2.2, 1.0});
    /* Mirrored for negative as specified by scRGB and extended sRGB. */
    to_ui->setNegativeStyle(OCIO_NAMESPACE::NEGATIVE_MIRROR);
    to_ui->setDirection(OCIO_NAMESPACE::TRANSFORM_DIR_INVERSE);
    group->appendTransform(to_ui);
  }
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

  /* Add look transform. */
  bool use_look = (display_parameters.look != nullptr && display_parameters.look[0] != '\0');
  if (use_look) {
    const char *look_output = nullptr;

    try {
      look_output = LookTransform::GetLooksResultColorSpace(
          ocio_config, ocio_config->getCurrentContext(), display_parameters.look.c_str());
    }
    catch (Exception &exception) {
      report_exception(exception);
      return nullptr;
    }

    if (look_output != nullptr && look_output[0] != 0) {
      LookTransformRcPtr lt = LookTransform::Create();
      lt->setSrc(from_colorspace);
      lt->setDst(look_output);
      lt->setLooks(display_parameters.look.c_str());
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
  DisplayViewTransformRcPtr dvt = DisplayViewTransform::Create();
  dvt->setSrc(from_colorspace);
  dvt->setLooksBypass(use_look);
  dvt->setView(display_parameters.view.c_str());
  dvt->setDisplay(display_parameters.display.c_str());
  group->appendTransform(dvt);

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

  if (display_parameters.use_display_emulation) {
    display_as_extended_srgb(config,
                             group,
                             display_parameters.display,
                             display_parameters.view,
                             display_parameters.use_hdr_buffer);
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
