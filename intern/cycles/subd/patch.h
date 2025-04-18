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

  virtual void eval(
      float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v) const = 0;

  int patch_index = 0;
  int shader = 0;
  bool smooth = true;
  bool from_ngon = false;
};

/* Linear Quad Patch */

class LinearQuadPatch final : public Patch {
 public:
  float3 hull[4];

  void eval(
      float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v) const override;
  BoundBox bound();
};

/* Bicubic Patch */

class BicubicPatch final : public Patch {
 public:
  float3 hull[16];

  void eval(
      float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v) const override;
  BoundBox bound();
};

CCL_NAMESPACE_END
