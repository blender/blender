/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once
/* This file must replaced at runtime. The following content is only a possible implementation. */
#pragma runtime_generated

#include "eevee_geom_types_lib.bsl.hh"
/* Note: Only included if there is a nodetree eval in the vertex shader. */
#include "eevee_nodetree_lib.bsl.hh"

/* Loading of the attributes into GlobalData. */
void attrib_load(MeshVertex /*domain*/) {}
void attrib_load(PointCloudPoint /*domain*/) {}
void attrib_load(CurvesPoint /*domain*/) {}

/* Material graph connected to the displacement output. */
float3 nodetree_displacement_vert(KernelGlobals & /*kg*/, ShadingData & /*sd*/)
{
  return float3(0.0f);
}
