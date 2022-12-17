/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */
#ifndef EEVEE_SHADER_SHARED_H
#define EEVEE_SHADER_SHARED_H
#ifndef GPU_SHADER
typedef struct CommonUniformBlock CommonUniformBlock;
#endif

#ifdef GPU_SHADER
/* Catch for non-create info case. */
#  ifndef BLI_STATIC_ASSERT_ALIGN
#    define BLI_STATIC_ASSERT_ALIGN(type, alignment)
#  endif
#endif

struct CommonUniformBlock {
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
  vec4 cameraUvScaleBias;
  vec4 planarClipPlane;
};
BLI_STATIC_ASSERT_ALIGN(CommonUniformBlock, 16)

struct CubeData {
  vec4 position_type;
  vec4 attenuation_fac_type;
  mat4 influencemat;
  mat4 parallaxmat;
};
BLI_STATIC_ASSERT_ALIGN(CubeData, 16)

struct PlanarData {
  vec4 plane_equation;
  vec4 clip_vec_x_fade_scale;
  vec4 clip_vec_y_fade_bias;
  vec4 clip_edges;
  vec4 facing_scale_bias;
  mat4 reflectionmat; /* transform world space into reflection texture space */
  mat4 unused;
};
BLI_STATIC_ASSERT_ALIGN(PlanarData, 16)

struct GridData {
  mat4 localmat;
  ivec4 resolution_offset;
  vec4 ws_corner_atten_scale;     /* world space corner position */
  vec4 ws_increment_x_atten_bias; /* world space vector between 2 opposite cells */
  vec4 ws_increment_y_lvl_bias;
  vec4 ws_increment_z;
  vec4 vis_bias_bleed_range;
};
BLI_STATIC_ASSERT_ALIGN(GridData, 16)

struct ProbeBlock {
  CubeData probes_data[MAX_PROBE];
};
BLI_STATIC_ASSERT_ALIGN(ProbeBlock, 16)

struct GridBlock {
  GridData grids_data[MAX_GRID];
};
BLI_STATIC_ASSERT_ALIGN(GridBlock, 16)

struct PlanarBlock {
  PlanarData planars_data[MAX_PLANAR];
};
BLI_STATIC_ASSERT_ALIGN(PlanarBlock, 16)

#ifndef MAX_CASCADE_NUM
#  define MAX_CASCADE_NUM 4
#endif

struct ShadowData {
  vec4 near_far_bias_id;
  vec4 contact_shadow_data;
};
BLI_STATIC_ASSERT_ALIGN(ShadowData, 16)

struct ShadowCubeData {
  mat4 shadowmat;
  vec4 position;
};
BLI_STATIC_ASSERT_ALIGN(ShadowCubeData, 16)

struct ShadowCascadeData {
  mat4 shadowmat[MAX_CASCADE_NUM];
  vec4 split_start_distances;
  vec4 split_end_distances;
  vec4 shadow_vec_id;
};
BLI_STATIC_ASSERT_ALIGN(ShadowCascadeData, 16)

struct ShadowBlock {
  ShadowData shadows_data[MAX_SHADOW];
  ShadowCubeData shadows_cube_data[MAX_SHADOW_CUBE];
  ShadowCascadeData shadows_cascade_data[MAX_SHADOW_CASCADE];
};
BLI_STATIC_ASSERT_ALIGN(ShadowBlock, 16)

struct LightData {
  vec4 position_influence;     /* w : InfluenceRadius (inversed and squared) */
  vec4 color_influence_volume; /* w : InfluenceRadius but for Volume power */
  vec4 spotdata_radius_shadow; /* x : spot size, y : spot blend, z : radius, w: shadow id */
  vec4 rightvec_sizex;         /* xyz: Normalized up vector, w: area size X or spot scale X */
  vec4 upvec_sizey;            /* xyz: Normalized right vector, w: area size Y or spot scale Y */
  vec4 forwardvec_type;        /* xyz: Normalized forward vector, w: Light Type */
  vec4 diff_spec_volume;       /* xyz: Diffuse/Spec/Volume power, w: radius for volumetric. */
};
BLI_STATIC_ASSERT_ALIGN(LightData, 16)

struct LightBlock {
  LightData lights_data[MAX_LIGHT];
};
BLI_STATIC_ASSERT_ALIGN(LightBlock, 16)

struct RenderpassBlock {
  bool renderPassDiffuse;
  bool renderPassDiffuseLight;
  bool renderPassGlossy;
  bool renderPassGlossyLight;
  bool renderPassEmit;
  bool renderPassSSSColor;
  bool renderPassEnvironment;
  bool renderPassAOV;
  uint renderPassAOVActive;
};
BLI_STATIC_ASSERT_ALIGN(RenderpassBlock, 16)

#define MAX_SSS_SAMPLES 65
#define SSS_LUT_SIZE 64.0
#define SSS_LUT_SCALE ((SSS_LUT_SIZE - 1.0) / float(SSS_LUT_SIZE))
#define SSS_LUT_BIAS (0.5 / float(SSS_LUT_SIZE))

struct SSSProfileBlock {
  vec4 sss_kernel[MAX_SSS_SAMPLES];
  vec4 radii_max_radius;
  float avg_inv_radius;
  int sss_samples;
};
BLI_STATIC_ASSERT_ALIGN(SSSProfileBlock, 16)

#ifdef GPU_SHADER

#  if defined(USE_GPU_SHADER_CREATE_INFO)

/* Keep compatibility_with old global scope syntax. */
#    define pastViewProjectionMatrix common_block.pastViewProjectionMatrix
#    define hizUvScale common_block.hizUvScale
#    define aoParameters common_block.aoParameters
#    define volTexSize common_block.volTexSize
#    define volDepthParameters common_block.volDepthParameters
#    define volInvTexSize common_block.volInvTexSize
#    define volJitter common_block.volJitter
#    define volCoordScale common_block.volCoordScale
#    define volHistoryAlpha common_block.volHistoryAlpha
#    define volShadowSteps common_block.volShadowSteps
#    define volUseLights common_block.volUseLights
#    define volUseSoftShadows common_block.volUseSoftShadows
#    define ssrParameters common_block.ssrParameters
#    define ssrBorderFac common_block.ssrBorderFac
#    define ssrMaxRoughness common_block.ssrMaxRoughness
#    define ssrFireflyFac common_block.ssrFireflyFac
#    define ssrBrdfBias common_block.ssrBrdfBias
#    define ssrToggle common_block.ssrToggle
#    define ssrefractToggle common_block.ssrefractToggle
#    define sssJitterThreshold common_block.sssJitterThreshold
#    define sssToggle common_block.sssToggle
#    define specToggle common_block.specToggle
#    define laNumLight common_block.laNumLight
#    define prbNumPlanar common_block.prbNumPlanar
#    define prbNumRenderCube common_block.prbNumRenderCube
#    define prbNumRenderGrid common_block.prbNumRenderGrid
#    define prbIrradianceVisSize common_block.prbIrradianceVisSize
#    define prbIrradianceSmooth common_block.prbIrradianceSmooth
#    define prbLodCubeMax common_block.prbLodCubeMax
#    define rayType common_block.rayType
#    define rayDepth common_block.rayDepth
#    define alphaHashOffset common_block.alphaHashOffset
#    define alphaHashScale common_block.alphaHashScale
#    define cameraUvScaleBias common_block.cameraUvScaleBias
#    define planarClipPlane common_block.planarClipPlane

/* ProbeBlock */
#    define probes_data probe_block.probes_data

/* GridBlock */
#    define grids_data grid_block.grids_data

/* PlanarBlock */
#    define planars_data planar_block.planars_data

/* ShadowBlock */
#    define shadows_data shadow_block.shadows_data
#    define shadows_cube_data shadow_block.shadows_cube_data
#    define shadows_cascade_data shadow_block.shadows_cascade_data

/* LightBlock */
#    define lights_data light_block.lights_data

/* RenderpassBlock */
#    define renderPassDiffuse renderpass_block.renderPassDiffuse
#    define renderPassDiffuseLight renderpass_block.renderPassDiffuseLight
#    define renderPassGlossy renderpass_block.renderPassGlossy
#    define renderPassGlossyLight renderpass_block.renderPassGlossyLight
#    define renderPassEmit renderpass_block.renderPassEmit
#    define renderPassSSSColor renderpass_block.renderPassSSSColor
#    define renderPassEnvironment renderpass_block.renderPassEnvironment
#    define renderPassAOV renderpass_block.renderPassAOV
#    define renderPassAOVActive renderpass_block.renderPassAOVActive

/* SSSProfileBlock */
#    define sss_kernel sssProfile.sss_kernel
#    define radii_max_radius sssProfile.radii_max_radius
#    define avg_inv_radius sssProfile.avg_inv_radius
#    define sss_samples sssProfile.sss_samples

#  endif /* USE_GPU_SHADER_CREATE_INFO */

#endif
#endif
