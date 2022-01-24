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
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup render
 */

#pragma once

struct Depsgraph;
struct ImBuf;
struct MLoopUV;
struct Mesh;
struct Render;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BakeImage {
  struct Image *image;
  int width;
  int height;
  size_t offset;
} BakeImage;

typedef struct BakeTargets {
  /* All images of the object. */
  BakeImage *images;
  int num_images;

  /* Lookup table from Material number to BakeImage. */
  int *material_to_image;
  int num_materials;

  /* Pixel buffer to bake to. */
  float *result;
  int num_pixels;
  int num_channels;

  /* Baking to non-color data image. */
  bool is_noncolor;
} BakeTargets;

typedef struct BakePixel {
  int primitive_id, object_id;
  int seed;
  float uv[2];
  float du_dx, du_dy;
  float dv_dx, dv_dy;
} BakePixel;

typedef struct BakeHighPolyData {
  struct Object *ob;
  struct Object *ob_eval;
  struct Mesh *me;
  bool is_flip_object;

  float obmat[4][4];
  float imat[4][4];
} BakeHighPolyData;

/* external_engine.c */
bool RE_bake_has_engine(const struct Render *re);

bool RE_bake_engine(struct Render *re,
                    struct Depsgraph *depsgraph,
                    struct Object *object,
                    int object_id,
                    const BakePixel pixel_array[],
                    const BakeTargets *targets,
                    eScenePassType pass_type,
                    int pass_filter,
                    float result[]);

/* bake.c */
int RE_pass_depth(eScenePassType pass_type);

bool RE_bake_pixels_populate_from_objects(struct Mesh *me_low,
                                          BakePixel pixel_array_from[],
                                          BakePixel pixel_array_to[],
                                          BakeHighPolyData highpoly[],
                                          int tot_highpoly,
                                          size_t num_pixels,
                                          bool is_custom_cage,
                                          float cage_extrusion,
                                          float max_ray_distance,
                                          float mat_low[4][4],
                                          float mat_cage[4][4],
                                          struct Mesh *me_cage);

void RE_bake_pixels_populate(struct Mesh *me,
                             struct BakePixel *pixel_array,
                             size_t num_pixels,
                             const struct BakeTargets *targets,
                             const char *uv_layer);

void RE_bake_mask_fill(const BakePixel pixel_array[], size_t num_pixels, char *mask);

void RE_bake_margin(struct ImBuf *ibuf,
                    char *mask,
                    int margin,
                    char margin_type,
                    struct Mesh const *me,
                    char const *uv_layer);

void RE_bake_normal_world_to_object(const BakePixel pixel_array[],
                                    size_t num_pixels,
                                    int depth,
                                    float result[],
                                    struct Object *ob,
                                    const eBakeNormalSwizzle normal_swizzle[3]);
/**
 * This function converts an object space normal map
 * to a tangent space normal map for a given low poly mesh.
 */
void RE_bake_normal_world_to_tangent(const BakePixel pixel_array[],
                                     size_t num_pixels,
                                     int depth,
                                     float result[],
                                     struct Mesh *me,
                                     const eBakeNormalSwizzle normal_swizzle[3],
                                     float mat[4][4]);
void RE_bake_normal_world_to_world(const BakePixel pixel_array[],
                                   size_t num_pixels,
                                   int depth,
                                   float result[],
                                   const eBakeNormalSwizzle normal_swizzle[3]);

void RE_bake_ibuf_clear(struct Image *image, bool is_tangent);

#ifdef __cplusplus
}
#endif
