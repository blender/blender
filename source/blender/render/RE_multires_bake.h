/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

struct MultiresBakeRender;
struct Scene;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MultiresBakeRender {
  Scene *scene;
  DerivedMesh *lores_dm, *hires_dm;
  int bake_margin;
  char bake_margin_type;
  int lvl, tot_lvl;
  short mode;
  bool use_lores_mesh; /* Use low-resolution mesh when baking displacement maps */

  /* material aligned image array (for per-face bake image) */
  struct {
    Image **array;
    int len;
  } ob_image;

  int number_of_rays; /* Number of rays to be cast when doing AO baking */
  float bias;         /* Bias between object and start ray point when doing AO baking */

  int tot_obj, tot_image;
  ListBase image;

  int baked_objects, baked_faces;

  int raytrace_structure; /* Optimization structure to be used for AO baking */
  int octree_resolution;  /* Resolution of octree when using octree optimization structure */
  int threads;            /* Number of threads to be used for baking */

  float user_scale; /* User scale used to scale displacement when baking derivative map. */

  bool *stop;
  bool *do_update;
  float *progress;
} MultiresBakeRender;

void RE_multires_bake_images(struct MultiresBakeRender *bkr);

#ifdef __cplusplus
}
#endif
