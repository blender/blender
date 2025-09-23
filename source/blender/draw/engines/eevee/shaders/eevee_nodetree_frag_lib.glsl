/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once
/* This file must replaced at runtime. The following content is only a possible implementation. */
#pragma runtime_generated

#include "eevee_geom_types_lib.glsl"
#include "eevee_nodetree_lib.glsl"

/* Loading of the attributes into GlobalData. */
void attrib_load(WorldPoint domain) {}
void attrib_load(VolumePoint domain) {}

/* Material graph connected to the displacement output. */
float3 nodetree_displacement()
{
  return float3(0.0f);
}

/* Material graph connected to the surface output. */
Closure nodetree_surface(float closure_rand)
{
  return Closure(0);
}

/* Material graph connected to the volume output. */
Closure nodetree_volume()
{
  return Closure(0);
}

/* Material graph connected to the volume output. */
float nodetree_thickness()
{
  return 0.1f;
}

/* Replaced by define at runtime. */
/* TODO(fclem): Find a way to pass material parameters inside the material UBO. */
float thickness_mode = 1.0f;
