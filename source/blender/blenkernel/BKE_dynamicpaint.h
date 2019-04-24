/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __BKE_DYNAMICPAINT_H__
#define __BKE_DYNAMICPAINT_H__

/** \file
 * \ingroup bke
 */

struct Depsgraph;
struct DynamicPaintCanvasSettings;
struct DynamicPaintModifierData;
struct DynamicPaintRuntime;
struct Main;
struct Scene;
struct ViewLayer;

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

struct Mesh *dynamicPaint_Modifier_do(struct DynamicPaintModifierData *pmd,
                                      struct Depsgraph *depsgraph,
                                      struct Scene *scene,
                                      struct Object *ob,
                                      struct Mesh *me);
void dynamicPaint_Modifier_free(struct DynamicPaintModifierData *pmd);
void dynamicPaint_Modifier_free_runtime(struct DynamicPaintRuntime *runtime);
void dynamicPaint_Modifier_copy(const struct DynamicPaintModifierData *pmd,
                                struct DynamicPaintModifierData *tsmd,
                                int flag);

bool dynamicPaint_createType(struct DynamicPaintModifierData *pmd, int type, struct Scene *scene);
struct DynamicPaintSurface *dynamicPaint_createNewSurface(
    struct DynamicPaintCanvasSettings *canvas, struct Scene *scene);
void dynamicPaint_clearSurface(const struct Scene *scene, struct DynamicPaintSurface *surface);
bool dynamicPaint_resetSurface(const struct Scene *scene, struct DynamicPaintSurface *surface);
void dynamicPaint_freeSurface(const struct DynamicPaintModifierData *pmd,
                              struct DynamicPaintSurface *surface);
void dynamicPaint_freeCanvas(struct DynamicPaintModifierData *pmd);
void dynamicPaint_freeBrush(struct DynamicPaintModifierData *pmd);
void dynamicPaint_freeSurfaceData(struct DynamicPaintSurface *surface);

void dynamicPaint_cacheUpdateFrames(struct DynamicPaintSurface *surface);
bool dynamicPaint_outputLayerExists(struct DynamicPaintSurface *surface,
                                    struct Object *ob,
                                    int output);
void dynamicPaintSurface_updateType(struct DynamicPaintSurface *surface);
void dynamicPaintSurface_setUniqueName(struct DynamicPaintSurface *surface, const char *basename);
struct DynamicPaintSurface *get_activeSurface(struct DynamicPaintCanvasSettings *canvas);

/* image sequence baking */
int dynamicPaint_createUVSurface(struct Scene *scene,
                                 struct DynamicPaintSurface *surface,
                                 float *progress,
                                 short *do_update);
int dynamicPaint_calculateFrame(struct DynamicPaintSurface *surface,
                                struct Depsgraph *depsgraph,
                                struct Scene *scene,
                                struct Object *cObject,
                                int frame);
void dynamicPaint_outputSurfaceImage(struct DynamicPaintSurface *surface,
                                     char *filename,
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

#endif /* __BKE_DYNAMICPAINT_H__ */
