/*
 * Copyright 2011-2016 Blender Foundation
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

#ifndef __BLENDER_OBJECT_CULL_H__
#define __BLENDER_OBJECT_CULL_H__

#include "blender/blender_sync.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

class Scene;

class BlenderObjectCulling {
 public:
  BlenderObjectCulling(Scene *scene, BL::Scene &b_scene);

  void init_object(Scene *scene, BL::Object &b_ob);
  bool test(Scene *scene, BL::Object &b_ob, Transform &tfm);

 private:
  bool test_camera(Scene *scene, float3 bb[8]);
  bool test_distance(Scene *scene, float3 bb[8]);

  bool use_scene_camera_cull_;
  bool use_camera_cull_;
  float camera_cull_margin_;
  bool use_scene_distance_cull_;
  bool use_distance_cull_;
  float distance_cull_margin_;
};

CCL_NAMESPACE_END

#endif /* __BLENDER_OBJECT_CULL_H__ */
