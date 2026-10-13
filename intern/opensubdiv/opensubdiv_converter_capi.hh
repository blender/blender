/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <span>
#include <utility>

#include "opensubdiv_capi_type.hh"

/**
 * Sharpness value which makes an edge or a vertex infinitely sharp. Mirrors
 * `OpenSubdiv::Sdc::Crease::SHARPNESS_INFINITE`, which can not be included here.
 */
constexpr float OPENSUBDIV_SHARPNESS_INFINITE = 10.0f;

struct OpenSubdiv_Converter {
  int verts_num;
  /**
   * The topology of the mesh to be subdivided. See #Mesh::edges(), #Mesh::face_offsets(), and
   * #Mesh::corner_verts() documentation for the details. Other topology information is currently
   * encoded with callbacks rather than arrays directly.
   */
  std::span<const std::pair<int, int>> edges;
  std::span<const int> face_offsets;
  std::span<const int> corner_verts;

  /**
   * Sharpness (aka crease) of every edge and every vertex, in the
   * `[0, #OPENSUBDIV_SHARPNESS_INFINITE]` range. Vertex sharpness includes the infinite sharpness
   * used to make vertices adjacent to a loose edge sharp.
   *
   * Either aligned with #edges and with the vertices respectively, or empty.
   */
  std::span<const float> edge_sharpness;
  std::span<const float> vert_sharpness;

  struct UVLayer {
    /** Number of distinct UV "vertices" (face-varying values) in the layer. */
    int uvs_num;
    /**
     * The index of every face corner's UV vertex in `[0, uvs_num)`. Corners that use the
     * same mesh vertex and have connected UVs share an index. Aligned with #corner_verts.
     */
    std::span<const int> corner_uv_indices;
  };
  std::span<const UVLayer> uv_layers;

  OpenSubdiv_SchemeType scheme_type;

  OpenSubdiv_VtxBoundaryInterpolation vtx_boundary_interpolation;
  OpenSubdiv_FVarLinearInterpolation fvar_linear_interpolation;

  void (*freeUserData)(const OpenSubdiv_Converter *converter);
  void *user_data;
};
