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

#include "internal/topology/topology_refiner_impl.h"

#include "internal/base/type.h"
#include "internal/base/type_convert.h"
#include "internal/topology/mesh_topology.h"
#include "internal/topology/topology_refiner_impl.h"

#include "opensubdiv_converter_capi.h"

namespace blender {
namespace opensubdiv {
namespace {

const OpenSubdiv::Far::TopologyRefiner *getOSDTopologyRefiner(
    const TopologyRefinerImpl *topology_refiner_impl)
{
  return topology_refiner_impl->topology_refiner;
}

const OpenSubdiv::Far::TopologyLevel &getOSDTopologyBaseLevel(
    const TopologyRefinerImpl *topology_refiner_impl)
{
  return getOSDTopologyRefiner(topology_refiner_impl)->GetLevel(0);
}

////////////////////////////////////////////////////////////////////////////////
// Quick preliminary checks.

bool checkSchemeTypeMatches(const TopologyRefinerImpl *topology_refiner_impl,
                            const OpenSubdiv_Converter *converter)
{
  const OpenSubdiv::Sdc::SchemeType converter_scheme_type =
      blender::opensubdiv::getSchemeTypeFromCAPI(converter->getSchemeType(converter));
  return (converter_scheme_type == getOSDTopologyRefiner(topology_refiner_impl)->GetSchemeType());
}

bool checkOptionsMatches(const TopologyRefinerImpl *topology_refiner_impl,
                         const OpenSubdiv_Converter *converter)
{
  typedef OpenSubdiv::Sdc::Options Options;
  const Options options = getOSDTopologyRefiner(topology_refiner_impl)->GetSchemeOptions();
  const Options::FVarLinearInterpolation fvar_interpolation = options.GetFVarLinearInterpolation();
  const Options::FVarLinearInterpolation converter_fvar_interpolation =
      blender::opensubdiv::getFVarLinearInterpolationFromCAPI(
          converter->getFVarLinearInterpolation(converter));
  if (fvar_interpolation != converter_fvar_interpolation) {
    return false;
  }
  return true;
}

bool checkGeometryCountersMatches(const TopologyRefinerImpl *topology_refiner_impl,
                                  const OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::TopologyLevel;
  const TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner_impl);
  return ((converter->getNumVertices(converter) == base_level.GetNumVertices()) &&
          (converter->getNumEdges(converter) == base_level.GetNumEdges()) &&
          (converter->getNumFaces(converter) == base_level.GetNumFaces()));
}

bool checkPreliminaryMatches(const TopologyRefinerImpl *topology_refiner_impl,
                             const OpenSubdiv_Converter *converter)
{
  return checkSchemeTypeMatches(topology_refiner_impl, converter) &&
         checkOptionsMatches(topology_refiner_impl, converter) &&
         checkGeometryCountersMatches(topology_refiner_impl, converter);
}

////////////////////////////////////////////////////////////////////////////////
// Geometry comparison.

// A thin wrapper around index like array which does cyclic access. This means,
// it basically does indices[requested_index % num_indices].
//
// NOTE: This array does not own the memory.
//
// TODO(sergey): Consider moving this to a more reusable place.
class CyclicArray {
 public:
  typedef int value_type;
  typedef int size_type;
  static constexpr size_type npos = -1;

  explicit CyclicArray(const std::vector<int> &data) : data_(data.data()), size_(data.size())
  {
  }

  explicit CyclicArray(const OpenSubdiv::Far::ConstIndexArray &data)
      : data_(&data[0]), size_(data.size())
  {
  }

  inline value_type operator[](int index) const
  {
    assert(index >= 0);
    // TODO(sergey): Check whether doing check for element index exceeding total
    // number of indices prior to modulo helps performance.
    return data_[index % size()];
  }

  inline size_type size() const
  {
    return size_;
  }

  // Find index of first occurrence of a given value.
  inline size_type find(const value_type value) const
  {
    const int num_indices = size();
    for (size_type i = 0; i < num_indices; ++i) {
      if (value == (*this)[i]) {
        return i;
      }
    }
    return npos;
  }

 protected:
  const value_type *data_;
  const size_type size_;
};

bool compareCyclicForward(const CyclicArray &array_a,
                          const int start_a,
                          const CyclicArray &array_b,
                          const int start_b)
{
  const int num_elements = array_a.size();
  for (int i = 0; i < num_elements; ++i) {
    if (array_a[start_a + i] != array_b[start_b + i]) {
      return false;
    }
  }
  return true;
}

bool compareCyclicBackward(const CyclicArray &array_a,
                           const int start_a,
                           const CyclicArray &array_b,
                           const int start_b)
{
  const int num_elements = array_a.size();
  // TODO(sergey): Some optimization might be possible with memcmp trickery.
  for (int i = 0; i < num_elements; ++i) {
    if (array_a[start_a + (num_elements - i - 1)] != array_b[start_b + (num_elements - i - 1)]) {
      return false;
    }
  }
  return true;
}

// Utility function dedicated for checking whether whether vertices indices
// used by two faces match.
// The tricky part here is that we can't trust 1:1 array match here, since it's
// possible that OpenSubdiv oriented edges of a face to make it compatible with
// an internal representation of non-manifold meshes.
//
// TODO(sergey): Check whether this is needed, ot whether OpenSubdiv is only
// creating edges in a proper orientation without modifying indices of face
// vertices.
bool checkVerticesOfFacesMatch(const CyclicArray &indices_a, const CyclicArray &indices_b)
{
  if (indices_a.size() != indices_b.size()) {
    return false;
  }
  // "Align" the arrays so we know first matched element.
  const int start_b = indices_b.find(indices_a[0]);
  if (start_b == indices_b.npos) {
    return false;
  }
  // Check match in both directions, for the case OpenSubdiv did orient face in
  // a way which made normals more consistent internally.
  if (compareCyclicForward(indices_a, 0, indices_b, start_b)) {
    return true;
  }
  if (compareCyclicBackward(indices_a, 0, indices_b, start_b)) {
    return true;
  }
  return false;
}

bool checkGeometryFacesMatch(const TopologyRefinerImpl *topology_refiner_impl,
                             const OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::ConstIndexArray;
  using OpenSubdiv::Far::TopologyLevel;
  const TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner_impl);
  const int num_faces = base_level.GetNumFaces();
  // TODO(sergey): Consider using data structure which keeps handful of
  // elements on stack before doing heep allocation.
  vector<int> conv_face_vertices;
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    const ConstIndexArray &face_vertices = base_level.GetFaceVertices(face_index);
    const int num_face_vertices = face_vertices.size();
    if (num_face_vertices != converter->getNumFaceVertices(converter, face_index)) {
      return false;
    }
    conv_face_vertices.resize(num_face_vertices);
    converter->getFaceVertices(converter, face_index, &conv_face_vertices[0]);
    if (!checkVerticesOfFacesMatch(CyclicArray(conv_face_vertices), CyclicArray(face_vertices))) {
      return false;
    }
  }
  return true;
}

bool checkGeometryMatches(const TopologyRefinerImpl *topology_refiner_impl,
                          const OpenSubdiv_Converter *converter)
{
  return checkGeometryFacesMatch(topology_refiner_impl, converter);
}

////////////////////////////////////////////////////////////////////////////////
// Compare attributes which affects on topology.

bool checkSingleUVLayerMatch(const OpenSubdiv::Far::TopologyLevel &base_level,
                             const OpenSubdiv_Converter *converter,
                             const int layer_index)
{
  converter->precalcUVLayer(converter, layer_index);
  const int num_faces = base_level.GetNumFaces();
  // TODO(sergey): Need to check whether converter changed the winding of
  // face to match OpenSubdiv's expectations.
  for (int face_index = 0; face_index < num_faces; ++face_index) {
    OpenSubdiv::Far::ConstIndexArray base_level_face_uvs = base_level.GetFaceFVarValues(
        face_index, layer_index);
    for (int corner = 0; corner < base_level_face_uvs.size(); ++corner) {
      const int uv_index = converter->getFaceCornerUVIndex(converter, face_index, corner);
      if (base_level_face_uvs[corner] != uv_index) {
        converter->finishUVLayer(converter);
        return false;
      }
    }
  }
  converter->finishUVLayer(converter);
  return true;
}

bool checkUVLayersMatch(const TopologyRefinerImpl *topology_refiner_impl,
                        const OpenSubdiv_Converter *converter)
{
  using OpenSubdiv::Far::TopologyLevel;
  const int num_layers = converter->getNumUVLayers(converter);
  const TopologyLevel &base_level = getOSDTopologyBaseLevel(topology_refiner_impl);
  // Number of UV layers should match.
  if (base_level.GetNumFVarChannels() != num_layers) {
    return false;
  }
  for (int layer_index = 0; layer_index < num_layers; ++layer_index) {
    if (!checkSingleUVLayerMatch(base_level, converter, layer_index)) {
      return false;
    }
  }
  return true;
}

bool checkTopologyAttributesMatch(const TopologyRefinerImpl *topology_refiner_impl,
                                  const OpenSubdiv_Converter *converter)
{
  return checkUVLayersMatch(topology_refiner_impl, converter);
}

}  // namespace

bool TopologyRefinerImpl::isEqualToConverter(const OpenSubdiv_Converter *converter) const
{
  if (!blender::opensubdiv::checkPreliminaryMatches(this, converter)) {
    return false;
  }
  if (!blender::opensubdiv::checkGeometryMatches(this, converter)) {
    return false;
  }
  if (!blender::opensubdiv::checkTopologyAttributesMatch(this, converter)) {
    return false;
  }

  if (!base_mesh_topology.isEqualToConverter(converter)) {
    return false;
  }

  return true;
}

}  // namespace opensubdiv
}  // namespace blender
