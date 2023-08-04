/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/imaging/hd/tokens.h>

#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"

#include "hydra_scene_delegate.h"
#include "mesh.h"

PXR_NAMESPACE_OPEN_SCOPE
TF_DEFINE_PRIVATE_TOKENS(tokens_, (st));
PXR_NAMESPACE_CLOSE_SCOPE

namespace blender::io::hydra {

MeshData::MeshData(HydraSceneDelegate *scene_delegate, Object *object, pxr::SdfPath const &prim_id)
    : ObjectData(scene_delegate, object, prim_id)
{
}

void MeshData::init()
{
  ID_LOGN(1, "");

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
  ID_LOGN(1, "");
  update_prims();
}

void MeshData::remove()
{
  ID_LOG(1, "");
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
    ID_LOGN(1, "%d", i);
  }
}

pxr::VtValue MeshData::get_data(pxr::TfToken const & /* key */) const
{
  return pxr::VtValue();
}

pxr::VtValue MeshData::get_data(pxr::SdfPath const &id, pxr::TfToken const &key) const
{
  if (key == pxr::HdTokens->normals) {
    return pxr::VtValue(submesh(id).normals);
  }
  if (key == pxr::tokens_->st) {
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
  for (auto &sm : submeshes_) {
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
          pxr::tokens_->st, interpolation, pxr::HdPrimvarRoleTokens->textureCoordinate);
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
      ID_LOGN(1, "%d", i);
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
  Object *object = (Object *)id;
  for (int i = 0; i < submeshes_.size(); ++i) {
    SubMesh &m = submeshes_[i];
    Material *mat = BKE_object_material_get_eval(object, m.mat_index + 1);
    m.mat_data = get_or_create_material(mat);
  }
}

pxr::SdfPath MeshData::submesh_prim_id(int index) const
{
  char name[16];
  snprintf(name, sizeof(name), "SM_%04d", index);
  return prim_id.AppendElementString(name);
}

const MeshData::SubMesh &MeshData::submesh(pxr::SdfPath const &id) const
{
  int index;
  sscanf(id.GetName().c_str(), "SM_%d", &index);
  return submeshes_[index];
}

void MeshData::write_submeshes(Mesh *mesh)
{
  submeshes_.clear();

  /* Insert base submeshes */
  int mat_count = BKE_object_material_count_eval((Object *)id);
  for (int i = 0; i < std::max(mat_count, 1); ++i) {
    SubMesh sm;
    sm.mat_index = i;
    submeshes_.push_back(sm);
  }

  /* Fill submeshes data */
  const int *material_indices = BKE_mesh_material_indices(mesh);

  blender::Span<int> looptri_faces = mesh->looptri_faces();
  blender::Span<int> corner_verts = mesh->corner_verts();
  blender::Span<MLoopTri> looptris = mesh->looptris();

  BKE_mesh_calc_normals_split(mesh);
  const float(*lnors)[3] = (float(*)[3])CustomData_get_layer(&mesh->loop_data, CD_NORMAL);
  const float(*luvs)[2] = (float(*)[2])CustomData_get_layer(&mesh->loop_data, CD_PROP_FLOAT2);

  for (size_t i = 0; i < looptris.size(); ++i) {
    int mat_ind = material_indices ? material_indices[looptri_faces[i]] : 0;
    const MLoopTri &lt = looptris[i];
    SubMesh &sm = submeshes_[mat_ind];

    sm.face_vertex_counts.push_back(3);
    sm.face_vertex_indices.push_back(corner_verts[lt.tri[0]]);
    sm.face_vertex_indices.push_back(corner_verts[lt.tri[1]]);
    sm.face_vertex_indices.push_back(corner_verts[lt.tri[2]]);

    if (lnors) {
      sm.normals.push_back(pxr::GfVec3f(lnors[lt.tri[0]]));
      sm.normals.push_back(pxr::GfVec3f(lnors[lt.tri[1]]));
      sm.normals.push_back(pxr::GfVec3f(lnors[lt.tri[2]]));
    }

    if (luvs) {
      sm.uvs.push_back(pxr::GfVec2f(luvs[lt.tri[0]]));
      sm.uvs.push_back(pxr::GfVec2f(luvs[lt.tri[1]]));
      sm.uvs.push_back(pxr::GfVec2f(luvs[lt.tri[2]]));
    }
  }

  /* Remove submeshes without faces */
  for (auto it = submeshes_.begin(); it != submeshes_.end();) {
    if (it->face_vertex_counts.empty()) {
      it = submeshes_.erase(it);
    }
    else {
      ++it;
    }
  }

  if (submeshes_.empty()) {
    return;
  }

  /* vertices */
  blender::Span<blender::float3> verts = mesh->vert_positions();
  pxr::VtVec3fArray vertices(mesh->totvert);
  int i = 0;
  for (blender::float3 v : verts) {
    vertices[i++] = pxr::GfVec3f(v.x, v.y, v.z);
  }

  if (submeshes_.size() == 1) {
    submeshes_[0].vertices = std::move(vertices);
  }
  else {
    /* Optimizing submeshes: getting only used vertices, rearranged indices */
    for (SubMesh &sm : submeshes_) {
      Vector<int> index_map(vertices.size(), 0);
      for (int &face_vertex_index : sm.face_vertex_indices) {
        const int v = face_vertex_index;
        if (index_map[v] == 0) {
          sm.vertices.push_back(vertices[v]);
          index_map[v] = sm.vertices.size();
        }
        face_vertex_index = index_map[v] - 1;
      }
    }
  }
}

void MeshData::update_prims()
{
  auto &render_index = scene_delegate_->GetRenderIndex();
  int i;
  for (i = 0; i < submeshes_.size(); ++i) {
    pxr::SdfPath p = submesh_prim_id(i);
    if (i < submeshes_count_) {
      render_index.GetChangeTracker().MarkRprimDirty(p, pxr::HdChangeTracker::AllDirty);
      ID_LOGN(1, "Update %d", i);
    }
    else {
      render_index.InsertRprim(pxr::HdPrimTypeTokens->mesh, scene_delegate_, p);
      ID_LOGN(1, "Insert %d", i);
    }
  }
  for (; i < submeshes_count_; ++i) {
    render_index.RemoveRprim(submesh_prim_id(i));
    ID_LOG(1, "Remove %d", i);
  }
  submeshes_count_ = submeshes_.size();
}

}  // namespace blender::io::hydra
