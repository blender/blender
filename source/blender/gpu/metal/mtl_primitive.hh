/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Encapsulation of Frame-buffer states (attached textures, viewport, scissors).
 */

#pragma once

#include "BLI_assert.h"

#include "GPU_primitive.hh"

#include <Metal/Metal.h>

namespace blender::gpu {

/** Utility functions **/
static inline MTLPrimitiveTopologyClass mtl_prim_type_to_topology_class(MTLPrimitiveType prim_type)
{
  switch (prim_type) {
    case MTLPrimitiveTypePoint:
      return MTLPrimitiveTopologyClassPoint;
    case MTLPrimitiveTypeLine:
    case MTLPrimitiveTypeLineStrip:
      return MTLPrimitiveTopologyClassLine;
    case MTLPrimitiveTypeTriangle:
    case MTLPrimitiveTypeTriangleStrip:
      return MTLPrimitiveTopologyClassTriangle;
  }
  return MTLPrimitiveTopologyClassUnspecified;
}

static inline MTLPrimitiveType gpu_prim_type_to_metal(GPUPrimType prim_type)
{
  switch (prim_type) {
    case GPU_PRIM_POINTS:
      return MTLPrimitiveTypePoint;
    case GPU_PRIM_LINES:
    case GPU_PRIM_LINES_ADJ:
      return MTLPrimitiveTypeLine;
    case GPU_PRIM_LINE_STRIP:
    case GPU_PRIM_LINE_STRIP_ADJ:
    case GPU_PRIM_LINE_LOOP:
      return MTLPrimitiveTypeLineStrip;
    case GPU_PRIM_TRIS:
    case GPU_PRIM_TRI_FAN:
    case GPU_PRIM_TRIS_ADJ:
      return MTLPrimitiveTypeTriangle;
    case GPU_PRIM_TRI_STRIP:
      return MTLPrimitiveTypeTriangleStrip;
    case GPU_PRIM_NONE:
      return MTLPrimitiveTypePoint;
  };
}

/* Certain primitive types are not supported in Metal, and require emulation.
 * `GPU_PRIM_LINE_LOOP` and  `GPU_PRIM_TRI_FAN` required index buffer patching.
 * Adjacency types do not need emulation as the input structure is the same,
 * and access is controlled from the vertex shader through SSBO vertex fetch.
 * -- These Adj cases are only used in geometry shaders in OpenGL. */
static inline bool mtl_needs_topology_emulation(GPUPrimType prim_type)
{

  BLI_assert(prim_type != GPU_PRIM_NONE);
  switch (prim_type) {
    case GPU_PRIM_LINE_LOOP:
    case GPU_PRIM_TRI_FAN:
      return true;
    default:
      return false;
  }
  return false;
}

static inline bool mtl_vertex_count_fits_primitive_type(uint32_t vertex_count,
                                                        MTLPrimitiveType prim_type)
{
  if (vertex_count == 0) {
    return false;
  }

  switch (prim_type) {
    case MTLPrimitiveTypeLineStrip:
      return (vertex_count > 1);
    case MTLPrimitiveTypeLine:
      return (vertex_count % 2 == 0);
    case MTLPrimitiveTypePoint:
      return (vertex_count > 0);
    case MTLPrimitiveTypeTriangle:
      return (vertex_count % 3 == 0);
    case MTLPrimitiveTypeTriangleStrip:
      return (vertex_count > 2);
  }
  BLI_assert(false);
  return false;
}

}  // namespace blender::gpu
