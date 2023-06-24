/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __SUBD_PATCH_H__
#define __SUBD_PATCH_H__

#include "util/boundbox.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

class Patch {
 public:
  Patch() : patch_index(0), shader(0), from_ngon(false) {}

  virtual ~Patch() = default;

  virtual void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, float u, float v) = 0;

  int patch_index;
  int shader;
  bool from_ngon;
};

/* Linear Quad Patch */

class LinearQuadPatch : public Patch {
 public:
  float3 hull[4];
  float3 normals[4];

  void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, float u, float v);
  BoundBox bound();
};

/* Bicubic Patch */

class BicubicPatch : public Patch {
 public:
  float3 hull[16];

  void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, float u, float v);
  BoundBox bound();
};

CCL_NAMESPACE_END

#endif /* __SUBD_PATCH_H__ */
