/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/boundbox.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

class Patch {
 public:
  Patch() = default;

  virtual ~Patch() = default;

  virtual void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v) = 0;

  int patch_index = 0;
  int shader = 0;
  bool from_ngon = false;
};

/* Linear Quad Patch */

class LinearQuadPatch : public Patch {
 public:
  float3 hull[4];
  float3 normals[4];

  void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v) override;
  BoundBox bound();
};

/* Bicubic Patch */

class BicubicPatch : public Patch {
 public:
  float3 hull[16];

  void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v) override;
  BoundBox bound();
};

CCL_NAMESPACE_END
