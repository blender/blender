/*
 * Copyright 2019 Blender Foundation
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

#ifndef __BLENDER_VIEWPORT_H__
#define __BLENDER_VIEWPORT_H__

#include "MEM_guardedalloc.h"
#include "RNA_access.h"
#include "RNA_blender_cpp.h"
#include "RNA_types.h"

#include "render/film.h"
#include "util/util_param.h"

CCL_NAMESPACE_BEGIN

class BlenderViewportParameters {
 private:
  bool use_scene_world;
  bool use_scene_lights;
  float studiolight_rotate_z;
  float studiolight_intensity;
  float studiolight_background_alpha;
  ustring studiolight_path;

  BlenderViewportParameters();
  BlenderViewportParameters(BL::SpaceView3D &b_v3d);

  const bool modified(const BlenderViewportParameters &other) const;
  const bool custom_viewport_parameters() const;
  friend class BlenderSync;

 public:
  /* Retrieve the render pass that needs to be displayed on the given `SpaceView3D`
   * When the `b_v3d` parameter is not given `PASS_NONE` will be returned. */
  static PassType get_viewport_display_render_pass(BL::SpaceView3D &b_v3d);
};

PassType update_viewport_display_passes(BL::SpaceView3D &b_v3d, vector<Pass> &passes);

CCL_NAMESPACE_END

#endif
