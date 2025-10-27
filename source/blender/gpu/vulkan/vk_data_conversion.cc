/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_data_conversion.hh"

#include "BLI_color.hh"
#include "BLI_math_half.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Conversion types
 * \{ */

enum class ConversionType {
  /** No conversion needed, result can be directly read back to host memory. */
  PASS_THROUGH,

  /** Pass through (ignores the stencil component). */
  PASS_THROUGH_D32F_S8,

  FLOAT_TO_UNORM8,
  UNORM8_TO_FLOAT,

  FLOAT_TO_SNORM8,
  SNORM8_TO_FLOAT,

  FLOAT_TO_UNORM16,
  UNORM16_TO_FLOAT,

  FLOAT_TO_SNORM16,
  SNORM16_TO_FLOAT,

  FLOAT_TO_UNORM32,
  UNORM32_TO_FLOAT,

  UI32_TO_UI16,
  UI16_TO_UI32,

  UI32_TO_UI8,
  UI8_TO_UI32,

  I32_TO_I16,
  I16_TO_I32,

  I32_TO_I8,
  I8_TO_I32,

  /** Convert device 16F to UINT */
  HALF_TO_UI8,
  UI8_TO_HALF,

  /** Convert device 16F to floats. */
  HALF_TO_FLOAT,
  FLOAT_TO_HALF,

  FLOAT_TO_B10F_G11F_R11F,
  B10F_G11F_R11F_TO_FLOAT,

  FLOAT3_TO_HALF4,
  HALF4_TO_FLOAT3,

  FLOAT3_TO_FLOAT4,
  FLOAT4_TO_FLOAT3,

  UINT_TO_DEPTH32F_STENCIL8,
  DEPTH32F_STENCIL8_TO_UINT,
  /**
   * The requested conversion isn't supported.
   */
  UNSUPPORTED,
};

static ConversionType type_of_conversion_float(const TextureFormat host_format,
                                               const TextureFormat device_format)
{
  if (host_format != device_format) {
    if (host_format == TextureFormat::SFLOAT_16_16_16 &&
        device_format == TextureFormat::SFLOAT_16_16_16_16)
    {
      return ConversionType::FLOAT3_TO_HALF4;
    }
    if (host_format == TextureFormat::SFLOAT_32_32_32 &&
        device_format == TextureFormat::SFLOAT_32_32_32_32)
    {
      return ConversionType::FLOAT3_TO_FLOAT4;
    }

    return ConversionType::UNSUPPORTED;
  }

  switch (device_format) {
    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::SFLOAT_32:
    case TextureFormat::SFLOAT_32_DEPTH:
      return ConversionType::PASS_THROUGH;

    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
      return ConversionType::PASS_THROUGH_D32F_S8;

    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::SFLOAT_16:
    case TextureFormat::SFLOAT_16_16_16:
      return ConversionType::FLOAT_TO_HALF;

    case TextureFormat::SRGBA_8_8_8_8:
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::UNORM_8_8:
    case TextureFormat::UNORM_8:
      return ConversionType::FLOAT_TO_UNORM8;

    case TextureFormat::SNORM_8_8_8_8:
    case TextureFormat::SNORM_8_8_8:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_8:
      return ConversionType::FLOAT_TO_SNORM8;

    case TextureFormat::UNORM_16_16_16_16:
    case TextureFormat::UNORM_16_16:
    case TextureFormat::UNORM_16:
      return ConversionType::FLOAT_TO_UNORM16;

    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::SNORM_16_16_16:
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_16:
      return ConversionType::FLOAT_TO_SNORM16;

    case TextureFormat::UFLOAT_11_11_10:
      return ConversionType::FLOAT_TO_B10F_G11F_R11F;

    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
      /* Not an actual "conversion", but compressed texture upload code
       * pretends that host data is a float. It is actually raw BCn bits. */
      return ConversionType::PASS_THROUGH;

      /* #TextureFormat::SFLOAT_32_32_32 Not supported by vendors. */
    case TextureFormat::SFLOAT_32_32_32:

    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::SINT_16_16_16_16:
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::UINT_8_8:
    case TextureFormat::SINT_8_8:
    case TextureFormat::UINT_16_16:
    case TextureFormat::SINT_16_16:
    case TextureFormat::UINT_32_32:
    case TextureFormat::SINT_32_32:
    case TextureFormat::UINT_8:
    case TextureFormat::SINT_8:
    case TextureFormat::UINT_16:
    case TextureFormat::SINT_16:
    case TextureFormat::UINT_32:
    case TextureFormat::SINT_32:
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::UINT_10_10_10_2:
    case TextureFormat::UINT_8_8_8:
    case TextureFormat::SINT_8_8_8:
    case TextureFormat::UNORM_8_8_8:
    case TextureFormat::UINT_16_16_16:
    case TextureFormat::SINT_16_16_16:
    case TextureFormat::UNORM_16_16_16:
    case TextureFormat::UINT_32_32_32:
    case TextureFormat::SINT_32_32_32:
    case TextureFormat::SRGBA_8_8_8:
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
    case TextureFormat::UNORM_16_DEPTH:
      return ConversionType::UNSUPPORTED;

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
      break;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_int(TextureFormat device_format)
{
  switch (device_format) {
    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::SINT_32_32:
    case TextureFormat::SINT_32:
      return ConversionType::PASS_THROUGH;

    case TextureFormat::SINT_16_16_16_16:
    case TextureFormat::SINT_16_16:
    case TextureFormat::SINT_16:
      return ConversionType::I32_TO_I16;

    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::SINT_8_8:
    case TextureFormat::SINT_8:
      return ConversionType::I32_TO_I8;

    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::UNORM_16_16_16_16:
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::UINT_8_8:
    case TextureFormat::UNORM_8_8:
    case TextureFormat::UINT_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::UINT_32_32:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::UNORM_16_16:
    case TextureFormat::UINT_8:
    case TextureFormat::UNORM_8:
    case TextureFormat::UINT_16:
    case TextureFormat::SFLOAT_16:
    case TextureFormat::UNORM_16:
    case TextureFormat::UINT_32:
    case TextureFormat::SFLOAT_32:
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::UINT_10_10_10_2:
    case TextureFormat::UFLOAT_11_11_10:
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
    case TextureFormat::SRGBA_8_8_8_8:
    case TextureFormat::SNORM_8_8_8_8:
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::UINT_8_8_8:
    case TextureFormat::SINT_8_8_8:
    case TextureFormat::UNORM_8_8_8:
    case TextureFormat::SNORM_8_8_8:
    case TextureFormat::UINT_16_16_16:
    case TextureFormat::SINT_16_16_16:
    case TextureFormat::SFLOAT_16_16_16:
    case TextureFormat::UNORM_16_16_16:
    case TextureFormat::SNORM_16_16_16:
    case TextureFormat::UINT_32_32_32:
    case TextureFormat::SINT_32_32_32:
    case TextureFormat::SFLOAT_32_32_32:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_8:
    case TextureFormat::SNORM_16:
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
    case TextureFormat::SRGBA_8_8_8:
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::UNORM_16_DEPTH:
      return ConversionType::UNSUPPORTED;

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
      break;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_uint(TextureFormat device_format)
{
  switch (device_format) {
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::UINT_32_32:
    case TextureFormat::UINT_32:
      return ConversionType::PASS_THROUGH;

    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::UINT_16_16:
    case TextureFormat::UINT_16:
    case TextureFormat::UINT_16_16_16:
      return ConversionType::UI32_TO_UI16;

    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::UINT_8_8:
    case TextureFormat::UINT_8:
      return ConversionType::UI32_TO_UI8;

    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
      return ConversionType::UNORM32_TO_FLOAT;

    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::SINT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::UNORM_16_16_16_16:
    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::SINT_8_8:
    case TextureFormat::UNORM_8_8:
    case TextureFormat::SINT_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::UNORM_16_16:
    case TextureFormat::SINT_32_32:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::SINT_8:
    case TextureFormat::UNORM_8:
    case TextureFormat::SINT_16:
    case TextureFormat::SFLOAT_16:
    case TextureFormat::UNORM_16:
    case TextureFormat::SINT_32:
    case TextureFormat::SFLOAT_32:
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::UINT_10_10_10_2:
    case TextureFormat::UFLOAT_11_11_10:
    case TextureFormat::SRGBA_8_8_8_8:
    case TextureFormat::SNORM_8_8_8_8:
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::UINT_8_8_8:
    case TextureFormat::SINT_8_8_8:
    case TextureFormat::UNORM_8_8_8:
    case TextureFormat::SNORM_8_8_8:
    case TextureFormat::SINT_16_16_16:
    case TextureFormat::SFLOAT_16_16_16:
    case TextureFormat::UNORM_16_16_16:
    case TextureFormat::SNORM_16_16_16:
    case TextureFormat::UINT_32_32_32:
    case TextureFormat::SINT_32_32_32:
    case TextureFormat::SFLOAT_32_32_32:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_8:
    case TextureFormat::SNORM_16:
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
    case TextureFormat::SRGBA_8_8_8:
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
    case TextureFormat::UNORM_16_DEPTH:
      return ConversionType::UNSUPPORTED;

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
      break;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_half(TextureFormat device_format)
{
  switch (device_format) {
    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::SFLOAT_16:
      return ConversionType::PASS_THROUGH;

    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::SINT_16_16_16_16:
    case TextureFormat::UNORM_16_16_16_16:
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::UINT_8_8:
    case TextureFormat::SINT_8_8:
    case TextureFormat::UNORM_8_8:
    case TextureFormat::UINT_16_16:
    case TextureFormat::SINT_16_16:
    case TextureFormat::UNORM_16_16:
    case TextureFormat::UINT_32_32:
    case TextureFormat::SINT_32_32:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::UINT_8:
    case TextureFormat::SINT_8:
    case TextureFormat::UNORM_8:
    case TextureFormat::UINT_16:
    case TextureFormat::SINT_16:
    case TextureFormat::UNORM_16:
    case TextureFormat::UINT_32:
    case TextureFormat::SINT_32:
    case TextureFormat::SFLOAT_32:
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::UINT_10_10_10_2:
    case TextureFormat::UFLOAT_11_11_10:
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
    case TextureFormat::SRGBA_8_8_8_8:
    case TextureFormat::SNORM_8_8_8_8:
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::UINT_8_8_8:
    case TextureFormat::SINT_8_8_8:
    case TextureFormat::UNORM_8_8_8:
    case TextureFormat::SNORM_8_8_8:
    case TextureFormat::UINT_16_16_16:
    case TextureFormat::SINT_16_16_16:
    case TextureFormat::SFLOAT_16_16_16:
    case TextureFormat::UNORM_16_16_16:
    case TextureFormat::SNORM_16_16_16:
    case TextureFormat::UINT_32_32_32:
    case TextureFormat::SINT_32_32_32:
    case TextureFormat::SFLOAT_32_32_32:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_8:
    case TextureFormat::SNORM_16:
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
    case TextureFormat::SRGBA_8_8_8:
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::UNORM_16_DEPTH:
      return ConversionType::UNSUPPORTED;

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
      break;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_ubyte(TextureFormat device_format)
{
  switch (device_format) {
    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::UINT_8_8:
    case TextureFormat::UNORM_8_8:
    case TextureFormat::UINT_8:
    case TextureFormat::UNORM_8:
    case TextureFormat::SRGBA_8_8_8_8:
      return ConversionType::PASS_THROUGH;

    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::SFLOAT_16:
      return ConversionType::UI8_TO_HALF;

    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::SINT_16_16_16_16:
    case TextureFormat::UNORM_16_16_16_16:
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::SINT_8_8:
    case TextureFormat::UINT_16_16:
    case TextureFormat::SINT_16_16:
    case TextureFormat::UNORM_16_16:
    case TextureFormat::UINT_32_32:
    case TextureFormat::SINT_32_32:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::SINT_8:
    case TextureFormat::UINT_16:
    case TextureFormat::SINT_16:
    case TextureFormat::UNORM_16:
    case TextureFormat::UINT_32:
    case TextureFormat::SINT_32:
    case TextureFormat::SFLOAT_32:
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::UINT_10_10_10_2:
    case TextureFormat::UFLOAT_11_11_10:
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
    case TextureFormat::SNORM_8_8_8_8:
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::UINT_8_8_8:
    case TextureFormat::SINT_8_8_8:
    case TextureFormat::UNORM_8_8_8:
    case TextureFormat::SNORM_8_8_8:
    case TextureFormat::UINT_16_16_16:
    case TextureFormat::SINT_16_16_16:
    case TextureFormat::SFLOAT_16_16_16:
    case TextureFormat::UNORM_16_16_16:
    case TextureFormat::SNORM_16_16_16:
    case TextureFormat::UINT_32_32_32:
    case TextureFormat::SINT_32_32_32:
    case TextureFormat::SFLOAT_32_32_32:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_8:
    case TextureFormat::SNORM_16:
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
    case TextureFormat::SRGBA_8_8_8:
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::UNORM_16_DEPTH:
      return ConversionType::UNSUPPORTED;

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
      break;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_uint248(const TextureFormat device_format)
{
  switch (device_format) {
    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
      return ConversionType::UINT_TO_DEPTH32F_STENCIL8;

    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::SFLOAT_32:
    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::SFLOAT_16:
    case TextureFormat::SFLOAT_16_16_16:
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::UNORM_8_8:
    case TextureFormat::UNORM_8:
    case TextureFormat::SNORM_8_8_8_8:
    case TextureFormat::SNORM_8_8_8:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_8:
    case TextureFormat::UNORM_16_16_16_16:
    case TextureFormat::UNORM_16_16:
    case TextureFormat::UNORM_16:
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::SNORM_16_16_16:
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_16:
    case TextureFormat::SRGBA_8_8_8_8:
    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::UFLOAT_11_11_10:
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:

      /* #TextureFormat::SFLOAT_32_32_32 Not supported by vendors. */
    case TextureFormat::SFLOAT_32_32_32:

    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::SINT_16_16_16_16:
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::UINT_8_8:
    case TextureFormat::SINT_8_8:
    case TextureFormat::UINT_16_16:
    case TextureFormat::SINT_16_16:
    case TextureFormat::UINT_32_32:
    case TextureFormat::SINT_32_32:
    case TextureFormat::UINT_8:
    case TextureFormat::SINT_8:
    case TextureFormat::UINT_16:
    case TextureFormat::SINT_16:
    case TextureFormat::UINT_32:
    case TextureFormat::SINT_32:
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::UINT_10_10_10_2:
    case TextureFormat::UINT_8_8_8:
    case TextureFormat::SINT_8_8_8:
    case TextureFormat::UNORM_8_8_8:
    case TextureFormat::UINT_16_16_16:
    case TextureFormat::SINT_16_16_16:
    case TextureFormat::UNORM_16_16_16:
    case TextureFormat::UINT_32_32_32:
    case TextureFormat::SINT_32_32_32:
    case TextureFormat::SRGBA_8_8_8:
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
    case TextureFormat::UNORM_16_DEPTH:
      return ConversionType::UNSUPPORTED;

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
      break;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_r11g11b10(TextureFormat device_format)
{
  if (device_format == TextureFormat::UFLOAT_11_11_10) {
    return ConversionType::PASS_THROUGH;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_r10g10b10a2(TextureFormat device_format)
{
  if (ELEM(device_format, TextureFormat::UNORM_10_10_10_2, TextureFormat::UINT_10_10_10_2)) {
    return ConversionType::PASS_THROUGH;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType host_to_device(const eGPUDataFormat host_format,
                                     const TextureFormat host_texture_format,
                                     const TextureFormat device_format)
{
  switch (host_format) {
    case GPU_DATA_FLOAT:
      return type_of_conversion_float(host_texture_format, device_format);
    case GPU_DATA_UINT:
      return type_of_conversion_uint(device_format);
    case GPU_DATA_INT:
      return type_of_conversion_int(device_format);
    case GPU_DATA_HALF_FLOAT:
      return type_of_conversion_half(device_format);
    case GPU_DATA_UBYTE:
      return type_of_conversion_ubyte(device_format);
    case GPU_DATA_10_11_11_REV:
      return type_of_conversion_r11g11b10(device_format);
    case GPU_DATA_2_10_10_10_REV:
      return type_of_conversion_r10g10b10a2(device_format);
    case GPU_DATA_UINT_24_8_DEPRECATED:
      return type_of_conversion_uint248(device_format);
  }

  return ConversionType::UNSUPPORTED;
}

static ConversionType reversed(ConversionType type)
{
#define CASE_SINGLE(a, b) \
  case ConversionType::a##_TO_##b: \
    return ConversionType::b##_TO_##a;

#define CASE_PAIR(a, b) \
  CASE_SINGLE(a, b) \
  CASE_SINGLE(b, a)

  switch (type) {
    case ConversionType::PASS_THROUGH:
      return ConversionType::PASS_THROUGH;
    case ConversionType::PASS_THROUGH_D32F_S8:
      return ConversionType::PASS_THROUGH_D32F_S8;

      CASE_PAIR(FLOAT, UNORM8)
      CASE_PAIR(FLOAT, SNORM8)
      CASE_PAIR(FLOAT, UNORM16)
      CASE_PAIR(FLOAT, SNORM16)
      CASE_PAIR(FLOAT, UNORM32)
      CASE_PAIR(UI32, UI16)
      CASE_PAIR(I32, I16)
      CASE_PAIR(UI32, UI8)
      CASE_PAIR(I32, I8)
      CASE_PAIR(FLOAT, HALF)
      CASE_PAIR(FLOAT, B10F_G11F_R11F)
      CASE_PAIR(FLOAT3, HALF4)
      CASE_PAIR(FLOAT3, FLOAT4)
      CASE_PAIR(UINT, DEPTH32F_STENCIL8)
      CASE_PAIR(UI8, HALF)

    case ConversionType::UNSUPPORTED:
      return ConversionType::UNSUPPORTED;
  }

#undef CASE_PAIR
#undef CASE_SINGLE

  return ConversionType::UNSUPPORTED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Conversion
 * \{ */

static uint32_t float_to_uint32_t(float value)
{
  union {
    float fl;
    uint32_t u;
  } float_to_bits;
  float_to_bits.fl = value;
  return float_to_bits.u;
}

static float uint32_t_to_float(uint32_t value)
{
  union {
    float fl;
    uint32_t u;
  } float_to_bits;
  float_to_bits.u = value;
  return float_to_bits.fl;
}

template<typename InnerType> struct ComponentValue {
  InnerType value;
};
template<typename InnerType> struct PixelValue {
  InnerType value;
};

using UI8 = ComponentValue<uint8_t>;
using UI16 = ComponentValue<uint16_t>;
using UI32 = ComponentValue<uint32_t>;
using I8 = ComponentValue<int8_t>;
using I16 = ComponentValue<int16_t>;
using I32 = ComponentValue<int32_t>;
using F16 = ComponentValue<uint16_t>;
using F32 = ComponentValue<float>;
using FLOAT3 = PixelValue<float3>;
using FLOAT4 = PixelValue<ColorSceneLinear4f<eAlpha::Premultiplied>>;
/* NOTE: Vulkan stores R11_G11_B10 in reverse component order. */
class B10F_G11G_R11F : public PixelValue<uint32_t> {};

class HALF4 : public PixelValue<uint64_t> {
 public:
  uint32_t get_r() const
  {
    return value & 0xffff;
  }

  void set_r(uint64_t new_value)
  {
    value = (value & 0xffffffffffff0000) | (new_value & 0xffff);
  }
  uint64_t get_g() const
  {
    return (value >> 16) & 0xffff;
  }

  void set_g(uint64_t new_value)
  {
    value = (value & 0xffffffff0000ffff) | ((new_value & 0xffff) << 16);
  }
  uint64_t get_b() const
  {
    return (value >> 32) & 0xffff;
  }

  void set_b(uint64_t new_value)
  {
    value = (value & 0xffff0000ffffffff) | ((new_value & 0xffff) << 32);
  }

  void set_a(uint64_t new_value)
  {
    value = (value & 0xffffffffffff) | ((new_value & 0xffff) << 48);
  }
};

/* Use a float as we only have the depth aspect in the staging buffers. */
struct Depth32fStencil8 : ComponentValue<float> {};

template<typename InnerType> struct SignedNormalized {
  static_assert(std::is_same<InnerType, uint8_t>() || std::is_same<InnerType, uint16_t>());
  InnerType value;

  static constexpr int32_t scalar()
  {
    return (1 << (sizeof(InnerType) * 8 - 1));
  }

  static constexpr int32_t delta()
  {
    return (1 << (sizeof(InnerType) * 8 - 1)) - 1;
  }

  static constexpr int32_t max()
  {
    return ((1 << (sizeof(InnerType) * 8)) - 1);
  }
};

template<typename InnerType> struct UnsignedNormalized {
  static_assert(std::is_same<InnerType, uint8_t>() || std::is_same<InnerType, uint16_t>() ||
                std::is_same<InnerType, uint32_t>());
  InnerType value;

  static constexpr size_t used_byte_size()
  {
    return sizeof(InnerType);
  }

  static constexpr uint32_t scalar()
  {
    return std::numeric_limits<InnerType>::max();
  }

  static constexpr uint32_t max()
  {
    return std::numeric_limits<InnerType>::max();
  }
};

template<typename StorageType> void convert(SignedNormalized<StorageType> &dst, const F32 &src)
{
  static constexpr int32_t scalar = SignedNormalized<StorageType>::scalar();
  static constexpr int32_t delta = SignedNormalized<StorageType>::delta();
  static constexpr int32_t max = SignedNormalized<StorageType>::max();
  dst.value = clamp_i((src.value * scalar + delta), 0, max);
}

template<typename StorageType> void convert(F32 &dst, const SignedNormalized<StorageType> &src)
{
  static constexpr int32_t scalar = SignedNormalized<StorageType>::scalar();
  static constexpr int32_t delta = SignedNormalized<StorageType>::delta();
  dst.value = float(int32_t(src.value) - delta) / scalar;
}

template<typename StorageType> void convert(UnsignedNormalized<StorageType> &dst, const F32 &src)
{
  static constexpr uint32_t scalar = UnsignedNormalized<StorageType>::scalar();
  static constexpr uint32_t max = scalar;
  /* When converting a DEPTH32F to DEPTH24 the scalar gets to large where 1.0 will wrap around and
   * become 0. Make sure that depth 1.0 will not wrap around. Without this gpu_select_pick will
   * fail as all depth 1.0 will occlude previous depths. */
  dst.value = src.value >= 1.0f ? max : max_ff(src.value * float(scalar), 0.0);
}

template<typename StorageType> void convert(F32 &dst, const UnsignedNormalized<StorageType> &src)
{
  static constexpr uint32_t scalar = UnsignedNormalized<StorageType>::scalar();
  dst.value = float(uint32_t(src.value & scalar)) / float(scalar);
}

template<typename StorageType>
void convert(UnsignedNormalized<StorageType> & /*dst*/, const UI32 & /*src*/)
{
  BLI_assert_unreachable();
}

template<typename StorageType> void convert(UI32 &dst, const UnsignedNormalized<StorageType> &src)
{
  static constexpr uint32_t scalar = UnsignedNormalized<StorageType>::scalar();
  dst.value = uint32_t(src.value) & scalar;
}

/* Copy the contents of src to dst with out performing any actual conversion. */
template<typename DestinationType, typename SourceType>
void convert(DestinationType &dst, const SourceType &src)
{
  static_assert(std::is_same<DestinationType, UI8>() || std::is_same<DestinationType, UI16>() ||
                std::is_same<DestinationType, UI32>() || std::is_same<DestinationType, I8>() ||
                std::is_same<DestinationType, I16>() || std::is_same<DestinationType, I32>());
  static_assert(std::is_same<SourceType, UI8>() || std::is_same<SourceType, UI16>() ||
                std::is_same<SourceType, UI32>() || std::is_same<SourceType, I8>() ||
                std::is_same<SourceType, I16>() || std::is_same<SourceType, I32>());
  static_assert(!std::is_same<DestinationType, SourceType>());
  dst.value = src.value;
}

static void convert(FLOAT3 &dst, const HALF4 &src)
{
  dst.value.x = math::half_to_float(src.get_r());
  dst.value.y = math::half_to_float(src.get_g());
  dst.value.z = math::half_to_float(src.get_b());
}

static void convert(HALF4 &dst, const FLOAT3 &src)
{
  dst.set_r(math::float_to_half(src.value.x));
  dst.set_g(math::float_to_half(src.value.y));
  dst.set_b(math::float_to_half(src.value.z));
  dst.set_a(0x3c00); /* FP16 1.0 */
}

static void convert(FLOAT3 &dst, const FLOAT4 &src)
{
  dst.value.x = src.value.r;
  dst.value.y = src.value.g;
  dst.value.z = src.value.b;
}

static void convert(FLOAT4 &dst, const FLOAT3 &src)
{
  dst.value.r = src.value.x;
  dst.value.g = src.value.y;
  dst.value.b = src.value.z;
  dst.value.a = 1.0f;
}

static void convert(F16 &dst, const UI8 &src)
{
  UnsignedNormalized<uint8_t> un8;
  un8.value = src.value;
  F32 f32;
  convert(f32, un8);
  dst.value = math::float_to_half(f32.value);
}

static void convert(UI8 &dst, const F16 &src)
{
  F32 f32;
  f32.value = math::half_to_float(src.value);
  UnsignedNormalized<uint8_t> un8;
  convert(un8, f32);
  dst.value = un8.value;
}

constexpr uint32_t MASK_10_BITS = 0b1111111111;
constexpr uint32_t MASK_11_BITS = 0b11111111111;
constexpr uint8_t SHIFT_B = 22;
constexpr uint8_t SHIFT_G = 11;
constexpr uint8_t SHIFT_R = 0;

static void convert(FLOAT3 &dst, const B10F_G11G_R11F &src)
{
  dst.value.x = uint32_t_to_float(
      convert_float_formats<FormatF32, FormatF11>((src.value >> SHIFT_R) & MASK_11_BITS));
  dst.value.y = uint32_t_to_float(
      convert_float_formats<FormatF32, FormatF11>((src.value >> SHIFT_G) & MASK_11_BITS));
  dst.value.z = uint32_t_to_float(
      convert_float_formats<FormatF32, FormatF10>((src.value >> SHIFT_B) & MASK_10_BITS));
}

static void convert(B10F_G11G_R11F &dst, const FLOAT3 &src)
{
  uint32_t r = convert_float_formats<FormatF11, FormatF32>(float_to_uint32_t(src.value.x));
  uint32_t g = convert_float_formats<FormatF11, FormatF32>(float_to_uint32_t(src.value.y));
  uint32_t b = convert_float_formats<FormatF10, FormatF32>(float_to_uint32_t(src.value.z));
  dst.value = r << SHIFT_R | g << SHIFT_G | b << SHIFT_B;
}

/** \} */

static void convert(UI32 &dst, const Depth32fStencil8 &src)
{
  uint32_t depth = uint32_t(src.value * 0xFFFFFF);
  dst.value = (depth << 8);
}

static void convert(Depth32fStencil8 &dst, const UI32 &src)
{
  uint32_t depth = (src.value >> 8) & 0xFFFFFF;
  dst.value = float(depth) * 0xFFFFFF;
}

template<typename DestinationType, typename SourceType>
void convert(MutableSpan<DestinationType> dst, Span<SourceType> src)
{
  BLI_assert(src.size() == dst.size());
  for (int64_t index : IndexRange(src.size())) {
    convert(dst[index], src[index]);
  }
}

template<typename DestinationType, typename SourceType>
void convert_per_component(void *dst_memory,
                           const void *src_memory,
                           size_t buffer_size,
                           TextureFormat device_format)
{
  size_t total_components = to_component_len(device_format) * buffer_size;
  Span<SourceType> src = Span<SourceType>(static_cast<const SourceType *>(src_memory),
                                          total_components);
  MutableSpan<DestinationType> dst = MutableSpan<DestinationType>(
      static_cast<DestinationType *>(dst_memory), total_components);
  convert<DestinationType, SourceType>(dst, src);
}

template<typename DestinationType, typename SourceType>
void convert_per_pixel(void *dst_memory, const void *src_memory, size_t buffer_size)
{
  Span<SourceType> src = Span<SourceType>(static_cast<const SourceType *>(src_memory),
                                          buffer_size);
  MutableSpan<DestinationType> dst = MutableSpan<DestinationType>(
      static_cast<DestinationType *>(dst_memory), buffer_size);
  convert<DestinationType, SourceType>(dst, src);
}

static void convert_buffer(void *dst_memory,
                           const void *src_memory,
                           size_t buffer_size,
                           TextureFormat device_format,
                           ConversionType type)
{
  switch (type) {
    case ConversionType::UNSUPPORTED:
      return;

    case ConversionType::PASS_THROUGH:
      memcpy(dst_memory, src_memory, buffer_size * to_bytesize(device_format));
      return;

    case ConversionType::PASS_THROUGH_D32F_S8:
      memcpy(dst_memory, src_memory, buffer_size * to_bytesize(TextureFormat::SFLOAT_32_DEPTH));
      return;

    case ConversionType::UI32_TO_UI16:
      convert_per_component<UI16, UI32>(dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::UI16_TO_UI32:
      convert_per_component<UI32, UI16>(dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::UI32_TO_UI8:
      convert_per_component<UI8, UI32>(dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::UI8_TO_UI32:
      convert_per_component<UI32, UI8>(dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::I32_TO_I16:
      convert_per_component<I16, I32>(dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::I16_TO_I32:
      convert_per_component<I32, I16>(dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::I32_TO_I8:
      convert_per_component<I8, I32>(dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::I8_TO_I32:
      convert_per_component<I32, I8>(dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::FLOAT_TO_SNORM8:
      convert_per_component<SignedNormalized<uint8_t>, F32>(
          dst_memory, src_memory, buffer_size, device_format);
      break;
    case ConversionType::SNORM8_TO_FLOAT:
      convert_per_component<F32, SignedNormalized<uint8_t>>(
          dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::FLOAT_TO_SNORM16:
      convert_per_component<SignedNormalized<uint16_t>, F32>(
          dst_memory, src_memory, buffer_size, device_format);
      break;
    case ConversionType::SNORM16_TO_FLOAT:
      convert_per_component<F32, SignedNormalized<uint16_t>>(
          dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::FLOAT_TO_UNORM8:
      convert_per_component<UnsignedNormalized<uint8_t>, F32>(
          dst_memory, src_memory, buffer_size, device_format);
      break;
    case ConversionType::UNORM8_TO_FLOAT:
      convert_per_component<F32, UnsignedNormalized<uint8_t>>(
          dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::FLOAT_TO_UNORM16:
      convert_per_component<UnsignedNormalized<uint16_t>, F32>(
          dst_memory, src_memory, buffer_size, device_format);
      break;
    case ConversionType::UNORM16_TO_FLOAT:
      convert_per_component<F32, UnsignedNormalized<uint16_t>>(
          dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::FLOAT_TO_UNORM32:
      convert_per_component<UnsignedNormalized<uint32_t>, F32>(
          dst_memory, src_memory, buffer_size, device_format);
      break;
    case ConversionType::UNORM32_TO_FLOAT:
      convert_per_component<F32, UnsignedNormalized<uint32_t>>(
          dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::UI8_TO_HALF:
      convert_per_component<F16, UI8>(dst_memory, src_memory, buffer_size, device_format);
      break;
    case ConversionType::HALF_TO_UI8:
      convert_per_component<UI8, F16>(dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::FLOAT_TO_HALF: {
      size_t element_len = to_component_len(device_format) * buffer_size;
      Span<float> src(static_cast<const float *>(src_memory), element_len);
      MutableSpan<uint16_t> dst(static_cast<uint16_t *>(dst_memory), element_len);

      constexpr int64_t chunk_size = 4 * 1024 * 1024;

      threading::parallel_for(IndexRange(element_len), chunk_size, [&](const IndexRange range) {
        /* Doing float to half conversion manually to avoid implementation specific behavior
         * regarding Inf and NaNs. Use make finite version to avoid unexpected black pixels on
         * certain implementation. For platform parity we clamp these infinite values to finite
         * values. */
        blender::math::float_to_half_make_finite_array(
            src.slice(range).data(), dst.slice(range).data(), range.size());
      });
      break;
    }
    case ConversionType::HALF_TO_FLOAT:
      blender::math::half_to_float_array(static_cast<const uint16_t *>(src_memory),
                                         static_cast<float *>(dst_memory),
                                         to_component_len(device_format) * buffer_size);
      break;

    case ConversionType::FLOAT_TO_B10F_G11F_R11F:
      convert_per_pixel<B10F_G11G_R11F, FLOAT3>(dst_memory, src_memory, buffer_size);
      break;

    case ConversionType::DEPTH32F_STENCIL8_TO_UINT:
      convert_per_pixel<UI32, Depth32fStencil8>(dst_memory, src_memory, buffer_size);
      break;
    case ConversionType::UINT_TO_DEPTH32F_STENCIL8:
      convert_per_pixel<Depth32fStencil8, UI32>(dst_memory, src_memory, buffer_size);
      break;

    case ConversionType::B10F_G11F_R11F_TO_FLOAT:
      convert_per_pixel<FLOAT3, B10F_G11G_R11F>(dst_memory, src_memory, buffer_size);
      break;

    case ConversionType::FLOAT3_TO_HALF4:
      convert_per_pixel<HALF4, FLOAT3>(dst_memory, src_memory, buffer_size);
      break;
    case ConversionType::HALF4_TO_FLOAT3:
      convert_per_pixel<FLOAT3, HALF4>(dst_memory, src_memory, buffer_size);
      break;

    case ConversionType::FLOAT3_TO_FLOAT4:
      convert_per_pixel<FLOAT4, FLOAT3>(dst_memory, src_memory, buffer_size);
      break;
    case ConversionType::FLOAT4_TO_FLOAT3:
      convert_per_pixel<FLOAT3, FLOAT4>(dst_memory, src_memory, buffer_size);
      break;
  }
}

/* -------------------------------------------------------------------- */
/** \name API
 * \{ */

void convert_host_to_device(void *dst_buffer,
                            const void *src_buffer,
                            size_t buffer_size,
                            eGPUDataFormat host_format,
                            TextureFormat host_texture_format,
                            TextureFormat device_format)
{
  ConversionType conversion_type = host_to_device(host_format, host_texture_format, device_format);
  BLI_assert(conversion_type != ConversionType::UNSUPPORTED);
  convert_buffer(dst_buffer, src_buffer, buffer_size, device_format, conversion_type);
}

void convert_device_to_host(void *dst_buffer,
                            const void *src_buffer,
                            size_t buffer_size,
                            eGPUDataFormat host_format,
                            TextureFormat host_texture_format,
                            TextureFormat device_format)
{
  ConversionType conversion_type = reversed(
      host_to_device(host_format, host_texture_format, device_format));
  BLI_assert_msg(conversion_type != ConversionType::UNSUPPORTED,
                 "Data conversion between host_format and device_format isn't supported (yet).");
  convert_buffer(dst_buffer, src_buffer, buffer_size, device_format, conversion_type);
}

/** \} */

}  // namespace blender::gpu
