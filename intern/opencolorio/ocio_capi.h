/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __OCIO_CAPI_H__
#define __OCIO_CAPI_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OCIO_GPUShader OCIO_GPUShader;

#define OCIO_DECLARE_HANDLE(name) \
  struct name; \
  typedef struct name *name##Ptr;

#define OCIO_ROLE_DATA "data"
#define OCIO_ROLE_SCENE_LINEAR "scene_linear"
#define OCIO_ROLE_COLOR_PICKING "color_picking"
#define OCIO_ROLE_TEXTURE_PAINT "texture_paint"
#define OCIO_ROLE_DEFAULT_BYTE "default_byte"
#define OCIO_ROLE_DEFAULT_FLOAT "default_float"
#define OCIO_ROLE_DEFAULT_SEQUENCER "default_sequencer"

OCIO_DECLARE_HANDLE(OCIO_ConstConfigRc);
OCIO_DECLARE_HANDLE(OCIO_ConstColorSpaceRc);
OCIO_DECLARE_HANDLE(OCIO_ConstProcessorRc);
OCIO_DECLARE_HANDLE(OCIO_ConstCPUProcessorRc);
OCIO_DECLARE_HANDLE(OCIO_ConstContextRc);
OCIO_DECLARE_HANDLE(OCIO_PackedImageDesc);
OCIO_DECLARE_HANDLE(OCIO_ConstLookRc);

/* Standard XYZ (D65) to linear Rec.709 transform. */
static const float OCIO_XYZ_TO_REC709[3][3] = {{3.2404542f, -0.9692660f, 0.0556434f},
                                               {-1.5371385f, 1.8760108f, -0.2040259f},
                                               {-0.4985314f, 0.0415560f, 1.0572252f}};
/* Standard ACES to XYZ (D65) transform.
 * Matches OpenColorIO builtin transform: UTILITY - ACES-AP0_to_CIE-XYZ-D65_BFD. */
static const float OCIO_ACES_TO_XYZ[3][3] = {{0.938280f, 0.337369f, 0.001174f},
                                             {-0.004451f, 0.729522f, -0.003711f},
                                             {0.016628f, -0.066890f, 1.091595f}};

/* This structure is used to pass curve mapping settings from
 * blender's DNA structure stored in view transform settings
 * to a generic OpenColorIO C-API.
 */
typedef struct OCIO_CurveMappingSettings {
  /* This is a LUT which contain values for all 4 curve mapping tables
   * (combined, R, G and B).
   *
   * Element I for table T is stored at I * 4 + T element of this LUT.
   *
   * This array is usually returned by curvemapping_table_RGBA().
   */
  float *lut;

  /* Size of single curve mapping table, 1/4 size of lut array. */
  int lut_size;

  /* Extend extrapolation flags for all the tables.
   * if use_extend_extrapolate != 0 means extrapolation for
   * curve.
   */
  int use_extend_extrapolate;

  /* Minimal X value of the curve mapping tables. */
  float mintable[4];

  /* Per curve mapping table range. */
  float range[4];

  /* Lower extension value, stored as per-component arrays. */
  float ext_in_x[4], ext_in_y[4];

  /* Higher extension value, stored as per-component arrays. */
  float ext_out_x[4], ext_out_y[4];

  /* First points of the tables, both X and Y values.
   * Needed for easier and faster access when extrapolating.
   */
  float first_x[4], first_y[4];

  /* Last points of the tables, both X and Y values.
   * Needed for easier and faster access when extrapolating.
   */
  float last_x[4], last_y[4];

  /* Premultiplication settings: black level and scale to match
   * with white level.
   */
  float black[3], bwmul[3];

  /* Cache id of the original curve mapping, used to detect when
   * upload of new settings to GPU is needed.
   */
  size_t cache_id;
} OCIO_CurveMappingSettings;

void OCIO_init(void);
void OCIO_exit(void);

OCIO_ConstConfigRcPtr *OCIO_getCurrentConfig(void);
void OCIO_setCurrentConfig(const OCIO_ConstConfigRcPtr *config);

OCIO_ConstConfigRcPtr *OCIO_configCreateFromEnv(void);
OCIO_ConstConfigRcPtr *OCIO_configCreateFromFile(const char *filename);
OCIO_ConstConfigRcPtr *OCIO_configCreateFallback(void);

void OCIO_configRelease(OCIO_ConstConfigRcPtr *config);

int OCIO_configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config);
const char *OCIO_configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
OCIO_ConstColorSpaceRcPtr *OCIO_configGetColorSpace(OCIO_ConstConfigRcPtr *config,
                                                    const char *name);
int OCIO_configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);

int OCIO_colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs);
int OCIO_colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs);
void OCIO_colorSpaceIsBuiltin(OCIO_ConstConfigRcPtr *config,
                              OCIO_ConstColorSpaceRcPtr *cs,
                              bool *is_scene_linear,
                              bool *is_srgb);

void OCIO_colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs);

const char *OCIO_configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config);
int OCIO_configGetNumDisplays(OCIO_ConstConfigRcPtr *config);
const char *OCIO_configGetDisplay(OCIO_ConstConfigRcPtr *config, int index);
const char *OCIO_configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display);
int OCIO_configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display);
const char *OCIO_configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index);
const char *OCIO_configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config,
                                                const char *display,
                                                const char *view);

void OCIO_configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr *config, float *rgb);
void OCIO_configGetXYZtoSceneLinear(OCIO_ConstConfigRcPtr *config,
                                    float xyz_to_scene_linear[3][3]);

int OCIO_configGetNumLooks(OCIO_ConstConfigRcPtr *config);
const char *OCIO_configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
OCIO_ConstLookRcPtr *OCIO_configGetLook(OCIO_ConstConfigRcPtr *config, const char *name);

const char *OCIO_lookGetProcessSpace(OCIO_ConstLookRcPtr *look);
void OCIO_lookRelease(OCIO_ConstLookRcPtr *look);

OCIO_ConstProcessorRcPtr *OCIO_configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                           const char *srcName,
                                                           const char *dstName);
void OCIO_processorRelease(OCIO_ConstProcessorRcPtr *cpu_processor);

OCIO_ConstCPUProcessorRcPtr *OCIO_processorGetCPUProcessor(OCIO_ConstProcessorRcPtr *processor);
bool OCIO_cpuProcessorIsNoOp(OCIO_ConstCPUProcessorRcPtr *cpu_processor);
void OCIO_cpuProcessorApply(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                            struct OCIO_PackedImageDesc *img);
void OCIO_cpuProcessorApply_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                      struct OCIO_PackedImageDesc *img);
void OCIO_cpuProcessorApplyRGB(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel);
void OCIO_cpuProcessorApplyRGBA(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel);
void OCIO_cpuProcessorApplyRGBA_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                          float *pixel);
void OCIO_cpuProcessorRelease(OCIO_ConstCPUProcessorRcPtr *processor);

const char *OCIO_colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs);
const char *OCIO_colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs);
const char *OCIO_colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs);
int OCIO_colorSpaceGetNumAliases(OCIO_ConstColorSpaceRcPtr *cs);
const char *OCIO_colorSpaceGetAlias(OCIO_ConstColorSpaceRcPtr *cs, const int index);

OCIO_ConstProcessorRcPtr *OCIO_createDisplayProcessor(OCIO_ConstConfigRcPtr *config,
                                                      const char *input,
                                                      const char *view,
                                                      const char *display,
                                                      const char *look,
                                                      const float scale,
                                                      const float exponent,
                                                      const bool inverse);

struct OCIO_PackedImageDesc *OCIO_createOCIO_PackedImageDesc(float *data,
                                                             long width,
                                                             long height,
                                                             long numChannels,
                                                             long chanStrideBytes,
                                                             long xStrideBytes,
                                                             long yStrideBytes);

void OCIO_PackedImageDescRelease(struct OCIO_PackedImageDesc *p);

bool OCIO_supportGPUShader(void);
bool OCIO_gpuDisplayShaderBind(OCIO_ConstConfigRcPtr *config,
                               const char *input,
                               const char *view,
                               const char *display,
                               const char *look,
                               OCIO_CurveMappingSettings *curve_mapping_settings,
                               const float scale,
                               const float exponent,
                               const float dither,
                               const bool use_predivide,
                               const bool use_overlay,
                               const bool use_hdr);
void OCIO_gpuDisplayShaderUnbind(void);
void OCIO_gpuCacheFree(void);

const char *OCIO_getVersionString(void);
int OCIO_getVersionHex(void);

#ifdef __cplusplus
}
#endif

#endif /* OCIO_CAPI_H */
