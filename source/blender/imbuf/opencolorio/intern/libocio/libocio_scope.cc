/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cmath>
#include <memory>
#include <mutex>

#include "libocio_view.hh"

#include "BLI_math_matrix.hh"
#include "BLI_string.h"

#include "OCIO_matrix.hh"
#include "OCIO_scope.hh"

namespace blender::ocio {

/**
 * Scope space encoding constants for SDR and HDR displays.
 *
 * For SDR we use the sRGB OETF to get something that is near perceptually
 * linear. For HDR we use the same curve for SDR values for the first 75% of
 * scope space, and then use log for the remaining 25% from 100 nits to
 * 10000 nits.
 *
 * The log curve is chosen such that the two pieces meet at 100 nits
 * with C1 continuity.
 *
 * Gamut is the same as the display space.
 *
 * The reason to use this instead of display space for scopes is that it
 * lets us design a curve allocates most space for SDR values. It also means
 * the scope drawing shaders can have a hardcoded color space transforms and
 * we don't need to insert arbitrary OCIO transforms into them.
 */

/** Scope value at 100 nits (SDR reference white) for HDR displays. */
static constexpr float SCOPE_SDR_IN_HDR_SCOPE = 0.75f;

/** Maximum HDR luminance in nits that maps to scope value 1.0, same as PQ and
 * beyond what any real world display can do. */
static constexpr float SCOPE_HDR_MAX_NITS = 10000.0f;

/** SDR reference white in nits. */
static constexpr float SCOPE_SDR_WHITE_NITS = 100.0f;

/** Maximum linear light value (nits / SDR white nits).
 * Linear 1.0 = 100 nits = SDR white, linear MAX_LINEAR = 10000 nits. */
static constexpr float SCOPE_MAX_LINEAR = SCOPE_HDR_MAX_NITS / SCOPE_SDR_WHITE_NITS;

/** Constants for the sRGB OETF used for the SDR portion of the scope encoding. */
static constexpr float SCOPE_SRGB_A = 1.055f;
static constexpr float SCOPE_SRGB_B = 0.055f;
static constexpr float SCOPE_SRGB_GAMMA = 2.4f;

/** Constants for the HDR log curve: scope = b * ln(linear - c) + d,
 * computed to ensure C1 continuity with the sRGB OETF at SCOPE_SDR_IN_HDR_SCOPE. */
static constexpr float SCOPE_HDR_B = 0.0368291f;
static constexpr float SCOPE_HDR_C = 0.8882908f;
static constexpr float SCOPE_HDR_D = 0.8307242f;

/** Convert linear value to sRGB using the sRGB OETF. */
static float linear_to_srgb(const float linear)
{
  if (linear <= 0.0031308f) {
    return 12.92f * linear;
  }
  return SCOPE_SRGB_A * std::pow(linear, 1.0f / SCOPE_SRGB_GAMMA) - SCOPE_SRGB_B;
}

/** Convert a nit value to a scope space position using the piecewise HDR encoding. */
static float nits_to_scope_value(const float nits)
{
  const float linear = nits / SCOPE_SDR_WHITE_NITS;
  if (linear <= 1.0f) {
    return SCOPE_SDR_IN_HDR_SCOPE * linear_to_srgb(linear);
  }
  return SCOPE_HDR_B * std::log(linear - SCOPE_HDR_C) + SCOPE_HDR_D;
}

const ScopeInfo &LibOCIOView::scope_info() const
{
  static std::mutex mutex;
  std::lock_guard lock(mutex);
  if (scope_info_) {
    return *scope_info_;
  }

  scope_info_ = std::make_unique<ScopeInfo>();

  /* Also use HDR mapping for raw scene linear data that goes beyond 1, but
   * maybe ideally it should have its own custom mapping with scene linear
   * gamut to visualize scene linear rather than display values. */
  scope_info_->is_hdr = is_hdr_ || is_data_;

  /* Scope space uses the same gamut as the display space. */
  double4x4 scope_gamut_to_rec709 = double4x4::identity();
  switch (gamut_) {
    case Gamut::Rec709:
    case Gamut::Unknown:
      break;
    case Gamut::P3D65:
      scope_gamut_to_rec709 = OCIO_XYZ_TO_REC709 * math::invert(OCIO_XYZ_TO_P3);
      break;
    case Gamut::Rec2020:
      scope_gamut_to_rec709 = OCIO_XYZ_TO_REC709 * math::invert(OCIO_XYZ_TO_REC2020);
      break;
  }
  scope_info_->scope_gamut_to_rec709 = float3x3(scope_gamut_to_rec709);

  scope_info_->luma_coefficients = scope_info_->luma_coefficients *
                                   scope_info_->scope_gamut_to_rec709;

  /* Compute RGB to YCbCr matrix commonly used in video encoding, using
   * either BT.709 (CICP matrix 1) or BT.2020 NCL (CICP matrix 9). */
  float kr = 0.2126f;
  float kb = 0.0722f;
  if (gamut_ == Gamut::Rec2020) {
    kr = 0.2627f;
    kb = 0.0593f;
  }
  const float kg = 1.0f - kr - kb;
  scope_info_->yuv_matrix = float3x3(
      float3(kr, -kr / (2.0f * (1.0f - kb)), 0.5f),
      float3(kg, -kg / (2.0f * (1.0f - kb)), -kg / (2.0f * (1.0f - kr))),
      float3(kb, 0.5f, -kb / (2.0f * (1.0f - kr))));

  if (scope_info_->is_hdr) {
    /* HDR. */
    scope_info_->view_transform_max_nits = max_nits();
    if (scope_info_->view_transform_max_nits > 0) {
      scope_info_->view_transform_max_nits_value = nits_to_scope_value(
          scope_info_->view_transform_max_nits);
    }

    scope_info_->sdr_white_level = SCOPE_SDR_IN_HDR_SCOPE;

    /* HDR: graticules at logarithmic nit values. */
    Vector<int> nit_values = {0, 18 /* middle gray */, 100};
    const int max_nits = (scope_info_->view_transform_max_nits) ?
                             scope_info_->view_transform_max_nits :
                             1000;
    if (!nit_values.contains(max_nits)) {
      nit_values.append(max_nits);
    }
    if (!nit_values.contains(10000)) {
      nit_values.append(10000);
    }

    for (const int nits : nit_values) {
      ScopeGraticule graticule;
      graticule.value = nits_to_scope_value(nits);
      if (is_data_) {
        SNPRINTF(graticule.label, "%.2f", nits / 100.0f);
      }
      else {
        SNPRINTF(graticule.label, "%d nits", nits);
      }
      scope_info_->graticules.append(graticule);
    }
  }
  else {
    /* SDR: evenly distributed graticules. */
    const float values[] = {0.0f, 0.25f, 0.50f, 0.75f, 1.0f};
    for (const float val : values) {
      ScopeGraticule graticule;
      graticule.value = val;
      SNPRINTF(graticule.label, "%.2f", val);
      scope_info_->graticules.append(graticule);
    }
  }

  return *scope_info_;
}

void LibOCIOView::append_scope_space_transforms(OCIO_NAMESPACE::GroupTransformRcPtr &group,
                                                const StringRefNull lin_cie_xyz_d65_name) const
{
  /* Matrix to convert from CIE XYZ D65 to linear display gamut. */
  double4x4 xyz_to_display_gamut;
  switch (gamut_) {
    case Gamut::Rec709:
    case Gamut::Unknown:
      xyz_to_display_gamut = OCIO_XYZ_TO_REC709;
      break;
    case Gamut::P3D65:
      xyz_to_display_gamut = OCIO_XYZ_TO_P3;
      break;
    case Gamut::Rec2020:
      xyz_to_display_gamut = OCIO_XYZ_TO_REC2020;
      break;
  }

  /* Convert from display colorspace to CIE XYZ D65. */
  auto to_cie_xyz_d65 = OCIO_NAMESPACE::ColorSpaceTransform::Create();
  to_cie_xyz_d65->setSrc(display_colorspace_->name().c_str());
  to_cie_xyz_d65->setDst(lin_cie_xyz_d65_name.c_str());
  group->appendTransform(to_cie_xyz_d65);

  /* Convert from CIE XYZ D65 to linear display gamut. */
  auto to_lin_gamut = OCIO_NAMESPACE::MatrixTransform::Create();
  to_lin_gamut->setMatrix(math::transpose(xyz_to_display_gamut).base_ptr());
  group->appendTransform(to_lin_gamut);

  if (is_hdr_ || is_data_) {
    /* Convert linear display gamut to scope space (see above for details). */

    /* Clamp to [0, SCOPE_MAX_LINEAR] and rescale to [0, 1] for the gamma below. */
    auto clamp = OCIO_NAMESPACE::RangeTransform::Create();
    clamp->setStyle(OCIO_NAMESPACE::RANGE_CLAMP);
    clamp->setMinInValue(0.0);
    clamp->setMinOutValue(0.0);
    clamp->setMaxInValue(SCOPE_MAX_LINEAR);
    clamp->setMaxOutValue(1.0);
    group->appendTransform(clamp);

    /* Apply gamma for better precision with the LUT below. */
    const double max_gamma_encoded = std::pow(SCOPE_MAX_LINEAR, 1.0 / SCOPE_SRGB_GAMMA);
    auto gamma = OCIO_NAMESPACE::ExponentTransform::Create();
    gamma->setValue({1.0 / SCOPE_SRGB_GAMMA, 1.0 / SCOPE_SRGB_GAMMA, 1.0 / SCOPE_SRGB_GAMMA, 1.0});
    gamma->setNegativeStyle(OCIO_NAMESPACE::NEGATIVE_CLAMP);
    gamma->setDirection(OCIO_NAMESPACE::TRANSFORM_DIR_FORWARD);
    group->appendTransform(gamma);

    /* 1D LUT to map gamma to our custom scope space. */
    constexpr int LUT_SIZE = 1024;
    auto lut = OCIO_NAMESPACE::Lut1DTransform::Create();
    lut->setLength(LUT_SIZE);
    for (int i = 0; i < LUT_SIZE; i++) {
      const float t = float(i) / float(LUT_SIZE - 1);
      const float g = t * max_gamma_encoded;
      const float nits = std::pow(g, SCOPE_SRGB_GAMMA) * SCOPE_SDR_WHITE_NITS;
      const float scope = nits_to_scope_value(nits);
      lut->setValue(i, scope, scope, scope);
    }
    group->appendTransform(lut);
  }
  else {
    /* SDR Display: clamp and sRGB OETF. */
    auto clamp = OCIO_NAMESPACE::RangeTransform::Create();
    clamp->setStyle(OCIO_NAMESPACE::RANGE_CLAMP);
    clamp->setMinInValue(0.0);
    clamp->setMinOutValue(0.0);
    group->appendTransform(clamp);

    auto srgb = OCIO_NAMESPACE::ExponentWithLinearTransform::Create();
    srgb->setGamma({SCOPE_SRGB_GAMMA, SCOPE_SRGB_GAMMA, SCOPE_SRGB_GAMMA, 1.0});
    srgb->setOffset({SCOPE_SRGB_B, SCOPE_SRGB_B, SCOPE_SRGB_B, 0.0});
    srgb->setDirection(OCIO_NAMESPACE::TRANSFORM_DIR_INVERSE);
    group->appendTransform(srgb);
  }
}

}  // namespace blender::ocio
