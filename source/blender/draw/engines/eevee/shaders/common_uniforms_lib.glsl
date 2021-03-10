
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
  float volLightClamp;
  float volShadowSteps;
  bool volUseLights;
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
  /* Misc*/
  int rayType;
  float rayDepth;
  float alphaHashOffset;
  float alphaHashScale;
  float pad6;
  float pad7;
  float pad8;
  float pad9;
};

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
