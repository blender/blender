/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_math_vector_types.hh"

#include "gpu_texture_private.hh"

namespace blender::gpu {

/**
 * Convert host buffer to device buffer.
 *
 * \param dst_buffer: device buffer.
 * \param src_buffer: host buffer.
 * \param buffer_size: number of pixels to convert from the start of the given buffer.
 * \param host_format: format of the host buffer.
 * \param device_format: format of the device buffer.
 *
 * \note Will assert when the host_format/device_format combination isn't valid
 * (#validate_data_format) or supported. Some combinations aren't supported in Vulkan due to
 * platform incompatibility.
 */
void convert_host_to_device(void *dst_buffer,
                            const void *src_buffer,
                            size_t buffer_size,
                            eGPUDataFormat host_format,
                            eGPUTextureFormat device_format);

/**
 * Convert host buffer to device buffer with row length.
 *
 * \param dst_buffer: device buffer.
 * \param src_buffer: host buffer.
 * \param src_size: size of the host buffer.
 * \param src_row_length: Length of a single row of the buffer (in pixels).
 * \param host_format: format of the host buffer.
 * \param device_format: format of the device buffer.
 *
 * \note Will assert when the host_format/device_format combination isn't valid
 * (#validate_data_format) or supported. Some combinations aren't supported in Vulkan due to
 * platform incompatibility.
 */
void convert_host_to_device(void *dst_buffer,
                            const void *src_buffer,
                            uint2 src_size,
                            uint src_row_length,
                            eGPUDataFormat host_format,
                            eGPUTextureFormat device_format);

/**
 * Convert device buffer to host buffer.
 *
 * \param dst_buffer: host buffer
 * \param src_buffer: device buffer.
 * \param buffer_size: number of pixels to convert from the start of the given buffer.
 * \param host_format: format of the host buffer
 * \param device_format: format of the device buffer.
 *
 * \note Will assert when the host_format/device_format combination isn't valid
 * (#validate_data_format) or supported. Some combinations aren't supported in Vulkan due to
 * platform incompatibility.
 */
void convert_device_to_host(void *dst_buffer,
                            const void *src_buffer,
                            size_t buffer_size,
                            eGPUDataFormat host_format,
                            eGPUTextureFormat device_format);

/**
 * Are all attributes of the given vertex format natively supported or does conversion needs to
 * happen.
 *
 * \param vertex_format: the vertex format to check if an associated buffer requires conversion
 *                       being done on the host.
 */
bool conversion_needed(const GPUVertFormat &vertex_format);

/**
 * Convert the given `data` to contain Vulkan natively supported data formats.
 *
 * When for an vertex attribute the fetch mode is set to GPU_FETCH_INT_TO_FLOAT and the attribute
 * is an int32_t or uint32_t the conversion will be done. Attributes of 16 or 8 bits are supported
 * natively and will be done in Vulkan.
 *
 * \param data: Buffer to convert. Data will be converted in place.
 * \param vertex_format: Vertex format of the given data. Attributes that aren't supported will be
 *        converted to a supported one.
 *  \param vertex_len: Number of vertices of the given data buffer;
 *        The number of vertices to convert.
 */
void convert_in_place(void *data, const GPUVertFormat &vertex_format, const uint vertex_len);

/* -------------------------------------------------------------------- */
/** \name Floating point conversions
 * \{ */

/**
 * Description of a IEEE 754-1985 floating point data type.
 */
template<bool HasSignBit, uint8_t MantissaBitLen, uint8_t ExponentBitLen>
class FloatingPointFormat {
 public:
  static constexpr bool HAS_SIGN = HasSignBit;
  static constexpr uint8_t SIGN_SHIFT = MantissaBitLen + ExponentBitLen;
  static constexpr uint32_t SIGN_MASK = HasSignBit ? 1 : 0;
  static constexpr uint8_t MANTISSA_LEN = MantissaBitLen;
  static constexpr uint8_t MANTISSA_SHIFT = 0;
  static constexpr uint32_t MANTISSA_MASK = (1 << MantissaBitLen) - 1;
  static constexpr uint32_t MANTISSA_NAN_MASK = MANTISSA_MASK;
  static constexpr uint8_t EXPONENT_SHIFT = MantissaBitLen;
  static constexpr uint8_t EXPONENT_LEN = ExponentBitLen;
  static constexpr uint32_t EXPONENT_MASK = (1 << ExponentBitLen) - 1;
  static constexpr int32_t EXPONENT_BIAS = (1 << (ExponentBitLen - 1)) - 1;
  static constexpr int32_t EXPONENT_SPECIAL_MASK = EXPONENT_MASK;

  static uint32_t get_mantissa(uint32_t floating_point_number)
  {
    return (floating_point_number >> MANTISSA_SHIFT) & MANTISSA_MASK;
  }
  static uint32_t clear_mantissa(uint32_t floating_point_number)
  {
    return floating_point_number & ~(MANTISSA_MASK << MANTISSA_SHIFT);
  }
  static uint32_t set_mantissa(uint32_t mantissa, uint32_t floating_point_number)
  {
    uint32_t result = clear_mantissa(floating_point_number);
    result |= mantissa << MANTISSA_SHIFT;
    return result;
  }

  static uint32_t get_exponent(uint32_t floating_point_number)
  {
    return ((floating_point_number >> EXPONENT_SHIFT) & EXPONENT_MASK);
  }
  static uint32_t clear_exponent(uint32_t floating_point_number)
  {
    return floating_point_number & ~(EXPONENT_MASK << EXPONENT_SHIFT);
  }
  static uint32_t set_exponent(uint32_t exponent, uint32_t floating_point_number)
  {
    uint32_t result = clear_exponent(floating_point_number);
    result |= (exponent) << EXPONENT_SHIFT;
    return result;
  }

  static bool is_signed(uint32_t floating_point_number)
  {
    if constexpr (HasSignBit) {
      return (floating_point_number >> SIGN_SHIFT) & SIGN_MASK;
    }
    return false;
  }
  static uint32_t clear_sign(uint32_t floating_point_number)
  {
    return floating_point_number & ~(1 << SIGN_SHIFT);
  }

  static uint32_t set_sign(bool sign, uint32_t floating_point_number)
  {
    if constexpr (!HasSignBit) {
      return floating_point_number;
    }
    uint32_t result = clear_sign(floating_point_number);
    result |= uint32_t(sign) << SIGN_SHIFT;
    return result;
  }
};

using FormatF32 = FloatingPointFormat<true, 23, 8>;
using FormatF16 = FloatingPointFormat<true, 10, 5>;
using FormatF11 = FloatingPointFormat<false, 6, 5>;
using FormatF10 = FloatingPointFormat<false, 5, 5>;

/**
 * Convert between low precision floating (including 32 bit floats).
 *
 * The input and output values are bits (uint32_t) as this function does a bit-wise operations to
 * convert between the formats. Additional conversion rules can be applied to the conversion
 * function. Due to the implementation the compiler would make an optimized version depending on
 * the actual possibilities.
 */
template<
    /**
     * FloatingPointFormat of the value that is converted to.
     */
    typename DestinationFormat,

    /**
     * FloatingPointFormat of the value that is converted from.
     */
    typename SourceFormat,

    /**
     * Should negative values be clamped to zero when DestinationFormat doesn't contain a sign
     * bit. Also -Inf will be clamped to zero.
     *
     * When set to `false` and DestinationFormat doesn't contain a sign bit the value will be
     * made absolute.
     */
    bool ClampNegativeToZero = true>
uint32_t convert_float_formats(uint32_t value)
{
  bool is_signed = SourceFormat::is_signed(value);
  uint32_t mantissa = SourceFormat::get_mantissa(value);
  int32_t exponent = SourceFormat::get_exponent(value);

  const bool is_nan = (exponent == SourceFormat::EXPONENT_SPECIAL_MASK) && mantissa;
  const bool is_inf = (exponent == SourceFormat::EXPONENT_SPECIAL_MASK) && (mantissa == 0);
  const bool is_zero = (exponent == 0 && mantissa == 0);

  /* Sign conversion */
  if constexpr (!DestinationFormat::HAS_SIGN && ClampNegativeToZero) {
    if (is_signed && !is_nan) {
      return 0;
    }
  }
  if (is_zero) {
    return 0;
  }

  if (is_inf) {
    exponent = DestinationFormat::EXPONENT_SPECIAL_MASK;
  }
  else if (is_nan) {
    exponent = DestinationFormat::EXPONENT_SPECIAL_MASK;
    mantissa = DestinationFormat::MANTISSA_NAN_MASK;
  }
  else {
    /* Exponent conversion */
    exponent -= SourceFormat::EXPONENT_BIAS;
    /* Clamping when destination has lower precision. */
    if constexpr (SourceFormat::EXPONENT_LEN > DestinationFormat::EXPONENT_LEN) {
      if (exponent > DestinationFormat::EXPONENT_BIAS) {
        exponent = 0;
        mantissa = SourceFormat::MANTISSA_MASK;
      }
      else if (exponent < -DestinationFormat::EXPONENT_BIAS) {
        return 0;
      }
    }
    exponent += DestinationFormat::EXPONENT_BIAS;

    /* Mantissa conversion */
    if constexpr (SourceFormat::MANTISSA_LEN > DestinationFormat::MANTISSA_LEN) {
      mantissa = mantissa >> (SourceFormat::MANTISSA_LEN - DestinationFormat::MANTISSA_LEN);
    }
    else if constexpr (SourceFormat::MANTISSA_LEN < DestinationFormat::MANTISSA_LEN) {
      mantissa = mantissa << (DestinationFormat::MANTISSA_LEN - SourceFormat::MANTISSA_LEN);
    }
  }

  uint32_t result = 0;
  result = DestinationFormat::set_sign(is_signed, result);
  result = DestinationFormat::set_exponent(exponent, result);
  result = DestinationFormat::set_mantissa(mantissa, result);
  return result;
}

/* \} */
};  // namespace blender::gpu
