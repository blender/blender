/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_data_conversion.hh"
#include "vk_device.hh"

#include "gpu_vertex_format_private.hh"

#include "BLI_color.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Conversion types
 * \{ */

enum class ConversionType {
  /** No conversion needed, result can be directly read back to host memory. */
  PASS_THROUGH,

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

  /** Convert device 16F to floats. */
  HALF_TO_FLOAT,
  FLOAT_TO_HALF,

  FLOAT_TO_SRGBA8,
  SRGBA8_TO_FLOAT,

  FLOAT_TO_DEPTH_COMPONENT24,
  DEPTH_COMPONENT24_TO_FLOAT,

  FLOAT_TO_B10F_G11F_R11F,
  B10F_G11F_R11F_TO_FLOAT,

  FLOAT3_TO_HALF4,
  HALF4_TO_FLOAT3,

  FLOAT3_TO_FLOAT4,
  FLOAT4_TO_FLOAT3,

  UINT_TO_DEPTH_COMPONENT24,
  DEPTH_COMPONENT24_TO_UINT,
  /**
   * The requested conversion isn't supported.
   */
  UNSUPPORTED,
};

static ConversionType type_of_conversion_float(const eGPUTextureFormat host_format,
                                               const eGPUTextureFormat device_format)
{
  if (host_format != device_format) {
    if (host_format == GPU_RGB16F && device_format == GPU_RGBA16F) {
      return ConversionType::FLOAT3_TO_HALF4;
    }
    if (host_format == GPU_RGB32F && device_format == GPU_RGBA32F) {
      return ConversionType::FLOAT3_TO_FLOAT4;
    }
    if (host_format == GPU_DEPTH_COMPONENT24 && device_format == GPU_DEPTH_COMPONENT32F) {
      return ConversionType::PASS_THROUGH;
    }

    return ConversionType::UNSUPPORTED;
  }

  switch (device_format) {
    case GPU_RGBA32F:
    case GPU_RG32F:
    case GPU_R32F:
    case GPU_DEPTH_COMPONENT32F:
      return ConversionType::PASS_THROUGH;

    case GPU_RGBA16F:
    case GPU_RG16F:
    case GPU_R16F:
    case GPU_RGB16F:
      return ConversionType::FLOAT_TO_HALF;

    case GPU_RGBA8:
    case GPU_RG8:
    case GPU_R8:
      return ConversionType::FLOAT_TO_UNORM8;

    case GPU_RGBA8_SNORM:
    case GPU_RGB8_SNORM:
    case GPU_RG8_SNORM:
    case GPU_R8_SNORM:
      return ConversionType::FLOAT_TO_SNORM8;

    case GPU_RGBA16:
    case GPU_RG16:
    case GPU_R16:
      return ConversionType::FLOAT_TO_UNORM16;

    case GPU_RGBA16_SNORM:
    case GPU_RGB16_SNORM:
    case GPU_RG16_SNORM:
    case GPU_R16_SNORM:
      return ConversionType::FLOAT_TO_SNORM16;

    case GPU_SRGB8_A8:
      return ConversionType::FLOAT_TO_SRGBA8;

    case GPU_DEPTH_COMPONENT24:
      return ConversionType::FLOAT_TO_DEPTH_COMPONENT24;

    case GPU_R11F_G11F_B10F:
      return ConversionType::FLOAT_TO_B10F_G11F_R11F;

    case GPU_SRGB8_A8_DXT1:
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT1:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
      /* Not an actual "conversion", but compressed texture upload code
       * pretends that host data is a float. It is actually raw BCn bits. */
      return ConversionType::PASS_THROUGH;

    case GPU_RGB32F: /* GPU_RGB32F Not supported by vendors. */
    case GPU_RGBA8UI:
    case GPU_RGBA8I:
    case GPU_RGBA16UI:
    case GPU_RGBA16I:
    case GPU_RGBA32UI:
    case GPU_RGBA32I:
    case GPU_RG8UI:
    case GPU_RG8I:
    case GPU_RG16UI:
    case GPU_RG16I:
    case GPU_RG32UI:
    case GPU_RG32I:
    case GPU_R8UI:
    case GPU_R8I:
    case GPU_R16UI:
    case GPU_R16I:
    case GPU_R32UI:
    case GPU_R32I:
    case GPU_RGB10_A2:
    case GPU_RGB10_A2UI:
    case GPU_DEPTH32F_STENCIL8:
    case GPU_DEPTH24_STENCIL8:
    case GPU_RGB8UI:
    case GPU_RGB8I:
    case GPU_RGB8:
    case GPU_RGB16UI:
    case GPU_RGB16I:
    case GPU_RGB16:
    case GPU_RGB32UI:
    case GPU_RGB32I:
    case GPU_SRGB8:
    case GPU_RGB9_E5:
    case GPU_DEPTH_COMPONENT16:
      return ConversionType::UNSUPPORTED;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_int(eGPUTextureFormat device_format)
{
  switch (device_format) {
    case GPU_RGBA32I:
    case GPU_RG32I:
    case GPU_R32I:
      return ConversionType::PASS_THROUGH;

    case GPU_RGBA16I:
    case GPU_RG16I:
    case GPU_R16I:
      return ConversionType::I32_TO_I16;

    case GPU_RGBA8I:
    case GPU_RG8I:
    case GPU_R8I:
      return ConversionType::I32_TO_I8;

    case GPU_RGBA8UI:
    case GPU_RGBA8:
    case GPU_RGBA16UI:
    case GPU_RGBA16F:
    case GPU_RGBA16:
    case GPU_RGBA32UI:
    case GPU_RGBA32F:
    case GPU_RG8UI:
    case GPU_RG8:
    case GPU_RG16UI:
    case GPU_RG16F:
    case GPU_RG32UI:
    case GPU_RG32F:
    case GPU_RG16:
    case GPU_R8UI:
    case GPU_R8:
    case GPU_R16UI:
    case GPU_R16F:
    case GPU_R16:
    case GPU_R32UI:
    case GPU_R32F:
    case GPU_RGB10_A2:
    case GPU_RGB10_A2UI:
    case GPU_R11F_G11F_B10F:
    case GPU_DEPTH32F_STENCIL8:
    case GPU_DEPTH24_STENCIL8:
    case GPU_SRGB8_A8:
    case GPU_RGBA8_SNORM:
    case GPU_RGBA16_SNORM:
    case GPU_RGB8UI:
    case GPU_RGB8I:
    case GPU_RGB8:
    case GPU_RGB8_SNORM:
    case GPU_RGB16UI:
    case GPU_RGB16I:
    case GPU_RGB16F:
    case GPU_RGB16:
    case GPU_RGB16_SNORM:
    case GPU_RGB32UI:
    case GPU_RGB32I:
    case GPU_RGB32F:
    case GPU_RG8_SNORM:
    case GPU_RG16_SNORM:
    case GPU_R8_SNORM:
    case GPU_R16_SNORM:
    case GPU_SRGB8_A8_DXT1:
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT1:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
    case GPU_SRGB8:
    case GPU_RGB9_E5:
    case GPU_DEPTH_COMPONENT32F:
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
      return ConversionType::UNSUPPORTED;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_uint(eGPUTextureFormat device_format)
{
  switch (device_format) {
    case GPU_RGBA32UI:
    case GPU_RG32UI:
    case GPU_R32UI:
    case GPU_DEPTH_COMPONENT24:
      return ConversionType::PASS_THROUGH;

    case GPU_RGBA16UI:
    case GPU_RG16UI:
    case GPU_R16UI:
    case GPU_RGB16UI:
      return ConversionType::UI32_TO_UI16;

    case GPU_RGBA8UI:
    case GPU_RG8UI:
    case GPU_R8UI:
      return ConversionType::UI32_TO_UI8;

    case GPU_DEPTH_COMPONENT32F:
    case GPU_DEPTH32F_STENCIL8:
      return ConversionType::UNORM32_TO_FLOAT;
    case GPU_DEPTH24_STENCIL8:
      return ConversionType::UINT_TO_DEPTH_COMPONENT24;
    case GPU_RGBA8I:
    case GPU_RGBA8:
    case GPU_RGBA16I:
    case GPU_RGBA16F:
    case GPU_RGBA16:
    case GPU_RGBA32I:
    case GPU_RGBA32F:
    case GPU_RG8I:
    case GPU_RG8:
    case GPU_RG16I:
    case GPU_RG16F:
    case GPU_RG16:
    case GPU_RG32I:
    case GPU_RG32F:
    case GPU_R8I:
    case GPU_R8:
    case GPU_R16I:
    case GPU_R16F:
    case GPU_R16:
    case GPU_R32I:
    case GPU_R32F:
    case GPU_RGB10_A2:
    case GPU_RGB10_A2UI:
    case GPU_R11F_G11F_B10F:
    case GPU_SRGB8_A8:
    case GPU_RGBA8_SNORM:
    case GPU_RGBA16_SNORM:
    case GPU_RGB8UI:
    case GPU_RGB8I:
    case GPU_RGB8:
    case GPU_RGB8_SNORM:
    case GPU_RGB16I:
    case GPU_RGB16F:
    case GPU_RGB16:
    case GPU_RGB16_SNORM:
    case GPU_RGB32UI:
    case GPU_RGB32I:
    case GPU_RGB32F:
    case GPU_RG8_SNORM:
    case GPU_RG16_SNORM:
    case GPU_R8_SNORM:
    case GPU_R16_SNORM:
    case GPU_SRGB8_A8_DXT1:
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT1:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
    case GPU_SRGB8:
    case GPU_RGB9_E5:
    case GPU_DEPTH_COMPONENT16:
      return ConversionType::UNSUPPORTED;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_half(eGPUTextureFormat device_format)
{
  switch (device_format) {
    case GPU_RGBA16F:
    case GPU_RG16F:
    case GPU_R16F:
      return ConversionType::PASS_THROUGH;

    case GPU_RGBA8UI:
    case GPU_RGBA8I:
    case GPU_RGBA8:
    case GPU_RGBA16UI:
    case GPU_RGBA16I:
    case GPU_RGBA16:
    case GPU_RGBA32UI:
    case GPU_RGBA32I:
    case GPU_RGBA32F:
    case GPU_RG8UI:
    case GPU_RG8I:
    case GPU_RG8:
    case GPU_RG16UI:
    case GPU_RG16I:
    case GPU_RG16:
    case GPU_RG32UI:
    case GPU_RG32I:
    case GPU_RG32F:
    case GPU_R8UI:
    case GPU_R8I:
    case GPU_R8:
    case GPU_R16UI:
    case GPU_R16I:
    case GPU_R16:
    case GPU_R32UI:
    case GPU_R32I:
    case GPU_R32F:
    case GPU_RGB10_A2:
    case GPU_RGB10_A2UI:
    case GPU_R11F_G11F_B10F:
    case GPU_DEPTH32F_STENCIL8:
    case GPU_DEPTH24_STENCIL8:
    case GPU_SRGB8_A8:
    case GPU_RGBA8_SNORM:
    case GPU_RGBA16_SNORM:
    case GPU_RGB8UI:
    case GPU_RGB8I:
    case GPU_RGB8:
    case GPU_RGB8_SNORM:
    case GPU_RGB16UI:
    case GPU_RGB16I:
    case GPU_RGB16F:
    case GPU_RGB16:
    case GPU_RGB16_SNORM:
    case GPU_RGB32UI:
    case GPU_RGB32I:
    case GPU_RGB32F:
    case GPU_RG8_SNORM:
    case GPU_RG16_SNORM:
    case GPU_R8_SNORM:
    case GPU_R16_SNORM:
    case GPU_SRGB8_A8_DXT1:
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT1:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
    case GPU_SRGB8:
    case GPU_RGB9_E5:
    case GPU_DEPTH_COMPONENT32F:
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
      return ConversionType::UNSUPPORTED;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_ubyte(eGPUTextureFormat device_format)
{
  switch (device_format) {
    case GPU_RGBA8UI:
    case GPU_RGBA8:
    case GPU_RG8UI:
    case GPU_RG8:
    case GPU_R8UI:
    case GPU_R8:
    case GPU_SRGB8_A8:
      return ConversionType::PASS_THROUGH;

    case GPU_RGBA8I:
    case GPU_RGBA16UI:
    case GPU_RGBA16I:
    case GPU_RGBA16F:
    case GPU_RGBA16:
    case GPU_RGBA32UI:
    case GPU_RGBA32I:
    case GPU_RGBA32F:
    case GPU_RG8I:
    case GPU_RG16UI:
    case GPU_RG16I:
    case GPU_RG16F:
    case GPU_RG16:
    case GPU_RG32UI:
    case GPU_RG32I:
    case GPU_RG32F:
    case GPU_R8I:
    case GPU_R16UI:
    case GPU_R16I:
    case GPU_R16F:
    case GPU_R16:
    case GPU_R32UI:
    case GPU_R32I:
    case GPU_R32F:
    case GPU_RGB10_A2:
    case GPU_RGB10_A2UI:
    case GPU_R11F_G11F_B10F:
    case GPU_DEPTH32F_STENCIL8:
    case GPU_DEPTH24_STENCIL8:
    case GPU_RGBA8_SNORM:
    case GPU_RGBA16_SNORM:
    case GPU_RGB8UI:
    case GPU_RGB8I:
    case GPU_RGB8:
    case GPU_RGB8_SNORM:
    case GPU_RGB16UI:
    case GPU_RGB16I:
    case GPU_RGB16F:
    case GPU_RGB16:
    case GPU_RGB16_SNORM:
    case GPU_RGB32UI:
    case GPU_RGB32I:
    case GPU_RGB32F:
    case GPU_RG8_SNORM:
    case GPU_RG16_SNORM:
    case GPU_R8_SNORM:
    case GPU_R16_SNORM:
    case GPU_SRGB8_A8_DXT1:
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT1:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
    case GPU_SRGB8:
    case GPU_RGB9_E5:
    case GPU_DEPTH_COMPONENT32F:
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
      return ConversionType::UNSUPPORTED;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_r11g11b10(eGPUTextureFormat device_format)
{
  if (device_format == GPU_R11F_G11F_B10F) {
    return ConversionType::PASS_THROUGH;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType type_of_conversion_r10g10b10a2(eGPUTextureFormat device_format)
{
  if (ELEM(device_format, GPU_RGB10_A2, GPU_RGB10_A2UI)) {
    return ConversionType::PASS_THROUGH;
  }
  return ConversionType::UNSUPPORTED;
}

static ConversionType host_to_device(const eGPUDataFormat host_format,
                                     const eGPUTextureFormat host_texture_format,
                                     const eGPUTextureFormat device_format)
{
  BLI_assert(validate_data_format(device_format, host_format));

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

    case GPU_DATA_UINT_24_8:
      return ConversionType::UNSUPPORTED;
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
      CASE_PAIR(FLOAT, SRGBA8)
      CASE_PAIR(FLOAT, DEPTH_COMPONENT24)
      CASE_PAIR(UINT, DEPTH_COMPONENT24)
      CASE_PAIR(FLOAT, B10F_G11F_R11F)
      CASE_PAIR(FLOAT3, HALF4)
      CASE_PAIR(FLOAT3, FLOAT4)

    case ConversionType::UNSUPPORTED:
      return ConversionType::UNSUPPORTED;
  }

#undef CASE_PAIR
#undef CASE_SINGLE

  return ConversionType::UNSUPPORTED;
}

/* \} */

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
using F32 = ComponentValue<float>;
using F16 = ComponentValue<uint16_t>;
using SRGBA8 = PixelValue<ColorSceneLinearByteEncoded4b<eAlpha::Premultiplied>>;
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

class DepthComponent24 : public ComponentValue<uint32_t> {
 public:
  operator uint32_t() const
  {
    return value;
  }

  DepthComponent24 &operator=(uint32_t new_value)
  {
    value = new_value;
    return *this;
  }

  /* Depth component24 are 4 bytes, but 1 isn't used. */
  static constexpr size_t used_byte_size()
  {
    return 3;
  }
};

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
                std::is_same<InnerType, uint32_t>() ||
                std::is_same<InnerType, DepthComponent24>());
  InnerType value;

  static constexpr size_t used_byte_size()
  {
    if constexpr (std::is_same<InnerType, DepthComponent24>()) {
      return InnerType::used_byte_size();
    }
    else {
      return sizeof(InnerType);
    }
  }

  static constexpr uint32_t scalar()
  {
    if constexpr (std::is_same<InnerType, DepthComponent24>()) {
      return (1 << (used_byte_size() * 8)) - 1;
    }
    else {
      return std::numeric_limits<InnerType>::max();
    }
  }

  static constexpr uint32_t max()
  {
    if constexpr (std::is_same<InnerType, DepthComponent24>()) {
      return (1 << (used_byte_size() * 8)) - 1;
    }
    else {
      return std::numeric_limits<InnerType>::max();
    }
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
  dst.value = clamp_f((src.value * float(scalar)), 0, float(max));
}

template<typename StorageType> void convert(F32 &dst, const UnsignedNormalized<StorageType> &src)
{
  static constexpr uint32_t scalar = UnsignedNormalized<StorageType>::scalar();
  dst.value = float(uint32_t(src.value)) / float(scalar);
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

static void convert(F16 &dst, const F32 &src)
{
  dst.value = convert_float_formats<FormatF16, FormatF32>(float_to_uint32_t(src.value));
}

static void convert(F32 &dst, const F16 &src)
{
  dst.value = uint32_t_to_float(convert_float_formats<FormatF32, FormatF16>(src.value));
}

static void convert(SRGBA8 &dst, const FLOAT4 &src)
{
  dst.value = src.value.encode();
}

static void convert(FLOAT4 &dst, const SRGBA8 &src)
{
  dst.value = src.value.decode();
}

static void convert(FLOAT3 &dst, const HALF4 &src)
{
  dst.value.x = uint32_t_to_float(convert_float_formats<FormatF32, FormatF16>(src.get_r()));
  dst.value.y = uint32_t_to_float(convert_float_formats<FormatF32, FormatF16>(src.get_g()));
  dst.value.z = uint32_t_to_float(convert_float_formats<FormatF32, FormatF16>(src.get_b()));
}

static void convert(HALF4 &dst, const FLOAT3 &src)
{
  dst.set_r(convert_float_formats<FormatF16, FormatF32>(float_to_uint32_t(src.value.x)));
  dst.set_g(convert_float_formats<FormatF16, FormatF32>(float_to_uint32_t(src.value.y)));
  dst.set_b(convert_float_formats<FormatF16, FormatF32>(float_to_uint32_t(src.value.z)));
  dst.set_a(convert_float_formats<FormatF16, FormatF32>(float_to_uint32_t(1.0f)));
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

/* \} */

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
                           eGPUTextureFormat device_format)
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
                           eGPUTextureFormat device_format,
                           ConversionType type)
{
  switch (type) {
    case ConversionType::UNSUPPORTED:
      return;

    case ConversionType::PASS_THROUGH:
    case ConversionType::UINT_TO_DEPTH_COMPONENT24:
      memcpy(dst_memory, src_memory, buffer_size * to_bytesize(device_format));
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

    case ConversionType::FLOAT_TO_HALF:
      convert_per_component<F16, F32>(dst_memory, src_memory, buffer_size, device_format);
      break;
    case ConversionType::HALF_TO_FLOAT:
      convert_per_component<F32, F16>(dst_memory, src_memory, buffer_size, device_format);
      break;

    case ConversionType::FLOAT_TO_SRGBA8:
      convert_per_pixel<SRGBA8, FLOAT4>(dst_memory, src_memory, buffer_size);
      break;
    case ConversionType::SRGBA8_TO_FLOAT:
      convert_per_pixel<FLOAT4, SRGBA8>(dst_memory, src_memory, buffer_size);
      break;

    case ConversionType::FLOAT_TO_DEPTH_COMPONENT24:
      convert_per_component<UnsignedNormalized<DepthComponent24>, F32>(
          dst_memory, src_memory, buffer_size, device_format);
      break;
    case ConversionType::DEPTH_COMPONENT24_TO_FLOAT:
      convert_per_component<F32, UnsignedNormalized<DepthComponent24>>(
          dst_memory, src_memory, buffer_size, device_format);
      break;
    case ConversionType::DEPTH_COMPONENT24_TO_UINT:
      convert_per_component<UI32, UnsignedNormalized<DepthComponent24>>(
          dst_memory, src_memory, buffer_size, device_format);
      break;
    case ConversionType::FLOAT_TO_B10F_G11F_R11F:
      convert_per_pixel<B10F_G11G_R11F, FLOAT3>(dst_memory, src_memory, buffer_size);
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
                            eGPUTextureFormat host_texture_format,
                            eGPUTextureFormat device_format)
{
  ConversionType conversion_type = host_to_device(host_format, host_texture_format, device_format);
  BLI_assert(conversion_type != ConversionType::UNSUPPORTED);
  convert_buffer(dst_buffer, src_buffer, buffer_size, device_format, conversion_type);
}

void convert_device_to_host(void *dst_buffer,
                            const void *src_buffer,
                            size_t buffer_size,
                            eGPUDataFormat host_format,
                            eGPUTextureFormat host_texture_format,
                            eGPUTextureFormat device_format)
{
  ConversionType conversion_type = reversed(
      host_to_device(host_format, host_texture_format, device_format));
  BLI_assert_msg(conversion_type != ConversionType::UNSUPPORTED,
                 "Data conversion between host_format and device_format isn't supported (yet).");
  convert_buffer(dst_buffer, src_buffer, buffer_size, device_format, conversion_type);
}

/* \} */

/* -------------------------------------------------------------------- */
/** \name Vertex Attributes
 * \{ */

static bool attribute_check(const GPUVertAttr attribute,
                            GPUVertCompType comp_type,
                            GPUVertFetchMode fetch_mode)
{
  return attribute.comp_type == comp_type && attribute.fetch_mode == fetch_mode;
}

static bool attribute_check(const GPUVertAttr attribute, GPUVertCompType comp_type, uint comp_len)
{
  return attribute.comp_type == comp_type && attribute.comp_len == comp_len;
}

void VertexFormatConverter::reset()
{
  source_format_ = nullptr;
  device_format_ = nullptr;
  GPU_vertformat_clear(&converted_format_);

  needs_conversion_ = false;
}

bool VertexFormatConverter::is_initialized() const
{
  return device_format_ != nullptr;
}

void VertexFormatConverter::init(const GPUVertFormat *vertex_format,
                                 const VKWorkarounds &workarounds)
{
  source_format_ = vertex_format;
  device_format_ = vertex_format;

  update_conversion_flags(*source_format_, workarounds);
  if (needs_conversion_) {
    init_device_format(workarounds);
  }
}

const GPUVertFormat &VertexFormatConverter::device_format_get() const
{
  BLI_assert(is_initialized());
  return *device_format_;
}

bool VertexFormatConverter::needs_conversion() const
{
  BLI_assert(is_initialized());
  return needs_conversion_;
}

void VertexFormatConverter::update_conversion_flags(const GPUVertFormat &vertex_format,
                                                    const VKWorkarounds &workarounds)
{
  needs_conversion_ = false;

  for (int attr_index : IndexRange(vertex_format.attr_len)) {
    const GPUVertAttr &vert_attr = vertex_format.attrs[attr_index];
    update_conversion_flags(vert_attr, workarounds);
  }
}

void VertexFormatConverter::update_conversion_flags(const GPUVertAttr &vertex_attribute,
                                                    const VKWorkarounds &workarounds)
{
  /* I32/U32 to F32 conversion doesn't exist in vulkan. */
  if (vertex_attribute.fetch_mode == GPU_FETCH_INT_TO_FLOAT &&
      ELEM(vertex_attribute.comp_type, GPU_COMP_I32, GPU_COMP_U32))
  {
    needs_conversion_ = true;
  }
  /* r8g8b8 formats will be stored as r8g8b8a8. */
  else if (workarounds.vertex_formats.r8g8b8 && attribute_check(vertex_attribute, GPU_COMP_U8, 3))
  {
    needs_conversion_ = true;
  }
}

void VertexFormatConverter::init_device_format(const VKWorkarounds &workarounds)
{
  BLI_assert(needs_conversion_);
  GPU_vertformat_copy(&converted_format_, source_format_);
  bool needs_repack = false;

  for (int attr_index : IndexRange(converted_format_.attr_len)) {
    GPUVertAttr &vert_attr = converted_format_.attrs[attr_index];
    make_device_compatible(vert_attr, workarounds, needs_repack);
  }

  if (needs_repack) {
    VertexFormat_pack(&converted_format_);
  }
  device_format_ = &converted_format_;
}

void VertexFormatConverter::make_device_compatible(GPUVertAttr &vertex_attribute,
                                                   const VKWorkarounds &workarounds,
                                                   bool &r_needs_repack) const
{
  if (vertex_attribute.fetch_mode == GPU_FETCH_INT_TO_FLOAT &&
      ELEM(vertex_attribute.comp_type, GPU_COMP_I32, GPU_COMP_U32))
  {
    vertex_attribute.fetch_mode = GPU_FETCH_FLOAT;
    vertex_attribute.comp_type = GPU_COMP_F32;
  }
  else if (workarounds.vertex_formats.r8g8b8 && attribute_check(vertex_attribute, GPU_COMP_U8, 3))
  {
    vertex_attribute.comp_len = 4;
    vertex_attribute.size = 4;
    r_needs_repack = true;
  }
}

void VertexFormatConverter::convert(void *device_data,
                                    const void *source_data,
                                    const uint vertex_len) const
{
  BLI_assert(needs_conversion_);
  if (source_data != device_data) {
    memcpy(device_data, source_data, device_format_->stride * vertex_len);
  }

  const void *source_row_data = static_cast<const uint8_t *>(source_data);
  void *device_row_data = static_cast<uint8_t *>(device_data);
  for (int vertex_index : IndexRange(vertex_len)) {
    UNUSED_VARS(vertex_index);
    convert_row(device_row_data, source_row_data);
    source_row_data = static_cast<const uint8_t *>(source_row_data) + source_format_->stride;
    device_row_data = static_cast<uint8_t *>(device_row_data) + device_format_->stride;
  }
}

void VertexFormatConverter::convert_row(void *device_row_data, const void *source_row_data) const
{
  for (int attr_index : IndexRange(source_format_->attr_len)) {
    const GPUVertAttr &device_attribute = device_format_->attrs[attr_index];
    const GPUVertAttr &source_attribute = source_format_->attrs[attr_index];
    convert_attribute(device_row_data, source_row_data, device_attribute, source_attribute);
  }
}

void VertexFormatConverter::convert_attribute(void *device_row_data,
                                              const void *source_row_data,
                                              const GPUVertAttr &device_attribute,
                                              const GPUVertAttr &source_attribute) const
{
  const void *source_attr_data = static_cast<const uint8_t *>(source_row_data) +
                                 source_attribute.offset;
  void *device_attr_data = static_cast<uint8_t *>(device_row_data) + device_attribute.offset;
  if (source_attribute.comp_len == device_attribute.comp_len &&
      source_attribute.comp_type == device_attribute.comp_type &&
      source_attribute.fetch_mode == device_attribute.fetch_mode)
  {
    /* This check is done first to improve possible branch prediction. */
  }
  else if (attribute_check(source_attribute, GPU_COMP_I32, GPU_FETCH_INT_TO_FLOAT) &&
           attribute_check(device_attribute, GPU_COMP_F32, GPU_FETCH_FLOAT))
  {
    for (int component : IndexRange(source_attribute.comp_len)) {
      const int32_t *component_in = static_cast<const int32_t *>(source_attr_data) + component;
      float *component_out = static_cast<float *>(device_attr_data) + component;
      *component_out = float(*component_in);
    }
  }
  else if (attribute_check(source_attribute, GPU_COMP_U32, GPU_FETCH_INT_TO_FLOAT) &&
           attribute_check(device_attribute, GPU_COMP_F32, GPU_FETCH_FLOAT))
  {
    for (int component : IndexRange(source_attribute.comp_len)) {
      const uint32_t *component_in = static_cast<const uint32_t *>(source_attr_data) + component;
      float *component_out = static_cast<float *>(device_attr_data) + component;
      *component_out = float(*component_in);
    }
  }
  else if (attribute_check(source_attribute, GPU_COMP_U8, 3) &&
           attribute_check(device_attribute, GPU_COMP_U8, 4))
  {
    const uchar3 *attr_in = static_cast<const uchar3 *>(source_attr_data);
    uchar4 *attr_out = static_cast<uchar4 *>(device_attr_data);
    *attr_out = uchar4(attr_in->x, attr_in->y, attr_in->z, 255);
  }
  else {
    BLI_assert_unreachable();
  }
}

/* \} */

}  // namespace blender::gpu
