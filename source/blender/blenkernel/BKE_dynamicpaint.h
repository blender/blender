/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct DynamicPaintCanvasSettings;
struct DynamicPaintModifierData;
struct DynamicPaintRuntime;
struct Object;
struct Scene;

/* Actual surface point */
typedef struct PaintSurfaceData {
  void *format_data;             /* special data for each surface "format" */
  void *type_data;               /* data used by specific surface type */
  struct PaintAdjData *adj_data; /* adjacency data for current surface */

  struct PaintBakeData *bData; /* temporary per step data used for frame calculation */
  int total_points;

} PaintSurfaceData;

/* Paint type surface point */
typedef struct PaintPoint {

  /* Wet paint is handled at effect layer only
   * and mixed to surface when drying */
  float e_color[4];
  float wetness;
  short state;
  float color[4];
} PaintPoint;

/* height field waves */
typedef struct PaintWavePoint {

  float height;
  float velocity;
  float brush_isect;
  short state;
} PaintWavePoint;

/**
 * Modifier call. Processes dynamic paint modifier step.
 */
struct Mesh *dynamicPaint_Modifier_do(struct DynamicPaintModifierData *pmd,
                                      struct Depsgraph *depsgraph,
                                      struct Scene *scene,
                                      struct Object *ob,
                                      struct Mesh *me);
/**
 * Free whole dynamic-paint modifier.
 */
void dynamicPaint_Modifier_free(struct DynamicPaintModifierData *pmd);
void dynamicPaint_Modifier_free_runtime(struct DynamicPaintRuntime *runtime);
void dynamicPaint_Modifier_copy(const struct DynamicPaintModifierData *pmd,
                                struct DynamicPaintModifierData *tpmd,
                                int flag);

/**
 * Initialize modifier data.
 */
bool dynamicPaint_createType(struct DynamicPaintModifierData *pmd, int type, struct Scene *scene);
/**
 * Creates a new surface and adds it to the list
 * If scene is null, frame range of 1-250 is used
 * A pointer to this surface is returned.
 */
struct DynamicPaintSurface *dynamicPaint_createNewSurface(
    struct DynamicPaintCanvasSettings *canvas, struct Scene *scene);
/**
 * Clears surface data back to zero.
 */
void dynamicPaint_clearSurface(const struct Scene *scene, struct DynamicPaintSurface *surface);
/**
 * Completely (re)initializes surface (only for point cache types).
 */
bool dynamicPaint_resetSurface(const struct Scene *scene, struct DynamicPaintSurface *surface);
void dynamicPaint_freeSurface(const struct DynamicPaintModifierData *pmd,
                              struct DynamicPaintSurface *surface);
/**
 * Free canvas data.
 */
void dynamicPaint_freeCanvas(struct DynamicPaintModifierData *pmd);
/* Free brush data */
void dynamicPaint_freeBrush(struct DynamicPaintModifierData *pmd);
void dynamicPaint_freeSurfaceData(struct DynamicPaintSurface *surface);

/**
 * Update cache frame range.
 */
void dynamicPaint_cacheUpdateFrames(struct DynamicPaintSurface *surface);
bool dynamicPaint_outputLayerExists(struct DynamicPaintSurface *surface,
                                    struct Object *ob,
                                    int output);
/**
 * Change surface data to defaults on new type.
 */
void dynamicPaintSurface_updateType(struct DynamicPaintSurface *surface);
void dynamicPaintSurface_setUniqueName(struct DynamicPaintSurface *surface, const char *basename);
/**
 * Get currently active surface (in user interface).
 */
struct DynamicPaintSurface *get_activeSurface(struct DynamicPaintCanvasSettings *canvas);

/**
 * Image sequence baking.
 */
int dynamicPaint_createUVSurface(struct Scene *scene,
                                 struct DynamicPaintSurface *surface,
                                 float *progress,
                                 bool *do_update);
/**
 * Calculate a single frame and included sub-frames for surface.
 */
int dynamicPaint_calculateFrame(struct DynamicPaintSurface *surface,
                                struct Depsgraph *depsgraph,
                                struct Scene *scene,
                                struct Object *cObject,
                                int frame);
void dynamicPaint_outputSurfaceImage(struct DynamicPaintSurface *surface,
                                     const char *filepath,
                                     short output_layer);

/* PaintPoint state */
#define DPAINT_PAINT_NONE -1
#define DPAINT_PAINT_DRY 0
#define DPAINT_PAINT_WET 1
#define DPAINT_PAINT_NEW 2

/* PaintWavePoint state */
#define DPAINT_WAVE_ISECT_CHANGED -1
#define DPAINT_WAVE_NONE 0
#define DPAINT_WAVE_OBSTACLE 1
#define DPAINT_WAVE_REFLECT_ONLY 2

#ifdef __cplusplus
}
#endif
