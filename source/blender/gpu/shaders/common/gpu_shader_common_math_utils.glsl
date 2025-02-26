/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_glsl_cpp_stubs.hh"

void invert_z(vec3 v, out vec3 outv)
{
  v.z = -v.z;
  outv = v;
}

void vector_normalize(vec3 normal, out vec3 outnormal)
{
  outnormal = normalize(normal);
}

void vector_copy(vec3 normal, out vec3 outnormal)
{
  outnormal = normal;
}
