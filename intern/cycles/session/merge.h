/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/string.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* Merge OpenEXR multi-layer renders. */

class ImageMerger {
 public:
  ImageMerger();
  bool run();

  /* Error message after running, in case of failure. */
  string error;

  /* List of image filepaths to merge. */
  vector<string> input;
  /* Output filepath. */
  string output;
};

CCL_NAMESPACE_END
