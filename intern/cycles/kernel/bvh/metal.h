/*
 * Copyright 2021 Blender Foundation
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

struct MetalRTIntersectionPayload {
  RaySelfPrimitives self;
  uint visibility;
  float u, v;
  int prim;
  int type;
#if defined(__METALRT_MOTION__)
  float time;
#endif
};

struct MetalRTIntersectionLocalPayload {
  RaySelfPrimitives self;
  uint local_object;
  uint lcg_state;
  short max_hits;
  bool has_lcg_state;
  bool result;
  LocalIntersection local_isect;
};

struct MetalRTIntersectionShadowPayload {
  RaySelfPrimitives self;
  uint visibility;
#if defined(__METALRT_MOTION__)
  float time;
#endif
  int state;
  float throughput;
  short max_hits;
  short num_hits;
  short num_recorded_hits;
  bool result;
};
