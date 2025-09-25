/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

#include "BLI_string_ref.hh"

#include "DNA_scene_types.h"

#include "RE_pipeline.h"

struct Depsgraph;
struct ImBuf;
struct Mesh;
struct Render;

struct BakeImage {
  struct Image *image;
  int tile_number;
  float uv_offset[2];
  int width;
  int height;
  size_t offset;

  /* For associating render result layer with image. */
  char render_layer_name[RE_MAXNAME];
};

struct BakeTargets {
  /* All images of the object. */
  BakeImage *images;
  int images_num;

  /* Lookup table from Material number to BakeImage. */
  struct Image **material_to_image;
  int materials_num;

  /* Pixel buffer to bake to. */
  float *result;
  int pixels_num;
  int channels_num;

  /* Baking to non-color data image. */
  bool is_noncolor;
};

struct BakePixel {
  int primitive_id, object_id;
  int seed;
  float uv[2];
  float du_dx, du_dy;
  float dv_dx, dv_dy;
};

struct BakeHighPolyData {
  struct Object *ob;
  struct Object *ob_eval;
  struct Mesh *mesh;
  bool is_flip_object;

  float obmat[4][4];
  float imat[4][4];
};

/* `external_engine.cc` */

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

/* `bake.cc` */

int RE_pass_depth(eScenePassType pass_type);

bool RE_bake_pixels_populate_from_objects(struct Mesh *me_low,
                                          BakePixel pixel_array_from[],
                                          BakePixel pixel_array_to[],
                                          BakeHighPolyData highpoly[],
                                          int highpoly_num,
                                          size_t pixels_num,
                                          bool is_custom_cage,
                                          float cage_extrusion,
                                          float max_ray_distance,
                                          const float mat_low[4][4],
                                          const float mat_cage[4][4],
                                          struct Mesh *me_cage);

void RE_bake_pixels_populate(struct Mesh *mesh,
                             struct BakePixel *pixel_array,
                             size_t pixels_num,
                             const struct BakeTargets *targets,
                             blender::StringRef uv_layer);

void RE_bake_mask_fill(const BakePixel pixel_array[], size_t pixels_num, char *mask);

void RE_bake_margin(struct ImBuf *ibuf,
                    char *mask,
                    int margin,
                    char margin_type,
                    const Mesh *mesh,
                    blender::StringRef uv_layer,
                    const float uv_offset[2]);

void RE_bake_normal_world_to_object(const BakePixel pixel_array[],
                                    size_t pixels_num,
                                    int depth,
                                    float result[],
                                    struct Object *ob,
                                    const eBakeNormalSwizzle normal_swizzle[3]);
/**
 * This function converts an object space normal map
 * to a tangent space normal map for a given low poly mesh.
 */
void RE_bake_normal_world_to_tangent(const BakePixel pixel_array[],
                                     size_t pixels_num,
                                     int depth,
                                     float result[],
                                     struct Mesh *mesh,
                                     const eBakeNormalSwizzle normal_swizzle[3],
                                     const float mat[4][4]);
void RE_bake_normal_world_to_world(const BakePixel pixel_array[],
                                   size_t pixels_num,
                                   int depth,
                                   float result[],
                                   const eBakeNormalSwizzle normal_swizzle[3]);

void RE_bake_ibuf_clear(struct Image *image, bool is_tangent);
