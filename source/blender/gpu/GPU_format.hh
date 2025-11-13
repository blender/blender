/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_assert.h"
#include "BLI_sys_types.h"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Standard Formats
 * \{ */

/* NOTE: Metal does not support pixel formats with 3 channel. These are aliased to 4 channel types
 * and converted on data upload. */
/* clang-format off */
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define SNORM_8_(impl)                  impl(/*TODO*/,  1 * 1,  1,    SNORM_8,             R8_SNORM,             R8Snorm,          CharNormalized,      R8_SNORM,       r8_snorm)
#define SNORM_8_8_(impl)                impl(/*TODO*/,  1 * 2,  2,    SNORM_8_8,           R8G8_SNORM,           RG8Snorm,         Char2Normalized,     RG8_SNORM,      rg8_snorm)
#define SNORM_8_8_8_(impl)              impl(/*TODO*/,  1 * 3,  3,    SNORM_8_8_8,         R8G8B8_SNORM,         RGBA8Snorm,       Char3Normalized,     RGB8_SNORM,     rgb8_snorm)
#define SNORM_8_8_8_8_(impl)            impl(/*TODO*/,  1 * 4,  4,    SNORM_8_8_8_8,       R8G8B8A8_SNORM,       RGBA8Snorm,       Char4Normalized,     RGBA8_SNORM,    rgba8_snorm)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define SNORM_16_(impl)                 impl(/*TODO*/,  2 * 1,  1,    SNORM_16,            R16_SNORM,            R16Snorm,         ShortNormalized,     R16_SNORM,      r16_snorm)
#define SNORM_16_16_(impl)              impl(/*TODO*/,  2 * 2,  2,    SNORM_16_16,         R16G16_SNORM,         RG16Snorm,        Short2Normalized,    RG16_SNORM,     rg16_snorm)
#define SNORM_16_16_16_(impl)           impl(/*TODO*/,  2 * 3,  3,    SNORM_16_16_16,      R16G16B16_SNORM,      RGBA16Snorm,      Short3Normalized,    RGB16_SNORM,    rgb16_snorm)
#define SNORM_16_16_16_16_(impl)        impl(/*TODO*/,  2 * 4,  4,    SNORM_16_16_16_16,   R16G16B16A16_SNORM,   RGBA16Snorm,      Short4Normalized,    RGBA16_SNORM,   rgba16_snorm)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define UNORM_8_(impl)                  impl(/*TODO*/,  1 * 1,  1,    UNORM_8,             R8_UNORM,             R8Unorm,          UCharNormalized,     R8,             r8_unorm)
#define UNORM_8_8_(impl)                impl(/*TODO*/,  1 * 2,  2,    UNORM_8_8,           R8G8_UNORM,           RG8Unorm,         UChar2Normalized,    RG8,            rg8_unorm)
#define UNORM_8_8_8_(impl)              impl(/*TODO*/,  1 * 3,  3,    UNORM_8_8_8,         R8G8B8_UNORM,         RGBA8Unorm,       UChar3Normalized,    RGB8,           rgb8_unorm)
#define UNORM_8_8_8_8_(impl)            impl(/*TODO*/,  1 * 4,  4,    UNORM_8_8_8_8,       R8G8B8A8_UNORM,       RGBA8Unorm,       UChar4Normalized,    RGBA8,          rgba8_unorm)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define UNORM_16_(impl)                 impl(/*TODO*/,  2 * 1,  1,    UNORM_16,            R16_UNORM,            R16Unorm,         UShortNormalized,    R16,            r16_unorm)
#define UNORM_16_16_(impl)              impl(/*TODO*/,  2 * 2,  2,    UNORM_16_16,         R16G16_UNORM,         RG16Unorm,        UShort2Normalized,   RG16,           rg16_unorm)
#define UNORM_16_16_16_(impl)           impl(/*TODO*/,  2 * 3,  3,    UNORM_16_16_16,      R16G16B16_UNORM,      RGBA16Unorm,      UShort3Normalized,   RGB16,          rgb16_unorm)
#define UNORM_16_16_16_16_(impl)        impl(/*TODO*/,  2 * 4,  4,    UNORM_16_16_16_16,   R16G16B16A16_UNORM,   RGBA16Unorm,      UShort4Normalized,   RGBA16,         rgba16_unorm)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define SINT_8_(impl)                   impl(int8_t,    1 * 1,  1,    SINT_8,              R8_SINT,              R8Sint,           Char,                R8I,            r8_sint)
#define SINT_8_8_(impl)                 impl(char2,     1 * 2,  2,    SINT_8_8,            R8G8_SINT,            RG8Sint,          Char2,               RG8I,           rg8_sint)
#define SINT_8_8_8_(impl)               impl(char3,     1 * 3,  3,    SINT_8_8_8,          R8G8B8_SINT,          RGBA8Sint,        Char3,               RGB8I,          rgb8_sint)
#define SINT_8_8_8_8_(impl)             impl(char4,     1 * 4,  4,    SINT_8_8_8_8,        R8G8B8A8_SINT,        RGBA8Sint,        Char4,               RGBA8I,         rgba8_sint)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define SINT_16_(impl)                  impl(int16_t,   2 * 1,  1,    SINT_16,             R16_SINT,             R16Sint,          Short,               R16I,           r16_sint)
#define SINT_16_16_(impl)               impl(short2,    2 * 2,  2,    SINT_16_16,          R16G16_SINT,          RG16Sint,         Short2,              RG16I,          rg16_sint)
#define SINT_16_16_16_(impl)            impl(short3,    2 * 3,  3,    SINT_16_16_16,       R16G16B16_SINT,       RGBA16Sint,       Short3,              RGB16I,         rgb16_sint)
#define SINT_16_16_16_16_(impl)         impl(short4,    2 * 4,  4,    SINT_16_16_16_16,    R16G16B16A16_SINT,    RGBA16Sint,       Short4,              RGBA16I,        rgba16_sint)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define SINT_32_(impl)                  impl(int32_t,   4 * 1,  1,    SINT_32,             R32_SINT,             R32Sint,          Int,                 R32I,           r32_sint)
#define SINT_32_32_(impl)               impl(int2,      4 * 2,  2,    SINT_32_32,          R32G32_SINT,          RG32Sint,         Int2,                RG32I,          rg32_sint)
#define SINT_32_32_32_(impl)            impl(int3,      4 * 3,  3,    SINT_32_32_32,       R32G32B32_SINT,       RGBA32Sint,       Int3,                RGB32I,         rgb32_sint)
#define SINT_32_32_32_32_(impl)         impl(int4,      4 * 4,  4,    SINT_32_32_32_32,    R32G32B32A32_SINT,    RGBA32Sint,       Int4,                RGBA32I,        rgba32_sint)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define UINT_8_(impl)                   impl(uint8_t,   1 * 1,  1,    UINT_8,              R8_UINT,              R8Uint,           UChar,               R8UI,           r8_uint)
#define UINT_8_8_(impl)                 impl(uchar2,    1 * 2,  2,    UINT_8_8,            R8G8_UINT,            RG8Uint,          UChar2,              RG8UI,          rg8_uint)
#define UINT_8_8_8_(impl)               impl(uchar3,    1 * 3,  3,    UINT_8_8_8,          R8G8B8_UINT,          RGBA8Uint,        UChar3,              RGB8UI,         rgb8_uint)
#define UINT_8_8_8_8_(impl)             impl(uchar4,    1 * 4,  4,    UINT_8_8_8_8,        R8G8B8A8_UINT,        RGBA8Uint,        UChar4,              RGBA8UI,        rgba8_uint)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define UINT_16_(impl)                  impl(uint16_t,  2 * 1,  1,    UINT_16,             R16_UINT,             R16Uint,          UShort,              R16UI,          r16_uint)
#define UINT_16_16_(impl)               impl(ushort2,   2 * 2,  2,    UINT_16_16,          R16G16_UINT,          RG16Uint,         UShort2,             RG16UI,         rg16_uint)
#define UINT_16_16_16_(impl)            impl(ushort3,   2 * 3,  3,    UINT_16_16_16,       R16G16B16_UINT,       RGBA16Uint,       UShort3,             RGB16UI,        rgb16_uint)
#define UINT_16_16_16_16_(impl)         impl(ushort4,   2 * 4,  4,    UINT_16_16_16_16,    R16G16B16A16_UINT,    RGBA16Uint,       UShort4,             RGBA16UI,       rgba16_uint)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define UINT_32_(impl)                  impl(uint32_t,  4 * 1,  1,    UINT_32,             R32_UINT,             R32Uint,          UInt,                R32UI,          r32_uint)
#define UINT_32_32_(impl)               impl(uint2,     4 * 2,  2,    UINT_32_32,          R32G32_UINT,          RG32Uint,         UInt2,               RG32UI,         rg32_uint)
#define UINT_32_32_32_(impl)            impl(uint3,     4 * 3,  3,    UINT_32_32_32,       R32G32B32_UINT,       RGBA32Uint,       UInt3,               RGB32UI,        rgb32_uint)
#define UINT_32_32_32_32_(impl)         impl(uint4,     4 * 4,  4,    UINT_32_32_32_32,    R32G32B32A32_UINT,    RGBA32Uint,       UInt4,               RGBA32UI,       rgba32_uint)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define SFLOAT_16_(impl)                impl(/*TODO*/,  2 * 1,  1,    SFLOAT_16,           R16_SFLOAT,           R16Float,         Half,                R16F,           r16_sfloat)
#define SFLOAT_16_16_(impl)             impl(/*TODO*/,  2 * 2,  2,    SFLOAT_16_16,        R16G16_SFLOAT,        RG16Float,        Half2,               RG16F,          rg16_sfloat)
#define SFLOAT_16_16_16_(impl)          impl(/*TODO*/,  2 * 3,  3,    SFLOAT_16_16_16,     R16G16B16_SFLOAT,     RGBA16Float,      Half3,               RGB16F,         rgb16_sfloat)
#define SFLOAT_16_16_16_16_(impl)       impl(/*TODO*/,  2 * 4,  4,    SFLOAT_16_16_16_16,  R16G16B16A16_SFLOAT,  RGBA16Float,      Half4,               RGBA16F,        rgba16_sfloat)
/*                                           type       size    comps blender_enum         vk_enum               mtl_pixel_enum    mtl_vertex_enum      gl_pixel_enum   shader_enum  */
#define SFLOAT_32_(impl)                impl(float,     4 * 1,  1,    SFLOAT_32,           R32_SFLOAT,           R32Float,         Float,               R32F,           r32_sfloat)
#define SFLOAT_32_32_(impl)             impl(float2,    4 * 2,  2,    SFLOAT_32_32,        R32G32_SFLOAT,        RG32Float,        Float2,              RG32F,          rg32_sfloat)
#define SFLOAT_32_32_32_(impl)          impl(float3,    4 * 3,  3,    SFLOAT_32_32_32,     R32G32B32_SFLOAT,     RGBA32Float,      Float3,              RGB32F,         rgb32_sfloat)
#define SFLOAT_32_32_32_32_(impl)       impl(float4,    4 * 4,  4,    SFLOAT_32_32_32_32,  R32G32B32A32_SFLOAT,  RGBA32Float,      Float4,              RGBA32F,        rgba32_sfloat)

/* clang-format on */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special Formats
 * \{ */

/* clang-format off */
/*                                           type       size comps blender_enum            vk_enum                   mtl_pixel_enum         mtl_vertex_enum        gl_pixel_enum                        shader_enum  */
#define SNORM_10_10_10_2_(impl)         impl(/*TODO*/,  4,   4,    SNORM_10_10_10_2,       A2B10G10R10_SNORM_PACK32, /* n/a */,             Int1010102Normalized,  /* n/a */,                           /* n/a */       )
#define UNORM_10_10_10_2_(impl)         impl(/*TODO*/,  4,   4,    UNORM_10_10_10_2,       A2B10G10R10_UNORM_PACK32, RGB10A2Unorm,          UInt1010102Normalized, RGB10_A2,                            rgb10_a2_unorm  )
#define UINT_10_10_10_2_(impl)          impl(/*TODO*/,  4,   4,    UINT_10_10_10_2,        A2B10G10R10_UINT_PACK32,  RGB10A2Uint,           /* n/a */,             RGB10_A2UI,                          rgb10_a2_uint   )
/*                                           type       size comps blender_enum            vk_enum                   mtl_pixel_enum         mtl_vertex_enum        gl_pixel_enum                        shader_enum  */
#define UFLOAT_11_11_10_(impl)          impl(/*TODO*/,  4,   3,    UFLOAT_11_11_10,        B10G11R11_UFLOAT_PACK32,  RG11B10Float,          FloatRG11B10,          R11F_G11F_B10F,                      r11_g11_b10_ufloat)
#define UFLOAT_9_9_9_EXP_5_(impl)       impl(/*TODO*/,  4,   3,    UFLOAT_9_9_9_EXP_5,     E5B9G9R9_UFLOAT_PACK32,   RGB9E5Float,           FloatRGB9E5,           RGB9_E5,                             /* n/a */)
/*                                           type       size comps blender_enum            vk_enum                   mtl_pixel_enum         mtl_vertex_enum        gl_pixel_enum                        shader_enum  */
#define SRGBA_8_8_8_8_(impl)            impl(/*TODO*/,  4,   4,    SRGBA_8_8_8_8,          R8G8B8A8_SRGB,            RGBA8Unorm_sRGB,       /* n/a */,             SRGB8_ALPHA8,                        /* n/a */   )
#define SRGBA_8_8_8_(impl)              impl(/*TODO*/,  3,   3,    SRGBA_8_8_8,            R8G8B8_SRGB,              RGBA8Unorm_sRGB,       /* n/a */,             SRGB8,                               /* n/a */   )
/*                                           type       size comps blender_enum            vk_enum                   mtl_pixel_enum         mtl_vertex_enum        gl_pixel_enum                        shader_enum  */
#define UNORM_16_DEPTH_(impl)           impl(/*TODO*/,  4,   1,    UNORM_16_DEPTH,         D16_UNORM,                Depth16Unorm,          /* n/a */,             DEPTH_COMPONENT16,                   /* n/a */   )
#define UNORM_24_DEPTH_(impl)           impl(/*TODO*/,  4,   1,    UNORM_24_DEPTH,         X8_D24_UNORM_PACK32,      Depth24Unorm_Stencil8, /* n/a */,             DEPTH_COMPONENT24,                   /* n/a */   )
#define UNORM_24_DEPTH_UINT_8_(impl)    impl(/*TODO*/,  8,   1,    UNORM_24_DEPTH_UINT_8,  D24_UNORM_S8_UINT,        Depth24Unorm_Stencil8, /* n/a */,             DEPTH24_STENCIL8,                    /* n/a */   )
#define SFLOAT_32_DEPTH_(impl)          impl(/*TODO*/,  4,   1,    SFLOAT_32_DEPTH,        D32_SFLOAT,               Depth32Float,          /* n/a */,             DEPTH_COMPONENT32F,                  /* n/a */   )
#define SFLOAT_32_DEPTH_UINT_8_(impl)   impl(/*TODO*/,  8,   1,    SFLOAT_32_DEPTH_UINT_8, D32_SFLOAT_S8_UINT,       Depth32Float_Stencil8, /* n/a */,             DEPTH32F_STENCIL8,                   /* n/a */   )
/*                                           type       size comps blender_enum            vk_enum                   mtl_pixel_enum         mtl_vertex_enum        gl_pixel_enum                        shader_enum  */
#define SNORM_DXT1_(impl)               impl(/* n/a */, 1,   1,    SNORM_DXT1,             BC1_RGBA_UNORM_BLOCK,     BC1_RGBA,              /* n/a */,             COMPRESSED_RGBA_S3TC_DXT1_EXT,       /* n/a */   )
#define SNORM_DXT3_(impl)               impl(/* n/a */, 1,   1,    SNORM_DXT3,             BC2_UNORM_BLOCK,          BC2_RGBA,              /* n/a */,             COMPRESSED_RGBA_S3TC_DXT3_EXT,       /* n/a */   )
#define SNORM_DXT5_(impl)               impl(/* n/a */, 1,   1,    SNORM_DXT5,             BC3_UNORM_BLOCK,          BC3_RGBA,              /* n/a */,             COMPRESSED_RGBA_S3TC_DXT5_EXT,       /* n/a */   )
#define SRGB_DXT1_(impl)                impl(/* n/a */, 1,   1,    SRGB_DXT1,              BC1_RGBA_SRGB_BLOCK,      BC1_RGBA_sRGB,         /* n/a */,             COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, /* n/a */   )
#define SRGB_DXT3_(impl)                impl(/* n/a */, 1,   1,    SRGB_DXT3,              BC2_SRGB_BLOCK,           BC2_RGBA_sRGB,         /* n/a */,             COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, /* n/a */   )
#define SRGB_DXT5_(impl)                impl(/* n/a */, 1,   1,    SRGB_DXT5,              BC3_SRGB_BLOCK,           BC3_RGBA_sRGB,         /* n/a */,             COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, /* n/a */   )
/* clang-format on */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Formats
 * \{ */

/**
 * Collection of all data formats.
 * Vertex formats and Texture formats are both subset of this.
 */
enum class DataFormat : uint8_t {
  Invalid = 0,

#define DECLARE(a, b, c, blender_enum, d, e, f, g, h) blender_enum,

#define GPU_DATA_FORMAT_EXPAND(impl) \
  SNORM_8_(impl) \
  SNORM_8_8_(impl) \
  SNORM_8_8_8_(impl) \
  SNORM_8_8_8_8_(impl) \
\
  SNORM_16_(impl) \
  SNORM_16_16_(impl) \
  SNORM_16_16_16_(impl) \
  SNORM_16_16_16_16_(impl) \
\
  UNORM_8_(impl) \
  UNORM_8_8_(impl) \
  UNORM_8_8_8_(impl) \
  UNORM_8_8_8_8_(impl) \
\
  UNORM_16_(impl) \
  UNORM_16_16_(impl) \
  UNORM_16_16_16_(impl) \
  UNORM_16_16_16_16_(impl) \
\
  SINT_8_(impl) \
  SINT_8_8_(impl) \
  SINT_8_8_8_(impl) \
  SINT_8_8_8_8_(impl) \
\
  SINT_16_(impl) \
  SINT_16_16_(impl) \
  SINT_16_16_16_(impl) \
  SINT_16_16_16_16_(impl) \
\
  SINT_32_(impl) \
  SINT_32_32_(impl) \
  SINT_32_32_32_(impl) \
  SINT_32_32_32_32_(impl) \
\
  UINT_8_(impl) \
  UINT_8_8_(impl) \
  UINT_8_8_8_(impl) \
  UINT_8_8_8_8_(impl) \
\
  UINT_16_(impl) \
  UINT_16_16_(impl) \
  UINT_16_16_16_(impl) \
  UINT_16_16_16_16_(impl) \
\
  UINT_32_(impl) \
  UINT_32_32_(impl) \
  UINT_32_32_32_(impl) \
  UINT_32_32_32_32_(impl) \
\
  SFLOAT_16_(impl) \
  SFLOAT_16_16_(impl) \
  SFLOAT_16_16_16_(impl) \
  SFLOAT_16_16_16_16_(impl) \
\
  SFLOAT_32_(impl) \
  SFLOAT_32_32_(impl) \
  SFLOAT_32_32_32_(impl) \
  SFLOAT_32_32_32_32_(impl) \
\
  SNORM_10_10_10_2_(impl) \
  UNORM_10_10_10_2_(impl) \
  UINT_10_10_10_2_(impl) \
\
  UFLOAT_11_11_10_(impl) \
  UFLOAT_9_9_9_EXP_5_(impl) \
\
  UNORM_16_DEPTH_(impl) \
  UNORM_24_DEPTH_(impl) /* TODO(fclem): Incompatible with metal, is emulated. To remove. */ \
  UNORM_24_DEPTH_UINT_8_(impl) \
  SFLOAT_32_DEPTH_(impl) \
  SFLOAT_32_DEPTH_UINT_8_(impl) \
\
  SRGBA_8_8_8_(impl) \
  SRGBA_8_8_8_8_(impl) \
\
  SNORM_DXT1_(impl) \
  SNORM_DXT3_(impl) \
  SNORM_DXT5_(impl) \
  SRGB_DXT1_(impl) \
  SRGB_DXT3_(impl) \
  SRGB_DXT5_(impl)

  GPU_DATA_FORMAT_EXPAND(DECLARE)

#undef DECLARE
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 *
 * Allow querying information about the format enum values.
 * \{ */

/* NOTE: Compressed format bytesize are rounded up as their actual value is fractional. */
inline int to_bytesize(const DataFormat format)
{
#define CASE(a, size, c, blender_enum, d, e, f, g, h) \
  case DataFormat::blender_enum: \
    return size;

  switch (format) {
    GPU_DATA_FORMAT_EXPAND(CASE)
    case DataFormat::Invalid:
      break;
  }
#undef CASE
  BLI_assert_unreachable();
  return -1;
}

inline int format_component_len(const DataFormat format)
{
#define CASE(a, b, comp, blender_enum, d, e, f, g, h) \
  case DataFormat::blender_enum: \
    return comp;

  switch (format) {
    GPU_DATA_FORMAT_EXPAND(CASE)
    case DataFormat::Invalid:
      break;
  }
#undef CASE
  BLI_assert_unreachable();
  return -1;
}

/** \} */

}  // namespace blender::gpu
