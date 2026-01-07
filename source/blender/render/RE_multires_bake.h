/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#pragma once

#include "DNA_scene_types.h"

#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "RE_pipeline.h"

namespace blender {

struct Image;
struct Mesh;
struct MultiresBakeRender;
struct MultiresModifierData;

struct MultiresBakeRender {
  /* Base mesh at the input of the multiresolution modifier and data of the modifier which is being
   * baked. */
  Mesh *base_mesh = nullptr;
  MultiresModifierData *multires_modifier = nullptr;

  int bake_margin = 0;
  eBakeMarginType bake_margin_type = R_BAKE_ADJACENT_FACES;
  eBakeType type = R_BAKE_NORMALS;
  eBakeSpace displacement_space = R_BAKE_SPACE_OBJECT;

  /* Use low-resolution mesh when baking displacement maps.
   * When true displacement is calculated between the final position in the SubdivCCG and the
   * corresponding location on the base mesh.
   * When false displacement is calculated between the final position in the SubdivCCG and the
   * multiresolution modifier calculated at the bake level, further subdivided (without adding
   * displacement) to the final multi-resolution level. */
  bool use_low_resolution_mesh = false;

  /* Material aligned image array (for per-face bake image), */
  Vector<Image *> ob_image;

  Set<Image *> images;

  int num_total_objects = 0;
  int num_baked_objects = 0;

  bool *stop = nullptr;
  bool *do_update = nullptr;
  float *progress = nullptr;
};

void RE_multires_bake_images(MultiresBakeRender &bake);

}  // namespace blender
