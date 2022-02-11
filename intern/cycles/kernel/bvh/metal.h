/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

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
