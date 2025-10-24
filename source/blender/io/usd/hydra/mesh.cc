/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/imaging/hd/tokens.h>

#include "BLI_array_utils.hh"
#include "BLI_string.h"
#include "BLI_vector_set.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"

#include "hydra_scene_delegate.hh"
#include "mesh.hh"

namespace blender::io::hydra {

namespace usdtokens {
static const pxr::TfToken st("st", pxr::TfToken::Immortal);
}

MeshData::MeshData(HydraSceneDelegate *scene_delegate,
                   const Object *object,
                   pxr::SdfPath const &prim_id)
    : ObjectData(scene_delegate, object, prim_id)
{
}

void MeshData::init()
{
  ID_LOGN("");

  Object *object = (Object *)id;
  Mesh *mesh = BKE_object_to_mesh(nullptr, object, false);
  if (mesh) {
    write_submeshes(mesh);
  }
  BKE_object_to_mesh_clear(object);

  write_transform();
  write_materials();
}

void MeshData::insert()
{
  ID_LOGN("");
  update_prims();
}

void MeshData::remove()
{
  ID_LOG("");
  submeshes_.clear();
  update_prims();
}

void MeshData::update()
{
  Object *object = (Object *)id;
  if ((id->recalc & ID_RECALC_GEOMETRY) || (((ID *)object->data)->recalc & ID_RECALC_GEOMETRY)) {
    init();
    update_prims();
    return;
  }

  pxr::HdDirtyBits bits = pxr::HdChangeTracker::Clean;
  if (id->recalc & ID_RECALC_SHADING) {
    write_materials();
    bits |= pxr::HdChangeTracker::DirtyMaterialId | pxr::HdChangeTracker::DirtyDoubleSided;
  }
  if (id->recalc & ID_RECALC_TRANSFORM) {
    write_transform();
    bits |= pxr::HdChangeTracker::DirtyTransform;
  }

  if (bits == pxr::HdChangeTracker::Clean) {
    return;
  }

  for (int i = 0; i < submeshes_.size(); ++i) {
    scene_delegate_->GetRenderIndex().GetChangeTracker().MarkRprimDirty(submesh_prim_id(i), bits);
    ID_LOGN("%d", i);
  }
}

pxr::VtValue MeshData::get_data(pxr::TfToken const & /*key*/) const
{
  return pxr::VtValue();
}

pxr::VtValue MeshData::get_data(pxr::SdfPath const &id, pxr::TfToken const &key) const
{
  if (key == pxr::HdTokens->normals) {
    return pxr::VtValue(submesh(id).normals);
  }
  if (key == usdtokens::st) {
    return pxr::VtValue(submesh(id).uvs);
  }
  if (key == pxr::HdTokens->points) {
    return pxr::VtValue(submesh(id).vertices);
  }

  return get_data(key);
}

pxr::SdfPath MeshData::material_id(pxr::SdfPath const &id) const
{
  const SubMesh &sm = submesh(id);
  if (!sm.mat_data) {
    return pxr::SdfPath();
  }
  return sm.mat_data->prim_id;
}

void MeshData::available_materials(Set<pxr::SdfPath> &paths) const
{
  for (const auto &sm : submeshes_) {
    if (sm.mat_data && !sm.mat_data->prim_id.IsEmpty()) {
      paths.add(sm.mat_data->prim_id);
    }
  }
}

pxr::HdMeshTopology MeshData::topology(pxr::SdfPath const &id) const
{
  const SubMesh &sm = submesh(id);
  return pxr::HdMeshTopology(pxr::PxOsdOpenSubdivTokens->none,
                             pxr::HdTokens->rightHanded,
                             sm.face_vertex_counts,
                             sm.face_vertex_indices);
}

pxr::HdPrimvarDescriptorVector MeshData::primvar_descriptors(
    pxr::HdInterpolation interpolation) const
{
  pxr::HdPrimvarDescriptorVector primvars;
  if (interpolation == pxr::HdInterpolationVertex) {
    primvars.emplace_back(pxr::HdTokens->points, interpolation, pxr::HdPrimvarRoleTokens->point);
  }
  else if (interpolation == pxr::HdInterpolationFaceVarying) {
    if (!submeshes_[0].normals.empty()) {
      primvars.emplace_back(
          pxr::HdTokens->normals, interpolation, pxr::HdPrimvarRoleTokens->normal);
    }
    if (!submeshes_[0].uvs.empty()) {
      primvars.emplace_back(
          usdtokens::st, interpolation, pxr::HdPrimvarRoleTokens->textureCoordinate);
    }
  }
  return primvars;
}

pxr::HdCullStyle MeshData::cull_style(pxr::SdfPath const &id) const
{
  const SubMesh &sm = submesh(id);
  if (sm.mat_data) {
    return sm.mat_data->cull_style();
  }
  return pxr::HdCullStyle::HdCullStyleNothing;
}

bool MeshData::double_sided(pxr::SdfPath const &id) const
{
  const SubMesh &sm = submesh(id);
  if (sm.mat_data) {
    return sm.mat_data->double_sided;
  }
  return true;
}

void MeshData::update_double_sided(MaterialData *mat_data)
{
  for (int i = 0; i < submeshes_.size(); ++i) {
    if (submeshes_[i].mat_data == mat_data) {
      scene_delegate_->GetRenderIndex().GetChangeTracker().MarkRprimDirty(
          submesh_prim_id(i),
          pxr::HdChangeTracker::DirtyDoubleSided | pxr::HdChangeTracker::DirtyCullStyle);
      ID_LOGN("%d", i);
    }
  }
}

pxr::SdfPathVector MeshData::submesh_paths() const
{
  pxr::SdfPathVector ret;
  for (int i = 0; i < submeshes_.size(); ++i) {
    ret.push_back(submesh_prim_id(i));
  }
  return ret;
}

void MeshData::write_materials()
{
  const Object *object = (const Object *)id;
  for (int i = 0; i < submeshes_.size(); ++i) {
    SubMesh &m = submeshes_[i];
    const Material *mat = BKE_object_material_get_eval(const_cast<Object *>(object),
                                                       m.mat_index + 1);
    m.mat_data = get_or_create_material(mat);
  }
}

pxr::SdfPath MeshData::submesh_prim_id(int index) const
{
  char name[16];
  SNPRINTF(name, "SM_%04d", index);
  return prim_id.AppendElementString(name);
}

const MeshData::SubMesh &MeshData::submesh(pxr::SdfPath const &id) const
{
  int index;
  sscanf(id.GetName().c_str(), "SM_%d", &index);
  return submeshes_[index];
}

/**
 * #VtArray::resize() does value initialization of every new value, which ends up being `memset`
 * for the trivial attribute types we deal with here. This is unnecessary since every item is
 * initialized via copy from a Blender mesh here anyway. This specializes the resize call to skip
 * initialization.
 */
template<typename T> static void resize_uninitialized(pxr::VtArray<T> &array, const int new_size)
{
  static_assert(std::is_trivial_v<T>);
  array.resize(new_size, [](auto /*begin*/, auto /*end*/) {});
}

static std::pair<bke::MeshNormalDomain, Span<float3>> get_mesh_normals(const Mesh &mesh)
{
  switch (mesh.normals_domain()) {
    case bke::MeshNormalDomain::Face:
      return {bke::MeshNormalDomain::Face, mesh.face_normals()};
    case bke::MeshNormalDomain::Point:
      return {bke::MeshNormalDomain::Point, mesh.vert_normals()};
    case bke::MeshNormalDomain::Corner:
      return {bke::MeshNormalDomain::Corner, mesh.corner_normals()};
  }
  BLI_assert_unreachable();
  return {};
}

template<typename T>
void gather_vert_data(const Span<int> verts,
                      const bool copy_all_verts,
                      const Span<T> src_data,
                      MutableSpan<T> dst_data)
{
  if (copy_all_verts) {
    array_utils::copy(src_data, dst_data);
  }
  else {
    array_utils::gather(src_data, verts, dst_data);
  }
}

template<typename T>
void gather_face_data(const Span<int> tri_faces,
                      const IndexMask &triangles,
                      const Span<T> src_data,
                      MutableSpan<T> dst_data)
{
  triangles.foreach_index_optimized<int>(GrainSize(1024), [&](const int src, const int dst) {
    dst_data[dst] = src_data[tri_faces[src]];
  });
}

template<typename T>
void gather_corner_data(const Span<int3> corner_tris,
                        const IndexMask &triangles,
                        const Span<T> src_data,
                        MutableSpan<T> dst_data)
{
  triangles.foreach_index_optimized<int>(GrainSize(1024), [&](const int src, const int dst) {
    const int3 &tri = corner_tris[src];
    dst_data[dst * 3 + 0] = src_data[tri[0]];
    dst_data[dst * 3 + 1] = src_data[tri[1]];
    dst_data[dst * 3 + 2] = src_data[tri[2]];
  });
}

static void copy_submesh(const Mesh &mesh,
                         const Span<float3> vert_positions,
                         const Span<int> corner_verts,
                         const Span<int3> corner_tris,
                         const Span<int> tri_faces,
                         const std::pair<bke::MeshNormalDomain, Span<float3>> normals,
                         const Span<float2> uv_map,
                         const IndexMask &triangles,
                         MeshData::SubMesh &sm)
{
  resize_uninitialized(sm.face_vertex_indices, triangles.size() * 3);

  /* If all triangles are part of this submesh and there are no loose vertices that shouldn't be
   * copied (Hydra will warn about this), vertex index compression can be completely skipped. */
  const bool copy_all_verts = triangles.size() == corner_tris.size() &&
                              mesh.verts_no_face().count == 0;

  int dst_verts_num;
  VectorSet<int> verts;
  if (copy_all_verts) {
    bke::mesh::vert_tris_from_corner_tris(
        corner_verts,
        corner_tris,
        MutableSpan(sm.face_vertex_indices.data(), sm.face_vertex_indices.size()).cast<int3>());
    dst_verts_num = vert_positions.size();
  }
  else {
    /* Compress vertex indices to be contiguous so it's only necessary to copy values
     * for vertices actually used by the subset of triangles. */
    verts.reserve(triangles.size());
    triangles.foreach_index([&](const int src, const int dst) {
      const int3 &tri = corner_tris[src];
      sm.face_vertex_indices[dst * 3 + 0] = verts.index_of_or_add(corner_verts[tri[0]]);
      sm.face_vertex_indices[dst * 3 + 1] = verts.index_of_or_add(corner_verts[tri[1]]);
      sm.face_vertex_indices[dst * 3 + 2] = verts.index_of_or_add(corner_verts[tri[2]]);
    });
    dst_verts_num = verts.size();
  }

  resize_uninitialized(sm.vertices, dst_verts_num);
  gather_vert_data(verts,
                   copy_all_verts,
                   vert_positions,
                   MutableSpan(sm.vertices.data(), sm.vertices.size()).cast<float3>());

  resize_uninitialized(sm.face_vertex_counts, triangles.size());
  std::fill(sm.face_vertex_counts.begin(), sm.face_vertex_counts.end(), 3);

  const Span<float3> src_normals = normals.second;
  resize_uninitialized(sm.normals, triangles.size() * 3);
  MutableSpan dst_normals = MutableSpan(sm.normals.data(), sm.normals.size()).cast<float3>();
  switch (normals.first) {
    case bke::MeshNormalDomain::Face:
      triangles.foreach_index(GrainSize(1024), [&](const int src, const int dst) {
        std::fill_n(&dst_normals[dst * 3], 3, src_normals[tri_faces[src]]);
      });
      break;
    case bke::MeshNormalDomain::Point:
      triangles.foreach_index(GrainSize(1024), [&](const int src, const int dst) {
        const int3 &tri = corner_tris[src];
        dst_normals[dst * 3 + 0] = src_normals[corner_verts[tri[0]]];
        dst_normals[dst * 3 + 1] = src_normals[corner_verts[tri[1]]];
        dst_normals[dst * 3 + 2] = src_normals[corner_verts[tri[2]]];
      });
      break;
    case bke::MeshNormalDomain::Corner:
      gather_corner_data(corner_tris, triangles, src_normals, dst_normals);
      break;
  }

  if (!uv_map.is_empty()) {
    resize_uninitialized(sm.uvs, triangles.size() * 3);
    gather_corner_data(
        corner_tris, triangles, uv_map, MutableSpan(sm.uvs.data(), sm.uvs.size()).cast<float2>());
  }
}

void MeshData::write_submeshes(const Mesh *mesh)
{
  const int mat_count = BKE_object_material_count_eval(reinterpret_cast<const Object *>(id));
  submeshes_.reinitialize(mat_count > 0 ? mat_count : 1);
  for (const int i : submeshes_.index_range()) {
    submeshes_[i].mat_index = i;
  }

  const Span<float3> vert_positions = mesh->vert_positions();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int3> corner_tris = mesh->corner_tris();
  const Span<int> tri_faces = mesh->corner_tri_faces();
  const std::pair<bke::MeshNormalDomain, Span<float3>> normals = get_mesh_normals(*mesh);
  const bke::AttributeAccessor attributes = mesh->attributes();
  const StringRef active_uv = mesh->active_uv_map_name();
  const VArraySpan uv_map = *attributes.lookup<float2>(active_uv, bke::AttrDomain::Corner);
  const VArraySpan material_indices = *attributes.lookup<int>("material_index",
                                                              bke::AttrDomain::Face);

  if (material_indices.is_empty()) {
    copy_submesh(*mesh,
                 vert_positions,
                 corner_verts,
                 corner_tris,
                 tri_faces,
                 normals,
                 uv_map,
                 corner_tris.index_range(),
                 submeshes_.first());
    return;
  }

  IndexMaskMemory memory;
  Array<IndexMask> triangles_by_material(submeshes_.size());
  const int max_index = std::max(mat_count - 1, 0);
  IndexMask::from_groups<int>(
      corner_tris.index_range(),
      memory,
      [&](const int i) { return std::clamp(material_indices[tri_faces[i]], 0, max_index); },
      triangles_by_material);

  threading::parallel_for(submeshes_.index_range(), 1, [&](const IndexRange range) {
    for (const int i : range) {
      copy_submesh(*mesh,
                   vert_positions,
                   corner_verts,
                   corner_tris,
                   tri_faces,
                   normals,
                   uv_map,
                   triangles_by_material[i],
                   submeshes_[i]);
    }
  });

  /* Remove submeshes without faces */
  submeshes_.remove_if([](const SubMesh &submesh) { return submesh.face_vertex_counts.empty(); });
}

void MeshData::update_prims()
{
  auto &render_index = scene_delegate_->GetRenderIndex();
  int i;
  for (i = 0; i < submeshes_.size(); ++i) {
    pxr::SdfPath p = submesh_prim_id(i);
    if (i < submeshes_count_) {
      render_index.GetChangeTracker().MarkRprimDirty(p, pxr::HdChangeTracker::AllDirty);
      ID_LOGN("Update %d", i);
    }
    else {
      render_index.InsertRprim(pxr::HdPrimTypeTokens->mesh, scene_delegate_, p);
      ID_LOGN("Insert %d", i);
    }
  }
  for (; i < submeshes_count_; ++i) {
    render_index.RemoveRprim(submesh_prim_id(i));
    ID_LOG("Remove %d", i);
  }
  submeshes_count_ = submeshes_.size();
}

}  // namespace blender::io::hydra
