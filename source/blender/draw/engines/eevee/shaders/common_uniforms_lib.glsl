/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define COMMON_UNIFORMS_LIB

#if !defined(USE_GPU_SHADER_CREATE_INFO)
/* keep in sync with CommonUniformBlock */
layout(std140) uniform common_block
{
  mat4 pastViewProjectionMatrix;
  vec4 hizUvScale; /* To correct mip level texel misalignment */
  /* Ambient Occlusion */
  vec4 aoParameters[2];
  /* Volumetric */
  ivec4 volTexSize;
  vec4 volDepthParameters; /* Parameters to the volume Z equation */
  vec4 volInvTexSize;
  vec4 volJitter;
  vec4 volCoordScale; /* To convert volume uvs to screen uvs */
  float volHistoryAlpha;
  float volShadowSteps;
  bool volUseLights;
  bool volUseSoftShadows;
  /* Screen Space Reflections */
  vec4 ssrParameters;
  float ssrBorderFac;
  float ssrMaxRoughness;
  float ssrFireflyFac;
  float ssrBrdfBias;
  bool ssrToggle;
  bool ssrefractToggle;
  /* SubSurface Scattering */
  float sssJitterThreshold;
  bool sssToggle;
  /* Specular */
  bool specToggle;
  /* Lights */
  int laNumLight;
  /* Probes */
  int prbNumPlanar;
  int prbNumRenderCube;
  int prbNumRenderGrid;
  int prbIrradianceVisSize;
  float prbIrradianceSmooth;
  float prbLodCubeMax;
  /* Misc */
  int rayType;
  float rayDepth;
  float alphaHashOffset;
  float alphaHashScale;
  /* Misc */
  vec4 cameraUvScaleBias;
  vec4 planarClipPlane;
};

#endif /* !USE_GPU_SHADER_CREATE_INFO */

#ifdef USE_GPU_SHADER_CREATE_INFO
#  ifndef EEVEE_SHADER_SHARED_H
#    error Missing eevee_legacy_common_lib additional create info on shader create info
#  endif
#endif

/* rayType (keep in sync with ray_type) */
#define EEVEE_RAY_CAMERA 0
#define EEVEE_RAY_SHADOW 1
#define EEVEE_RAY_DIFFUSE 2
#define EEVEE_RAY_GLOSSY 3

/* aoParameters */
#define aoDistance aoParameters[0].x
#define aoSamples aoParameters[0].y /* UNUSED */
#define aoFactor aoParameters[0].z
#define aoInvSamples aoParameters[0].w /* UNUSED */

#define aoOffset aoParameters[1].x /* UNUSED */
#define aoBounceFac aoParameters[1].y
#define aoQuality aoParameters[1].z
#define aoSettings aoParameters[1].w

/* ssrParameters */
#define ssrQuality ssrParameters.x
#define ssrThickness ssrParameters.y
#define ssrPixelSize ssrParameters.zw

#define ssrUvScale hizUvScale.zw
