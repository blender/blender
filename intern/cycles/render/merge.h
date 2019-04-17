/*
 * Copyright 2011-2019 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MERGE_H__
#define __MERGE_H__

#include "util/util_string.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

/* Merge OpenEXR multilayer renders. */

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

#endif /* __MERGE_H__ */
