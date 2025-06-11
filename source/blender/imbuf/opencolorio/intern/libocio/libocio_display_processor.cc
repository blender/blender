/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_display_processor.hh"

#if defined(WITH_OPENCOLORIO)

#  include "BLI_math_matrix.hh"

#  include "OCIO_config.hh"

#  include "error_handling.hh"
#  include "libocio_config.hh"

#  include "../white_point.hh"

namespace blender::ocio {

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
