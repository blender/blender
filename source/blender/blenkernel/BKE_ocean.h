/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <stdbool.h>

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct OceanModifierData;

typedef struct OceanResult {
  float disp[3];
  float normal[3];
  float foam;

  /* raw eigenvalues/vectors */
  float Jminus;
  float Jplus;
  float Eminus[3];
  float Eplus[3];
} OceanResult;

typedef struct OceanCache {
  struct ImBuf **ibufs_disp;
  struct ImBuf **ibufs_foam;
  struct ImBuf **ibufs_norm;
  /* spray is Eplus */
  struct ImBuf **ibufs_spray;
  /* spray_inverse is Eminus */
  struct ImBuf **ibufs_spray_inverse;

  const char *bakepath;
  const char *relbase;

  /* precalculated for time range */
  float *time;

  /* constant for time range */
  float wave_scale;
  float chop_amount;
  float foam_coverage;
  float foam_fade;

  int start;
  int end;
  int duration;
  int resolution_x;
  int resolution_y;

  int baked;
} OceanCache;

struct Ocean *BKE_ocean_add(void);
void BKE_ocean_free_data(struct Ocean *oc);
void BKE_ocean_free(struct Ocean *oc);
bool BKE_ocean_ensure(struct OceanModifierData *omd, int resolution);
/**
 * Return true if the ocean data is valid and can be used.
 */
bool BKE_ocean_init_from_modifier(struct Ocean *ocean,
                                  struct OceanModifierData const *omd,
                                  int resolution);

/**
 * Return true if the ocean is valid and can be used.
 */
bool BKE_ocean_is_valid(const struct Ocean *o);

/**
 * Return true if the ocean data is valid and can be used.
 */
bool BKE_ocean_init(struct Ocean *o,
                    int M,
                    int N,
                    float Lx,
                    float Lz,
                    float V,
                    float l,
                    float A,
                    float w,
                    float damp,
                    float alignment,
                    float depth,
                    float time,
                    int spectrum,
                    float fetch_jonswap,
                    float sharpen_peak_jonswap,
                    short do_height_field,
                    short do_chop,
                    short do_spray,
                    short do_normals,
                    short do_jacobian,
                    int seed);
void BKE_ocean_simulate(struct Ocean *o, float t, float scale, float chop_amount);

float BKE_ocean_jminus_to_foam(float jminus, float coverage);
/**
 * Sampling the ocean surface.
 */
void BKE_ocean_eval_uv(struct Ocean *oc, struct OceanResult *ocr, float u, float v);
/**
 * Use catmullrom interpolation rather than linear.
 */
void BKE_ocean_eval_uv_catrom(struct Ocean *oc, struct OceanResult *ocr, float u, float v);
void BKE_ocean_eval_xz(struct Ocean *oc, struct OceanResult *ocr, float x, float z);
void BKE_ocean_eval_xz_catrom(struct Ocean *oc, struct OceanResult *ocr, float x, float z);
/**
 * Note that this doesn't wrap properly for i, j < 0, but its not really meant for that being
 * just a way to get the raw data out to save in some image format.
 */
void BKE_ocean_eval_ij(struct Ocean *oc, struct OceanResult *ocr, int i, int j);

/**
 * Ocean cache handling.
 */
struct OceanCache *BKE_ocean_init_cache(const char *bakepath,
                                        const char *relbase,
                                        int start,
                                        int end,
                                        float wave_scale,
                                        float chop_amount,
                                        float foam_coverage,
                                        float foam_fade,
                                        int resolution);
void BKE_ocean_simulate_cache(struct OceanCache *och, int frame);

void BKE_ocean_bake(struct Ocean *o,
                    struct OceanCache *och,
                    void (*update_cb)(void *, float progress, int *cancel),
                    void *update_cb_data);
void BKE_ocean_cache_eval_uv(
    struct OceanCache *och, struct OceanResult *ocr, int f, float u, float v);
void BKE_ocean_cache_eval_ij(struct OceanCache *och, struct OceanResult *ocr, int f, int i, int j);

void BKE_ocean_free_cache(struct OceanCache *och);
void BKE_ocean_free_modifier_cache(struct OceanModifierData *omd);

/* `ocean_spectrum.cc` */

/**
 * Pierson-Moskowitz model, 1964, assumes waves reach equilibrium with wind.
 * Model is intended for large area 'fully developed' sea, where winds have been steadily blowing
 * for days over an area that includes hundreds of wavelengths on a side.
 */
float BLI_ocean_spectrum_piersonmoskowitz(const struct Ocean *oc, float kx, float kz);
/**
 * TMA extends the JONSWAP spectrum.
 * This spectral model is best suited to shallow water.
 */
float BLI_ocean_spectrum_texelmarsenarsloe(const struct Ocean *oc, float kx, float kz);
/**
 * Hasselmann et al, 1973. This model extends the Pierson-Moskowitz model with a peak sharpening
 * function This enhancement is an artificial construct to address the problem that the wave
 * spectrum is never fully developed.
 *
 * The fetch parameter represents the distance from a lee shore,
 * called the fetch, or the distance over which the wind blows with constant velocity.
 */
float BLI_ocean_spectrum_jonswap(const struct Ocean *oc, float kx, float kz);

#ifdef __cplusplus
}
#endif
