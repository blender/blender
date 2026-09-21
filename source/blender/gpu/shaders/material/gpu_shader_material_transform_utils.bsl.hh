/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Requires all common matrices declared. */

#pragma once

#include "gpu_shader_material_interface.bsl.hh"

[[node]]
void normal_transform_object_to_world(float3 vin,
                                      [[resource_table]] KernelGlobals &kg,
                                      const ShadingData &sd,
                                      float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  /* Expansion of NormalMatrix. */
  vout = vin * to_float3x3(obj.model_inverse);
}

[[node]]
void normal_transform_world_to_object(float3 vin,
                                      [[resource_table]] KernelGlobals &kg,
                                      const ShadingData &sd,
                                      float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  /* Expansion of NormalMatrixInverse. */
  vout = vin * to_float3x3(obj.model);
}

[[node]]
void normal_transform_object_to_view(float3 vin,
                                     [[resource_table]] KernelGlobals &kg,
                                     const ShadingData &sd,
                                     float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = vin * to_float3x3(obj.model_inverse);
  vout = to_float3x3(view.viewmat) * vout;
}

[[node]]
void normal_transform_view_to_world(float3 vin,
                                    [[resource_table]] KernelGlobals &kg,
                                    const ShadingData &sd,
                                    float3 &vout)
{
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = to_float3x3(view.viewinv) * vin;
}

[[node]]
void normal_transform_view_to_object(float3 vin,
                                     [[resource_table]] KernelGlobals &kg,
                                     const ShadingData &sd,
                                     float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = to_float3x3(view.viewinv) * vin;
  vout = vout * to_float3x3(obj.model);
}

[[node]]
void normal_transform_world_to_view(float3 vin,
                                    [[resource_table]] KernelGlobals &kg,
                                    const ShadingData &sd,
                                    float3 &vout)
{
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = to_float3x3(view.viewmat) * vin;
}

[[node]]
void direction_transform_object_to_world(float3 vin,
                                         [[resource_table]] KernelGlobals &kg,
                                         const ShadingData &sd,
                                         float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  vout = to_float3x3(obj.model) * vin;
}

[[node]]
void direction_transform_object_to_view(float3 vin,
                                        [[resource_table]] KernelGlobals &kg,
                                        const ShadingData &sd,
                                        float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = to_float3x3(obj.model) * vin;
  vout = to_float3x3(view.viewmat) * vout;
}

[[node]]
void direction_transform_view_to_world(float3 vin,
                                       [[resource_table]] KernelGlobals &kg,
                                       const ShadingData &sd,
                                       float3 &vout)
{
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = to_float3x3(view.viewinv) * vin;
}

[[node]]
void direction_transform_view_to_object(float3 vin,
                                        [[resource_table]] KernelGlobals &kg,
                                        const ShadingData &sd,
                                        float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = to_float3x3(view.viewinv) * vin;
  vout = to_float3x3(obj.model_inverse) * vout;
}

[[node]]
void direction_transform_world_to_view(float3 vin,
                                       [[resource_table]] KernelGlobals &kg,
                                       const ShadingData &sd,
                                       float3 &vout)
{
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = to_float3x3(view.viewmat) * vin;
}

[[node]]
void direction_transform_world_to_object(float3 vin,
                                         [[resource_table]] KernelGlobals &kg,
                                         const ShadingData &sd,
                                         float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  vout = to_float3x3(obj.model_inverse) * vin;
}

[[node]]
void point_transform_object_to_world(float3 vin,
                                     [[resource_table]] KernelGlobals &kg,
                                     const ShadingData &sd,
                                     float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  vout = (obj.model * float4(vin, 1.0f)).xyz;
}

[[node]]
void point_transform_object_to_view(float3 vin,
                                    [[resource_table]] KernelGlobals &kg,
                                    const ShadingData &sd,
                                    float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = (view.viewmat * (obj.model * float4(vin, 1.0f))).xyz;
}

[[node]]
void point_transform_view_to_world(float3 vin,
                                   [[resource_table]] KernelGlobals &kg,
                                   const ShadingData &sd,
                                   float3 &vout)
{
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = (view.viewinv * float4(vin, 1.0f)).xyz;
}

[[node]]
void point_transform_view_to_object(float3 vin,
                                    [[resource_table]] KernelGlobals &kg,
                                    const ShadingData &sd,
                                    float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = (obj.model_inverse * (view.viewinv * float4(vin, 1.0f))).xyz;
}

[[node]]
void point_transform_world_to_view(float3 vin,
                                   [[resource_table]] KernelGlobals &kg,
                                   const ShadingData &sd,
                                   float3 &vout)
{
  const ViewMatrices view = kg.view_matrices_get(sd);
  vout = (view.viewmat * float4(vin, 1.0f)).xyz;
}

[[node]]
void point_transform_world_to_object(float3 vin,
                                     [[resource_table]] KernelGlobals &kg,
                                     const ShadingData &sd,
                                     float3 &vout)
{
  const ObjectMatrices obj = kg.object_matrices_get(sd);
  vout = (obj.model_inverse * float4(vin, 1.0f)).xyz;
}
