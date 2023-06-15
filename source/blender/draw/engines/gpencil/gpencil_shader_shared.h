/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  pragma once

#  include "GPU_shader_shared_utils.h"

#  ifndef __cplusplus
typedef struct gpScene gpScene;
typedef struct gpMaterial gpMaterial;
typedef struct gpLight gpLight;
typedef struct gpObject gpObject;
typedef struct gpLayer gpLayer;
typedef enum gpMaterialFlag gpMaterialFlag;
#    ifdef GP_LIGHT
typedef enum gpLightType gpLightType;
#    endif
#  endif
#endif

enum gpMaterialFlag {
  GP_FLAG_NONE = 0u,
  GP_STROKE_ALIGNMENT_STROKE = 1u,
  GP_STROKE_ALIGNMENT_OBJECT = 2u,
  GP_STROKE_ALIGNMENT_FIXED = 3u,
  GP_STROKE_ALIGNMENT = 0x3u,
  GP_STROKE_OVERLAP = (1u << 2u),
  GP_STROKE_TEXTURE_USE = (1u << 3u),
  GP_STROKE_TEXTURE_STENCIL = (1u << 4u),
  GP_STROKE_TEXTURE_PREMUL = (1u << 5u),
  GP_STROKE_DOTS = (1u << 6u),
  GP_STROKE_HOLDOUT = (1u << 7u),
  GP_FILL_HOLDOUT = (1u << 8u),
  GP_FILL_TEXTURE_USE = (1u << 10u),
  GP_FILL_TEXTURE_PREMUL = (1u << 11u),
  GP_FILL_TEXTURE_CLIP = (1u << 12u),
  GP_FILL_GRADIENT_USE = (1u << 13u),
  GP_FILL_GRADIENT_RADIAL = (1u << 14u),
  GP_SHOW_STROKE = (1u << 15u),
  GP_SHOW_FILL = (1u << 16u),
  GP_FILL_FLAGS = (GP_FILL_TEXTURE_USE | GP_FILL_TEXTURE_PREMUL | GP_FILL_TEXTURE_CLIP |
                   GP_FILL_GRADIENT_USE | GP_FILL_GRADIENT_RADIAL | GP_FILL_HOLDOUT),
};

enum gpLightType {
  GP_LIGHT_TYPE_POINT = 0u,
  GP_LIGHT_TYPE_SPOT = 1u,
  GP_LIGHT_TYPE_SUN = 2u,
  GP_LIGHT_TYPE_AMBIENT = 3u,
};

#define GP_IS_STROKE_VERTEX_BIT (1 << 30)
#define GP_VERTEX_ID_SHIFT 2

/* Avoid compiler funkiness with enum types not being strongly typed in C. */
#ifndef GPU_SHADER
#  define gpMaterialFlag uint
#  define gpLightType uint
#endif

struct gpScene {
  float2 render_size;
  float2 _pad0;
};
BLI_STATIC_ASSERT_ALIGN(gpScene, 16)

struct gpMaterial {
  float4 stroke_color;
  float4 fill_color;
  float4 fill_mix_color;
  float4 fill_uv_rot_scale;
#ifndef GPU_SHADER
  float2 fill_uv_offset;
  float2 alignment_rot;
  float stroke_texture_mix;
  float stroke_u_scale;
  float fill_texture_mix;
  gpMaterialFlag flag;
#else
  /* Some drivers are completely messing the alignment or the fetches here.
   * We are forced to pack these into vec4 otherwise we only get 0.0 as value. */
  /* NOTE(@fclem): This was the case on MacOS OpenGL implementation.
   * This might be fixed in newer APIs. */
  float4 packed1;
  float4 packed2;
#  define _fill_uv_offset packed1.xy
#  define _alignment_rot packed1.zw
#  define _stroke_texture_mix packed2.x
#  define _stroke_u_scale packed2.y
#  define _fill_texture_mix packed2.z
  /** NOTE(@fclem): Needs floatBitsToUint(). */
#  define _flag packed2.w
#endif
};
BLI_STATIC_ASSERT_ALIGN(gpMaterial, 16)

#ifdef GP_LIGHT
struct gpLight {
#  ifndef GPU_SHADER
  float3 color;
  gpLightType type;
  float3 right;
  float spot_size;
  float3 up;
  float spot_blend;
  float3 forward;
  float _pad0;
  float3 position;
  float _pad1;
#  else
  /* Some drivers are completely messing the alignment or the fetches here.
   * We are forced to pack these into vec4 otherwise we only get 0.0 as value. */
  /* NOTE(@fclem): This was the case on MacOS OpenGL implementation.
   * This might be fixed in newer APIs. */
  float4 packed0;
  float4 packed1;
  float4 packed2;
  float4 packed3;
  float4 packed4;
#    define _color packed0.xyz
#    define _type packed0.w
#    define _right packed1.xyz
#    define _spot_size packed1.w
#    define _up packed2.xyz
#    define _spot_blend packed2.w
#    define _forward packed3.xyz
#    define _position packed4.xyz
#  endif
};
BLI_STATIC_ASSERT_ALIGN(gpLight, 16)
#endif

struct gpObject {
  /** Weather or not to apply lighting to the GPencil object. */
  bool1 is_shadeless;
  /** Switch between 2d and 3D stroke order. */
  bool1 stroke_order3d;
  /** Offset inside the layer buffer to the first layer data of this object. */
  uint layer_offset;
  /** Offset inside the material buffer to the first material data of this object. */
  uint material_offset;
  /** Color to multiply to the final mixed color. */
  float4 tint;
  /** Color to multiply to the final mixed color. */
  float3 normal;

  float _pad0;
};
BLI_STATIC_ASSERT_ALIGN(gpObject, 16)

struct gpLayer {
  /** Amount of vertex color to blend with actual material color. */
  float vertex_color_opacity;
  /** Thickness change of all the strokes. */
  float thickness_offset;
  /** Thickness change of all the strokes. */
  float opacity;
  /** Offset to apply to stroke index to be able to insert a currently drawn stroke in between. */
  float stroke_index_offset;
  /** Color to multiply to the final mixed color. */
  float4 tint;
};
BLI_STATIC_ASSERT_ALIGN(gpLayer, 16)

#ifndef GPU_SHADER
#  undef gpMaterialFlag
#  undef gpLightType
#endif
