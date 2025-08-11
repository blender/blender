/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

#include "BLI_set.hh"
#include "BLI_vector.hh"

struct Image;
struct DerivedMesh;
struct MultiresBakeRender;
struct Scene;

struct MultiresBakeRender {
  Scene *scene;
  DerivedMesh *lores_dm, *hires_dm;
  int bake_margin;
  char bake_margin_type;
  int lvl, tot_lvl;
  short mode;
  bool use_lores_mesh; /* Use low-resolution mesh when baking displacement maps */

  /* material aligned image array (for per-face bake image) */
  blender::Vector<Image *> ob_image;

  float bias; /* Bias between object and start ray point when doing AO baking */

  int tot_obj;
  blender::Set<Image *> images;

  int baked_objects, baked_faces;

  int threads; /* Number of threads to be used for baking */

  bool *stop;
  bool *do_update;
  float *progress;
};

void RE_multires_bake_images(struct MultiresBakeRender *bkr);
