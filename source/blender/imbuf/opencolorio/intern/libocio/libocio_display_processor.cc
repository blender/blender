/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_display_processor.hh"

#if defined(WITH_OPENCOLORIO)

#  include "BLI_math_matrix.hh"

#  include "OCIO_config.hh"

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
                                     StringRefNull view_name)
{
  /* Emulate the user specified display on an extended sRGB display:
   * - Apply the view and display transform
   * - Clamp colors to be within gamut
   * - Apply the inverse untonemapped display transform
   * - Convert to extended sRGB
   */

  /* TODO: This is quite inefficient, and may be possible to optimize.
   *
   * Ideally we want OpenColor to cancel out the last part of the forward display transform
   * and inverse untonemapped display transform. By itself this seems to generally work for
   * the Blender and ACES 2.0 configs.
   *
   * However the clamping prevents this. For common display color spaces it should be possible
   * to clamp the gamut and HDR in a linear space. For example for PQ, we'd need to clamp
   * to Rec.2020 gamut and (10000 nits max / 100 nits reference white) = 100.0. Probably this
   * can be done with a matrix transform, clamp and inverse matrix transform. Where hopefully
   * the matrix transforms can be merged with a preceding or following one. */

  const LibOCIODisplay *display = static_cast<const LibOCIODisplay *>(
      config.get_display_by_name(display_name));
  const LibOCIOView *view = (display) ? static_cast<const LibOCIOView *>(
                                            display->get_view_by_name(view_name)) :
                                        nullptr;

  /* Clamp to gamut, so that we don't display values outside of it. One exception
   * is extended sRGB, where we need to preserve them for correct results and assume
   * the view transform already clamped them. */
  if (!(view && view->is_extended())) {
    auto clamp = OCIO_NAMESPACE::RangeTransform::Create();
    clamp->setStyle(OCIO_NAMESPACE::RANGE_CLAMP);
    clamp->setMinInValue(0.0);
    clamp->setMinOutValue(0.0);
    clamp->setMaxInValue(1.0);
    clamp->setMaxOutValue(1.0);
    group->appendTransform(clamp);
  }

  /* Nothing further to do if already using sRGB. */
  if (view && view->is_srgb()) {
    return;
  }

  /* Find the linear Rec.709 colorspace needed for the extended sRGB transform. */
  const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config = config.get_ocio_config();
  const char *lin_rec709_srgb = ocio_config->getCanonicalName("lin_rec709_srgb");
  if (lin_rec709_srgb == nullptr || lin_rec709_srgb[0] == '\0') {
    CLOG_INFO(&LOG, "Failed to find lin_rec709_srgb colorspace, display may be incorrect");
    return;
  }

  /* Find the display and its untonemapped view. */
  const View *untonemapped_view = display->get_untonemapped_view();
  if (untonemapped_view == nullptr) {
    CLOG_INFO(&LOG,
              "Failed to find untonemapped view for \"%s\", display may be incorrect",
              display->name().c_str());
    return;
  }

  /* Convert to extended sRGB. */
  const ColorSpace *display_colorspace = config.get_display_view_color_space(
      display->name().c_str(), untonemapped_view->name().c_str());

  auto to_rec709 = OCIO_NAMESPACE::ColorSpaceTransform::Create();
  to_rec709->setSrc(display_colorspace->name().c_str());
  to_rec709->setDst(lin_rec709_srgb);
  group->appendTransform(to_rec709);

  /* sRGB transfer function, mirrored for negative as specified by scRGB and extended sRGB. */
  auto to_extended_srgb = OCIO_NAMESPACE::ExponentWithLinearTransform::Create();
  to_extended_srgb->setGamma({2.4, 2.4, 2.4, 1.0});
  to_extended_srgb->setOffset({0.055, 0.055, 0.055, 0.0});
  to_extended_srgb->setDirection(OCIO_NAMESPACE::TRANSFORM_DIR_INVERSE);
  to_extended_srgb->setNegativeStyle(OCIO_NAMESPACE::NEGATIVE_MIRROR);
  group->appendTransform(to_extended_srgb);
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
    display_as_extended_srgb(config, group, display_parameters.display, display_parameters.view);
  }

  if (display_parameters.inverse) {
    group->setDirection(TRANSFORM_DIR_INVERSE);
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
