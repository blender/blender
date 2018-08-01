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

#include "opensubdiv_topology_refiner_capi.h"

#include <vector>

#include "MEM_guardedalloc.h"
#include "internal/opensubdiv_converter_factory.h"
#include "internal/opensubdiv_converter_internal.h"
#include "internal/opensubdiv_internal.h"
#include "internal/opensubdiv_topology_refiner_internal.h"

namespace {

const OpenSubdiv::Far::TopologyRefiner* getOSDTopologyRefiner(
    const OpenSubdiv_TopologyRefiner* topology_refiner) {
  return topology_refiner->internal->osd_topology_refiner;
}

const OpenSubdiv::Far::TopologyLevel* getOSDTopologyBaseLevel(
    const OpenSubdiv_TopologyRefiner* topology_refiner) {
  return &getOSDTopologyRefiner(topology_refiner)->GetLevel(0);
}

int getSubdivisionLevel(const OpenSubdiv_TopologyRefiner* topology_refiner) {
  return topology_refiner->internal->settings.level;
}

bool getIsAdaptive(const OpenSubdiv_TopologyRefiner* topology_refiner) {
  return topology_refiner->internal->settings.is_adaptive;
}

////////////////////////////////////////////////////////////////////////////////
// Query basic topology information from base level.

int getNumVertices(const OpenSubdiv_TopologyRefiner* topology_refiner) {
  return getOSDTopologyBaseLevel(topology_refiner)->GetNumVertices();
}

int getNumEdges(const OpenSubdiv_TopologyRefiner* topology_refiner) {
  return getOSDTopologyBaseLevel(topology_refiner)->GetNumEdges();
}

int getNumFaces(const OpenSubdiv_TopologyRefiner* topology_refiner) {
  return getOSDTopologyBaseLevel(topology_refiner)->GetNumFaces();
}

////////////////////////////////////////////////////////////////////////////////
// PTex face geometry queries.

int getNumFaceVertices(const OpenSubdiv_TopologyRefiner* topology_refiner,
                       const int face_index) {
  const OpenSubdiv::Far::TopologyLevel* base_level =
      getOSDTopologyBaseLevel(topology_refiner);
  return base_level->GetFaceVertices(face_index).size();
}

int getNumFacePtexFaces(const OpenSubdiv_TopologyRefiner* topology_refiner,
                        const int face_index) {
  const int num_face_vertices =
      topology_refiner->getNumFaceVertices(topology_refiner, face_index);
  if (num_face_vertices == 4) {
    return 1;
  } else {
    return num_face_vertices;
  }
}

int getNumPtexFaces(const OpenSubdiv_TopologyRefiner* topology_refiner) {
  const int num_faces = topology_refiner->getNumFaces(topology_refiner);
  int num_ptex_faces = 0;
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    num_ptex_faces +=
        topology_refiner->getNumFacePtexFaces(topology_refiner, face_index);
  }
  return num_ptex_faces;
}

void fillFacePtexIndexOffset(const OpenSubdiv_TopologyRefiner* topology_refiner,
                             int* face_ptex_index_offset) {
  const int num_faces = topology_refiner->getNumFaces(topology_refiner);
  int num_ptex_faces = 0;
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    face_ptex_index_offset[face_index] = num_ptex_faces;
    num_ptex_faces +=
        topology_refiner->getNumFacePtexFaces(topology_refiner, face_index);
  }
}

//////////////////////////////////////////////////////////////////////////////
// Face-varying data.

int getNumFVarChannels(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner) {
  const OpenSubdiv::Far::TopologyLevel* base_level =
      getOSDTopologyBaseLevel(topology_refiner);
  return base_level->GetNumFVarChannels();
}

OpenSubdiv_FVarLinearInterpolation getFVarLinearInterpolation(
    const struct OpenSubdiv_TopologyRefiner* topology_refiner) {
  return opensubdiv_capi::getCAPIFVarLinearInterpolationFromOSD(
    getOSDTopologyRefiner(topology_refiner)->GetFVarLinearInterpolation());
}

int getNumFVarValues(
      const struct OpenSubdiv_TopologyRefiner* topology_refiner,
      const int channel) {
  const OpenSubdiv::Far::TopologyLevel* base_level =
      getOSDTopologyBaseLevel(topology_refiner);
  return base_level->GetNumFVarValues(channel);
}

const int* getFaceFVarValueIndices(
    const struct OpenSubdiv_TopologyRefiner* topology_refiner,
    const int face_index,
    const int channel) {
  const OpenSubdiv::Far::TopologyLevel* base_level =
      getOSDTopologyBaseLevel(topology_refiner);
  return &base_level->GetFaceFVarValues(face_index, channel)[0];
}

//////////////////////////////////////////////////////////////////////////////
// Internal helpers.

void assignFunctionPointers(OpenSubdiv_TopologyRefiner* topology_refiner) {
  topology_refiner->getSubdivisionLevel = getSubdivisionLevel;
  topology_refiner->getIsAdaptive = getIsAdaptive;
  // Basic topology information.
  topology_refiner->getNumVertices = getNumVertices;
  topology_refiner->getNumEdges = getNumEdges;
  topology_refiner->getNumFaces = getNumFaces;
  topology_refiner->getNumFaceVertices = getNumFaceVertices;
  // PTex face geometry.
  topology_refiner->getNumFacePtexFaces = getNumFacePtexFaces;
  topology_refiner->getNumPtexFaces = getNumPtexFaces;
  topology_refiner->fillFacePtexIndexOffset = fillFacePtexIndexOffset;
  // Face-varying data.
  topology_refiner->getNumFVarChannels = getNumFVarChannels;
  topology_refiner->getFVarLinearInterpolation = getFVarLinearInterpolation;
  topology_refiner->getNumFVarValues = getNumFVarValues;
  topology_refiner->getFaceFVarValueIndices = getFaceFVarValueIndices;
}

OpenSubdiv_TopologyRefiner* allocateTopologyRefiner() {
  OpenSubdiv_TopologyRefiner* topology_refiner =
      OBJECT_GUARDED_NEW(OpenSubdiv_TopologyRefiner);
  topology_refiner->internal =
      OBJECT_GUARDED_NEW(OpenSubdiv_TopologyRefinerInternal);
  assignFunctionPointers(topology_refiner);
  return topology_refiner;
}

}  // namespace

OpenSubdiv_TopologyRefiner* openSubdiv_createTopologyRefinerFromConverter(
    OpenSubdiv_Converter* converter,
    const OpenSubdiv_TopologyRefinerSettings* settings) {
  OpenSubdiv::Far::TopologyRefiner* osd_topology_refiner =
      opensubdiv_capi::createOSDTopologyRefinerFromConverter(converter);
  if (osd_topology_refiner == NULL) {
    // Happens on empty or bad topology.
    return NULL;
  }
  OpenSubdiv_TopologyRefiner* topology_refiner = allocateTopologyRefiner();
  topology_refiner->internal->osd_topology_refiner = osd_topology_refiner;
  // Store setting which we want to keep track of and which can not be stored
  // in OpenSubdiv's descriptor yet.
  topology_refiner->internal->settings = *settings;
  return topology_refiner;
}

void openSubdiv_deleteTopologyRefiner(
    OpenSubdiv_TopologyRefiner* topology_refiner) {
  OBJECT_GUARDED_DELETE(topology_refiner->internal,
                        OpenSubdiv_TopologyRefinerInternal);
  OBJECT_GUARDED_DELETE(topology_refiner, OpenSubdiv_TopologyRefiner);
}

////////////////////////////////////////////////////////////////////////////////
// Comparison with converter.

namespace {

///////////////////////////////////////////////////////////
// Quick preliminary checks.

bool checkSchemeTypeMatches(
    const OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const OpenSubdiv_Converter* converter) {
  const OpenSubdiv::Sdc::SchemeType converter_scheme_type =
      opensubdiv_capi::getSchemeTypeFromCAPI(
          converter->getSchemeType(converter));
  return (converter_scheme_type == topology_refiner->GetSchemeType());
}

bool checkOptionsMatches(
    const OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const OpenSubdiv_Converter* converter) {
  typedef OpenSubdiv::Sdc::Options Options;
  const Options options = topology_refiner->GetSchemeOptions();
  const Options::FVarLinearInterpolation fvar_interpolation =
      options.GetFVarLinearInterpolation();
  const Options::FVarLinearInterpolation converter_fvar_interpolation =
      opensubdiv_capi::getFVarLinearInterpolationFromCAPI(
          converter->getFVarLinearInterpolation(converter));
  if (fvar_interpolation != converter_fvar_interpolation) {
    return false;
  }
  return true;
}

bool checkGeometryCoountersMatches(
    const OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const OpenSubdiv_Converter* converter) {
  using OpenSubdiv::Far::TopologyLevel;
  const TopologyLevel& base_level = topology_refiner->GetLevel(0);
  return (
      (converter->getNumVertices(converter) == base_level.GetNumVertices()) &&
      (converter->getNumEdges(converter) == base_level.GetNumEdges()) &&
      (converter->getNumFaces(converter) == base_level.GetNumFaces()));
}

bool checkPreliminaryMatches(
    const OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const OpenSubdiv_Converter* converter) {
  return checkSchemeTypeMatches(topology_refiner, converter) &&
         checkOptionsMatches(topology_refiner, converter) &&
         checkGeometryCoountersMatches(topology_refiner, converter);
}

///////////////////////////////////////////////////////////
// Geometry comparison.

bool checkGeometryEdgesMatch(
    const OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const OpenSubdiv_Converter* converter) {
  using OpenSubdiv::Far::ConstIndexArray;
  using OpenSubdiv::Far::TopologyLevel;
  const TopologyLevel& base_level = topology_refiner->GetLevel(0);
  const int num_edges = base_level.GetNumEdges();
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    const ConstIndexArray& edge_vertices =
        base_level.GetEdgeVertices(edge_index);
    int conv_edge_vertices[2];
    converter->getEdgeVertices(converter, edge_index, conv_edge_vertices);
    if (conv_edge_vertices[0] != edge_vertices[0] ||
        conv_edge_vertices[1] != edge_vertices[1]) {
      return false;
    }
  }
  return true;
}

bool checkGeometryFacesMatch(
    const OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const OpenSubdiv_Converter* converter) {
  using OpenSubdiv::Far::ConstIndexArray;
  using OpenSubdiv::Far::TopologyLevel;
  const TopologyLevel& base_level = topology_refiner->GetLevel(0);
  const int num_faces = base_level.GetNumFaces();
  // TODO(sergey): Consider using data structure which keeps handful of
  // elements on stack before doing heep allocation.
  std::vector<int> conv_face_vertices;
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    const ConstIndexArray& face_vertices =
        base_level.GetFaceVertices(face_index);
    const int num_face_vertices = face_vertices.size();
    if (num_face_vertices !=
        converter->getNumFaceVertices(converter, face_index)) {
      return false;
    }
    conv_face_vertices.resize(num_face_vertices);
    converter->getFaceVertices(converter, face_index, &conv_face_vertices[0]);
    // Check face-vertex indices in the direct order (assuming topology
    // orientation is disabled or did not flip order of the face-vertices).
    //
    // TODO(sergey): Can we simply memcmp() with OpenSubdiv's array?
    bool direct_match = true;
    for (int face_vertex_index = 0; face_vertex_index < num_face_vertices;
         ++face_vertex_index) {
      if (conv_face_vertices[face_vertex_index] !=
          face_vertices[face_vertex_index]) {
        direct_match = false;
        break;
      }
    }
    if (!direct_match) {
// If face didn't match in direct direction we also test if it matches in
// reversed direction. This is because conversion might reverse loops to
// make normals consistent.
#ifdef OPENSUBDIV_ORIENT_TOPOLOGY
      for (int face_vertex_index = 0; face_vertex_index < num_face_vertices;
           ++face_vertex_index) {
        if (conv_face_vertices[face_vertex_index] !=
            face_vertices[num_face_vertices - face_vertex_index - 1]) {
          return false;
        }
      }
#endif
      return false;
    }
  }
  return true;
}

bool checkGeometryMatches(
    const OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const OpenSubdiv_Converter* converter) {
  return checkGeometryEdgesMatch(topology_refiner, converter) &&
         checkGeometryFacesMatch(topology_refiner, converter);
}

///////////////////////////////////////////////////////////
// Compare attributes which affects on topology

bool checkEdgeSharpnessMatch(
    const OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const OpenSubdiv_Converter* converter) {
  using OpenSubdiv::Far::ConstIndexArray;
  using OpenSubdiv::Far::TopologyLevel;
  const TopologyLevel& base_level = topology_refiner->GetLevel(0);
  const int num_edges = base_level.GetNumEdges();
  for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
    const float sharpness = base_level.GetEdgeSharpness(edge_index);
    const float conv_sharpness =
        converter->getEdgeSharpness(converter, edge_index);
    if (sharpness != conv_sharpness) {
      return false;
    }
  }
  return false;
}

bool checkTopologyAttributesMatch(
    const OpenSubdiv::Far::TopologyRefiner* topology_refiner,
    const OpenSubdiv_Converter* converter) {
  return checkEdgeSharpnessMatch(topology_refiner, converter);
}

}  // namespace

bool openSubdiv_topologyRefinerCompareWithConverter(
    const OpenSubdiv_TopologyRefiner* topology_refiner,
    const OpenSubdiv_Converter* converter) {
  const OpenSubdiv::Far::TopologyRefiner* refiner =
      getOSDTopologyRefiner(topology_refiner);
  return (checkPreliminaryMatches(refiner, converter) &&
          checkGeometryMatches(refiner, converter) &&
          checkTopologyAttributesMatch(refiner, converter));
}
