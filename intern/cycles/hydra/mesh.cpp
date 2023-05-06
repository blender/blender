/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2022 NVIDIA Corporation
 * Copyright 2022 Blender Foundation */

#include "hydra/mesh.h"
#include "hydra/geometry.inl"
#include "scene/mesh.h"

#include <pxr/base/gf/vec2f.h>
#include <pxr/imaging/hd/extComputationUtils.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

namespace {

template<typename T>
VtValue ComputeTriangulatedUniformPrimvar(VtValue value, const VtIntArray &primitiveParams)
{
  T output;
  output.reserve(primitiveParams.size());
  const T &input = value.Get<T>();

  for (size_t i = 0; i < primitiveParams.size(); ++i) {
    const int faceIndex = HdMeshUtil::DecodeFaceIndexFromCoarseFaceParam(primitiveParams[i]);

    output.push_back(input[faceIndex]);
  }

  return VtValue(output);
}

VtValue ComputeTriangulatedUniformPrimvar(VtValue value,
                                          const HdType valueType,
                                          const VtIntArray &primitiveParams)
{
  switch (valueType) {
    case HdTypeFloat:
      return ComputeTriangulatedUniformPrimvar<VtFloatArray>(value, primitiveParams);
    case HdTypeFloatVec2:
      return ComputeTriangulatedUniformPrimvar<VtVec2fArray>(value, primitiveParams);
    case HdTypeFloatVec3:
      return ComputeTriangulatedUniformPrimvar<VtVec3fArray>(value, primitiveParams);
    case HdTypeFloatVec4:
      return ComputeTriangulatedUniformPrimvar<VtVec4fArray>(value, primitiveParams);
    default:
      TF_RUNTIME_ERROR("Unsupported attribute type %d", static_cast<int>(valueType));
      return VtValue();
  }
}

VtValue ComputeTriangulatedFaceVaryingPrimvar(VtValue value,
                                              const HdType valueType,
                                              HdMeshUtil &meshUtil)
{
  if (meshUtil.ComputeTriangulatedFaceVaryingPrimvar(
          HdGetValueData(value), value.GetArraySize(), valueType, &value))
  {
    return value;
  }

  return VtValue();
}

}  // namespace

Transform convert_transform(const GfMatrix4d &matrix)
{
  return make_transform(matrix[0][0],
                        matrix[1][0],
                        matrix[2][0],
                        matrix[3][0],
                        matrix[0][1],
                        matrix[1][1],
                        matrix[2][1],
                        matrix[3][1],
                        matrix[0][2],
                        matrix[1][2],
                        matrix[2][2],
                        matrix[3][2]);
}

HdCyclesMesh::HdCyclesMesh(const SdfPath &rprimId
#if PXR_VERSION < 2102
                           ,
                           const SdfPath &instancerId
#endif
                           )
    : HdCyclesGeometry(rprimId
#if PXR_VERSION < 2102
                       ,
                       instancerId
#endif
                       ),
      _util(&_topology, rprimId)
{
}

HdCyclesMesh::~HdCyclesMesh() {}

HdDirtyBits HdCyclesMesh::GetInitialDirtyBitsMask() const
{
  HdDirtyBits bits = HdCyclesGeometry::GetInitialDirtyBitsMask();
  bits |= HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyNormals |
          HdChangeTracker::DirtyPrimvar | HdChangeTracker::DirtyTopology |
          HdChangeTracker::DirtyDisplayStyle | HdChangeTracker::DirtySubdivTags;
  return bits;
}

HdDirtyBits HdCyclesMesh::_PropagateDirtyBits(HdDirtyBits bits) const
{
  if (bits & (HdChangeTracker::DirtyMaterialId)) {
    // Update used shaders from geometry subsets if any exist in the topology
    bits |= HdChangeTracker::DirtyTopology;
  }

  if (bits & (HdChangeTracker::DirtyTopology | HdChangeTracker::DirtyDisplayStyle |
              HdChangeTracker::DirtySubdivTags))
  {
    // Do full topology update when display style or subdivision changes
    bits |= HdChangeTracker::DirtyTopology | HdChangeTracker::DirtyDisplayStyle |
            HdChangeTracker::DirtySubdivTags;
  }

  if (bits & (HdChangeTracker::DirtyTopology)) {
    // Changing topology clears the geometry, so need to populate everything again
    bits |= HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyNormals |
            HdChangeTracker::DirtyPrimvar;
  }

  return bits;
}

void HdCyclesMesh::Populate(HdSceneDelegate *sceneDelegate, HdDirtyBits dirtyBits, bool &rebuild)
{
  if (HdChangeTracker::IsTopologyDirty(dirtyBits, GetId())) {
    PopulateTopology(sceneDelegate);
  }

  if (dirtyBits & HdChangeTracker::DirtyPoints) {
    PopulatePoints(sceneDelegate);
  }

  // Must happen after topology update, so that normals attribute size can be calculated
  if (dirtyBits & HdChangeTracker::DirtyNormals) {
    PopulateNormals(sceneDelegate);
  }

  // Must happen after topology update, so that appropriate attribute set can be selected
  if (dirtyBits & HdChangeTracker::DirtyPrimvar) {
    PopulatePrimvars(sceneDelegate);
  }

  rebuild = (_geom->triangles_is_modified()) || (_geom->subd_start_corner_is_modified()) ||
            (_geom->subd_num_corners_is_modified()) || (_geom->subd_shader_is_modified()) ||
            (_geom->subd_smooth_is_modified()) || (_geom->subd_ptex_offset_is_modified()) ||
            (_geom->subd_face_corners_is_modified());
}

void HdCyclesMesh::PopulatePoints(HdSceneDelegate *sceneDelegate)
{
  VtValue value;

  for (const HdExtComputationPrimvarDescriptor &desc :
       sceneDelegate->GetExtComputationPrimvarDescriptors(GetId(), HdInterpolationVertex))
  {
    if (desc.name == HdTokens->points) {
      auto valueStore = HdExtComputationUtils::GetComputedPrimvarValues({desc}, sceneDelegate);
      const auto valueStoreIt = valueStore.find(desc.name);
      if (valueStoreIt != valueStore.end()) {
        value = std::move(valueStoreIt->second);
      }
      break;
    }
  }

  if (value.IsEmpty()) {
    value = GetPoints(sceneDelegate);
  }

  if (!value.IsHolding<VtVec3fArray>()) {
    TF_WARN("Invalid points data for %s", GetId().GetText());
    return;
  }

  const auto &points = value.UncheckedGet<VtVec3fArray>();

  TF_VERIFY(points.size() >= static_cast<size_t>(_topology.GetNumPoints()));

  array<float3> pointsDataCycles;
  pointsDataCycles.reserve(points.size());
  for (const GfVec3f &point : points) {
    pointsDataCycles.push_back_reserved(make_float3(point[0], point[1], point[2]));
  }

  _geom->set_verts(pointsDataCycles);
}

void HdCyclesMesh::PopulateNormals(HdSceneDelegate *sceneDelegate)
{
  _geom->attributes.remove(ATTR_STD_FACE_NORMAL);
  _geom->attributes.remove(ATTR_STD_VERTEX_NORMAL);

  // Authored normals should only exist on triangle meshes
  if (_geom->get_subdivision_type() != Mesh::SUBDIVISION_NONE) {
    return;
  }

  VtValue value;
  HdInterpolation interpolation = HdInterpolationCount;

  for (int i = 0; i < HdInterpolationCount && interpolation == HdInterpolationCount; ++i) {
    for (const HdExtComputationPrimvarDescriptor &desc :
         sceneDelegate->GetExtComputationPrimvarDescriptors(GetId(),
                                                            static_cast<HdInterpolation>(i)))
    {
      if (desc.name == HdTokens->normals) {
        auto valueStore = HdExtComputationUtils::GetComputedPrimvarValues({desc}, sceneDelegate);
        const auto valueStoreIt = valueStore.find(desc.name);
        if (valueStoreIt != valueStore.end()) {
          value = std::move(valueStoreIt->second);
          interpolation = static_cast<HdInterpolation>(i);
        }
        break;
      }
    }
  }

  if (value.IsEmpty()) {
    interpolation = GetPrimvarInterpolation(sceneDelegate, HdTokens->normals);
    if (interpolation == HdInterpolationCount) {
      return;  // Ignore missing normals
    }

    value = GetNormals(sceneDelegate);
  }

  if (!value.IsHolding<VtVec3fArray>()) {
    TF_WARN("Invalid normals data for %s", GetId().GetText());
    return;
  }

  const auto &normals = value.UncheckedGet<VtVec3fArray>();

  if (interpolation == HdInterpolationConstant) {
    TF_VERIFY(normals.size() == 1);

    const GfVec3f constantNormal = normals[0];

    float3 *const N = _geom->attributes.add(ATTR_STD_VERTEX_NORMAL)->data_float3();
    for (size_t i = 0; i < _geom->get_verts().size(); ++i) {
      N[i] = make_float3(constantNormal[0], constantNormal[1], constantNormal[2]);
    }
  }
  else if (interpolation == HdInterpolationUniform) {
    TF_VERIFY(normals.size() == static_cast<size_t>(_topology.GetNumFaces()));

    float3 *const N = _geom->attributes.add(ATTR_STD_FACE_NORMAL)->data_float3();
    for (size_t i = 0; i < _geom->num_triangles(); ++i) {
      const int faceIndex = HdMeshUtil::DecodeFaceIndexFromCoarseFaceParam(_primitiveParams[i]);

      N[i] = make_float3(normals[faceIndex][0], normals[faceIndex][1], normals[faceIndex][2]);
    }
  }
  else if (interpolation == HdInterpolationVertex || interpolation == HdInterpolationVarying) {
    TF_VERIFY(normals.size() == static_cast<size_t>(_topology.GetNumPoints()) &&
              static_cast<size_t>(_topology.GetNumPoints()) == _geom->get_verts().size());

    float3 *const N = _geom->attributes.add(ATTR_STD_VERTEX_NORMAL)->data_float3();
    for (size_t i = 0; i < _geom->get_verts().size(); ++i) {
      N[i] = make_float3(normals[i][0], normals[i][1], normals[i][2]);
    }
  }
  else if (interpolation == HdInterpolationFaceVarying) {
    TF_VERIFY(normals.size() == static_cast<size_t>(_topology.GetNumFaceVaryings()));

    if (!_util.ComputeTriangulatedFaceVaryingPrimvar(
            normals.data(), normals.size(), HdTypeFloatVec3, &value))
    {
      return;
    }

    const auto &normalsTriangulated = value.UncheckedGet<VtVec3fArray>();

    // Cycles has no standard attribute for face-varying normals, so this is a lossy transformation
    float3 *const N = _geom->attributes.add(ATTR_STD_FACE_NORMAL)->data_float3();
    for (size_t i = 0; i < _geom->num_triangles(); ++i) {
      GfVec3f averageNormal = normalsTriangulated[i * 3] + normalsTriangulated[i * 3 + 1] +
                              normalsTriangulated[i * 3 + 2];
      GfNormalize(&averageNormal);

      N[i] = make_float3(averageNormal[0], averageNormal[1], averageNormal[2]);
    }
  }
}

void HdCyclesMesh::PopulatePrimvars(HdSceneDelegate *sceneDelegate)
{
  Scene *const scene = (Scene *)_geom->get_owner();

  const bool subdivision = _geom->get_subdivision_type() != Mesh::SUBDIVISION_NONE;
  AttributeSet &attributes = subdivision ? _geom->subd_attributes : _geom->attributes;

  const std::pair<HdInterpolation, AttributeElement> interpolations[] = {
      std::make_pair(HdInterpolationFaceVarying, ATTR_ELEMENT_CORNER),
      std::make_pair(HdInterpolationUniform, ATTR_ELEMENT_FACE),
      std::make_pair(HdInterpolationVertex, ATTR_ELEMENT_VERTEX),
      std::make_pair(HdInterpolationVarying, ATTR_ELEMENT_VERTEX),
      std::make_pair(HdInterpolationConstant, ATTR_ELEMENT_OBJECT),
  };

  for (const auto &interpolation : interpolations) {
    for (const HdPrimvarDescriptor &desc :
         GetPrimvarDescriptors(sceneDelegate, interpolation.first)) {
      // Skip special primvars that are handled separately
      if (desc.name == HdTokens->points || desc.name == HdTokens->normals) {
        continue;
      }

      VtValue value = GetPrimvar(sceneDelegate, desc.name);
      if (value.IsEmpty()) {
        continue;
      }

      const ustring name(desc.name.GetString());

      AttributeStandard std = ATTR_STD_NONE;
      if (desc.role == HdPrimvarRoleTokens->textureCoordinate) {
        std = ATTR_STD_UV;
      }
      else if (interpolation.first == HdInterpolationVertex) {
        if (desc.name == HdTokens->displayColor || desc.role == HdPrimvarRoleTokens->color) {
          std = ATTR_STD_VERTEX_COLOR;
        }
        else if (desc.name == HdTokens->normals) {
          std = ATTR_STD_VERTEX_NORMAL;
        }
      }
      else if (desc.name == HdTokens->displayColor &&
               interpolation.first == HdInterpolationConstant) {
        if (value.IsHolding<VtVec3fArray>() && value.GetArraySize() == 1) {
          const GfVec3f color = value.UncheckedGet<VtVec3fArray>()[0];
          _instances[0]->set_color(make_float3(color[0], color[1], color[2]));
        }
      }

      // Skip attributes that are not needed
      if ((std != ATTR_STD_NONE && _geom->need_attribute(scene, std)) ||
          _geom->need_attribute(scene, name))
      {
        const HdType valueType = HdGetValueTupleType(value).type;

        if (!subdivision) {
          // Adjust attributes for polygons that were triangulated
          if (interpolation.first == HdInterpolationUniform) {
            value = ComputeTriangulatedUniformPrimvar(value, valueType, _primitiveParams);
            if (value.IsEmpty()) {
              continue;
            }
          }
          else if (interpolation.first == HdInterpolationFaceVarying) {
            value = ComputeTriangulatedFaceVaryingPrimvar(value, valueType, _util);
            if (value.IsEmpty()) {
              continue;
            }
          }
        }

        ApplyPrimvars(attributes, name, value, interpolation.second, std);
      }
    }
  }
}

void HdCyclesMesh::PopulateTopology(HdSceneDelegate *sceneDelegate)
{
  // Clear geometry before populating it again with updated topology
  _geom->clear(true);

  const HdDisplayStyle displayStyle = GetDisplayStyle(sceneDelegate);
  _topology = HdMeshTopology(GetMeshTopology(sceneDelegate), displayStyle.refineLevel);

  const TfToken subdivScheme = _topology.GetScheme();
  if (subdivScheme == PxOsdOpenSubdivTokens->bilinear && _topology.GetRefineLevel() > 0) {
    _geom->set_subdivision_type(Mesh::SUBDIVISION_LINEAR);
  }
  else if (subdivScheme == PxOsdOpenSubdivTokens->catmullClark && _topology.GetRefineLevel() > 0) {
    _geom->set_subdivision_type(Mesh::SUBDIVISION_CATMULL_CLARK);
  }
  else {
    _geom->set_subdivision_type(Mesh::SUBDIVISION_NONE);
  }

  const bool smooth = !displayStyle.flatShadingEnabled;
  const bool subdivision = _geom->get_subdivision_type() != Mesh::SUBDIVISION_NONE;

  // Initialize lookup table from polygon face to material shader index
  VtIntArray faceShaders(_topology.GetNumFaces(), 0);

  HdGeomSubsets const &geomSubsets = _topology.GetGeomSubsets();
  if (!geomSubsets.empty()) {
    array<Node *> usedShaders = std::move(_geom->get_used_shaders());
    // Remove any previous materials except for the material assigned to the prim
    usedShaders.resize(1);

    std::unordered_map<SdfPath, int, SdfPath::Hash> materials;

    for (const HdGeomSubset &geomSubset : geomSubsets) {
      TF_VERIFY(geomSubset.type == HdGeomSubset::TypeFaceSet);

      int shader = 0;
      const auto it = materials.find(geomSubset.materialId);
      if (it != materials.end()) {
        shader = it->second;
      }
      else {
        const auto material = static_cast<const HdCyclesMaterial *>(
            sceneDelegate->GetRenderIndex().GetSprim(HdPrimTypeTokens->material,
                                                     geomSubset.materialId));

        if (material && material->GetCyclesShader()) {
          shader = static_cast<int>(usedShaders.size());
          usedShaders.push_back_slow(material->GetCyclesShader());

          materials.emplace(geomSubset.materialId, shader);
        }
      }

      for (int face : geomSubset.indices) {
        faceShaders[face] = shader;
      }
    }

    _geom->set_used_shaders(usedShaders);
  }

  const VtIntArray vertIndx = _topology.GetFaceVertexIndices();
  const VtIntArray vertCounts = _topology.GetFaceVertexCounts();

  if (!subdivision) {
    VtVec3iArray triangles;
    _util.ComputeTriangleIndices(&triangles, &_primitiveParams);

    _geom->reserve_mesh(_topology.GetNumPoints(), triangles.size());

    for (size_t i = 0; i < _primitiveParams.size(); ++i) {
      const int faceIndex = HdMeshUtil::DecodeFaceIndexFromCoarseFaceParam(_primitiveParams[i]);

      const GfVec3i triangle = triangles[i];
      _geom->add_triangle(triangle[0], triangle[1], triangle[2], faceShaders[faceIndex], smooth);
    }
  }
  else {
    PxOsdSubdivTags subdivTags = GetSubdivTags(sceneDelegate);
    _topology.SetSubdivTags(subdivTags);

    size_t numNgons = 0;
    size_t numCorners = 0;
    for (int vertCount : vertCounts) {
      numNgons += (vertCount == 4) ? 0 : 1;
      numCorners += vertCount;
    }

    _geom->reserve_subd_faces(_topology.GetNumFaces(), numNgons, numCorners);

    // TODO: Handle hole indices
    size_t faceIndex = 0;
    size_t indexOffset = 0;
    for (int vertCount : vertCounts) {
      _geom->add_subd_face(&vertIndx[indexOffset], vertCount, faceShaders[faceIndex], smooth);

      faceIndex++;
      indexOffset += vertCount;
    }

    const VtIntArray creaseLengths = subdivTags.GetCreaseLengths();
    if (!creaseLengths.empty()) {
      size_t numCreases = 0;
      for (int creaseLength : creaseLengths) {
        numCreases += creaseLength - 1;
      }

      _geom->reserve_subd_creases(numCreases);

      const VtIntArray creaseIndices = subdivTags.GetCreaseIndices();
      const VtFloatArray creaseWeights = subdivTags.GetCreaseWeights();

      indexOffset = 0;
      size_t creaseLengthOffset = 0;
      size_t createWeightOffset = 0;
      for (int creaseLength : creaseLengths) {
        for (int j = 0; j < creaseLength - 1; ++j, ++createWeightOffset) {
          const int v0 = creaseIndices[indexOffset + j];
          const int v1 = creaseIndices[indexOffset + j + 1];

          float weight = creaseWeights.size() == creaseLengths.size() ?
                             creaseWeights[creaseLengthOffset] :
                             creaseWeights[createWeightOffset];

          _geom->add_edge_crease(v0, v1, weight);
        }

        indexOffset += creaseLength;
        creaseLengthOffset++;
      }

      const VtIntArray cornerIndices = subdivTags.GetCornerIndices();
      const VtFloatArray cornerWeights = subdivTags.GetCornerWeights();

      for (size_t i = 0; i < cornerIndices.size(); ++i) {
        _geom->add_vertex_crease(cornerIndices[i], cornerWeights[i]);
      }
    }

    _geom->set_subd_dicing_rate(1.0f);
    _geom->set_subd_max_level(_topology.GetRefineLevel());
    _geom->set_subd_objecttoworld(_instances[0]->get_tfm());
  }
}

void HdCyclesMesh::Finalize(PXR_NS::HdRenderParam *renderParam)
{
  _topology = HdMeshTopology();
  _primitiveParams.clear();

  HdCyclesGeometry<PXR_NS::HdMesh, Mesh>::Finalize(renderParam);
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
