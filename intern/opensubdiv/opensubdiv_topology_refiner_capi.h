// Copyright 2018 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#ifndef OPENSUBDIV_TOPOLOGY_REFINER_CAPI_H_
#define OPENSUBDIV_TOPOLOGY_REFINER_CAPI_H_

#include <stdint.h>  // for bool

#include "opensubdiv_capi_type.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenSubdiv_Converter;
struct OpenSubdiv_TopologyRefinerInternal;

// Those settings don't really belong to OpenSubdiv's topology refiner, but
// we are keeping track of them on our side of topology refiner. This is to
// make it possible to ensure we are not trying to abuse same OpenSubdiv's
// topology refiner with different subdivision levels or with different
// adaptive settings.
typedef struct OpenSubdiv_TopologyRefinerSettings {
  bool is_adaptive;
  int level;
} OpenSubdiv_TopologyRefinerSettings;

typedef struct OpenSubdiv_TopologyRefiner {
  // Query subdivision level the refiner is created for.
  int (*getSubdivisionLevel)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner);
  bool (*getIsAdaptive)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner);

  // NOTE: All queries are querying base level.
  //
  // TODO(sergey): Consider making it more obvious in function naming,
  // but since it's unlikely (or at least, will be uncommon use) for API
  // which queries final geometry, we should be fine with this name for
  // now.

  //////////////////////////////////////////////////////////////////////////////
  // Query basic topology information from base level.

  int (*getNumVertices)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner);
  int (*getNumEdges)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner);
  int (*getNumFaces)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner);
  int (*getNumFaceVertices)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner,
      const int face_index);

  //////////////////////////////////////////////////////////////////////////////
  // PTex face geometry queries.

  // Ptex face corresponds to OpenSubdiv's internal "patch" and to Blender's
  // subdivision grid. The rule commes as:
  // - Triangle face consist of 3 ptex faces, ordered in the order of
  //   face-vertices.
  // - Quad face consists of a single ptex face.
  // - N-gons (similar to triangle) consists of N ptex faces, ordered same
  //   way as for triangle.
  int (*getNumFacePtexFaces)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner,
      const int face_index);
  int (*getNumPtexFaces)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner);

  // Initialize a per-base-face offset measured in ptex face indices.
  //
  // Basically, face_ptex_offset[base_face_index] is a total number of ptex
  // faces created for bases faces [0 .. base_face_index - 1].
  //
  // The array must contain at least total number of ptex faces elements.
  void (*fillFacePtexIndexOffset)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner,
      int* face_ptex_index_offset);

  //////////////////////////////////////////////////////////////////////////////
  // Face-varying data.

  // Number of face-varying channels (or how they are called in Blender layers).
  int (*getNumFVarChannels)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner);
  // Get face-varying interpolation type.
  OpenSubdiv_FVarLinearInterpolation (*getFVarLinearInterpolation)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner);
  // Get total number of face-varying values in a particular channel.
  int (*getNumFVarValues)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner,
      const int channel);
  // Get face-varying value indices associated with a particular face.
  //
  // This is an array of indices inside of face-varying array, array elements
  // are aligned with face corners (or loops in Blender terminology).
  const int* (*getFaceFVarValueIndices)(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner,
      const int face_index,
      const int channel);

  //////////////////////////////////////////////////////////////////////////////
  // Internal use.

  // Internal storage for the use in this module only.
  //
  // Tease: Contains actual OpenSubdiv's refiner and (optionally) some other
  // data and state needed for an internbal use.
  struct OpenSubdiv_TopologyRefinerInternal* internal;
} OpenSubdiv_TopologyRefiner;

// NOTE: Will return NULL in cases of bad topology.
// NOTE: Mesh without faces is considered a bad topology.
OpenSubdiv_TopologyRefiner* openSubdiv_createTopologyRefinerFromConverter(
    struct OpenSubdiv_Converter* converter,
    const OpenSubdiv_TopologyRefinerSettings* settings);

void openSubdiv_deleteTopologyRefiner(
    OpenSubdiv_TopologyRefiner* topology_refiner);

// Compare given topology refiner with converter. Returns truth if topology
// refiner matches given converter, false otherwise.
//
// This allows users to construct converter (which is supposed to be cheap)
// and compare with existing refiner before going into more computationally
// complicated parts of subdivision process.
bool openSubdiv_topologyRefinerCompareWithConverter(
    const OpenSubdiv_TopologyRefiner* topology_refiner,
    const struct OpenSubdiv_Converter* converter);

#ifdef __cplusplus
}
#endif

#endif  // OPENSUBDIV_TOPOLOGY_REFINER_CAPI_H_
