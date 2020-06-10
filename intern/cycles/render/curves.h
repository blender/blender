/*
 * Copyright 2011-2013 Blender Foundation
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

#ifndef __CURVES_H__
#define __CURVES_H__

#include "util/util_array.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;

void curvebounds(float *lower, float *upper, float3 *p, int dim);

typedef enum CurveShapeType {
  CURVE_RIBBON = 0,
  CURVE_THICK = 1,

  CURVE_NUM_SHAPE_TYPES,
} CurveShapeType;

class ParticleCurveData {

 public:
  ParticleCurveData();
  ~ParticleCurveData();

  array<int> psys_firstcurve;
  array<int> psys_curvenum;
  array<int> psys_shader;

  array<float> psys_rootradius;
  array<float> psys_tipradius;
  array<float> psys_shape;
  array<bool> psys_closetip;

  array<int> curve_firstkey;
  array<int> curve_keynum;
  array<float> curve_length;
  array<float2> curve_uv;
  array<float3> curve_vcol;

  array<float3> curvekey_co;
  array<float> curvekey_time;
};

/* HairSystem Manager */

class CurveSystemManager {
 public:
  CurveShapeType curve_shape;
  int subdivisions;

  bool use_curves;

  bool need_update;
  bool need_mesh_update;

  CurveSystemManager();
  ~CurveSystemManager();

  void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress &progress);
  void device_free(Device *device, DeviceScene *dscene);
  bool modified(const CurveSystemManager &CurveSystemManager);
  bool modified_mesh(const CurveSystemManager &CurveSystemManager);

  void tag_update(Scene *scene);
  void tag_update_mesh();
};

CCL_NAMESPACE_END

#endif /* __CURVES_H__ */
