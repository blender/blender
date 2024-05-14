/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "instancer.hh"

#include <pxr/base/gf/vec2f.h>
#include <pxr/imaging/hd/light.h>

#include "BKE_particle.h"

#include "BLI_math_matrix.h"
#include "BLI_string.h"

#include "DEG_depsgraph_query.hh"

#include "DNA_particle_types.h"

#include "hydra_scene_delegate.hh"

namespace blender::io::hydra {

InstancerData::InstancerData(HydraSceneDelegate *scene_delegate, pxr::SdfPath const &prim_id)
    : IdData(scene_delegate, nullptr, prim_id)
{
}

void InstancerData::init() {}

void InstancerData::insert() {}

void InstancerData::remove()
{
  CLOG_INFO(LOG_HYDRA_SCENE, 1, "%s", prim_id.GetText());
  for (auto &m_inst : mesh_instances_.values()) {
    m_inst.data->remove();
  }
  if (!mesh_instances_.is_empty()) {
    scene_delegate_->GetRenderIndex().RemoveInstancer(prim_id);
  }
  mesh_instances_.clear();

  for (auto &l_inst : nonmesh_instances_.values()) {
    l_inst.transforms.clear();
    update_nonmesh_instance(l_inst);
  }
  nonmesh_instances_.clear();
}

void InstancerData::update() {}

pxr::VtValue InstancerData::get_data(pxr::TfToken const &key) const
{
  ID_LOG(3, "%s", key.GetText());
  if (key == pxr::HdInstancerTokens->instanceTransforms) {
    return pxr::VtValue(mesh_transforms_);
  }
  return pxr::VtValue();
}

pxr::GfMatrix4d InstancerData::transform(pxr::SdfPath const &id) const
{
  NonmeshInstance *nm_inst = nonmesh_instance(id);
  if (nm_inst) {
    return nm_inst->transforms[nonmesh_prim_id_index(id)];
  }

  /* Mesh instance transform must be identity */
  return pxr::GfMatrix4d(1.0);
}

pxr::HdPrimvarDescriptorVector InstancerData::primvar_descriptors(
    pxr::HdInterpolation interpolation) const
{
  pxr::HdPrimvarDescriptorVector primvars;
  if (interpolation == pxr::HdInterpolationInstance) {
    primvars.emplace_back(
        pxr::HdInstancerTokens->instanceTransforms, interpolation, pxr::HdPrimvarRoleTokens->none);
  }
  return primvars;
}

pxr::VtIntArray InstancerData::indices(pxr::SdfPath const &id) const
{
  return mesh_instance(id)->indices;
}

ObjectData *InstancerData::object_data(pxr::SdfPath const &id) const
{
  MeshInstance *m_inst = mesh_instance(id);
  if (m_inst) {
    return m_inst->data.get();
  }
  NonmeshInstance *nm_inst = nonmesh_instance(id);
  if (nm_inst) {
    return nm_inst->data.get();
  }
  return nullptr;
}

pxr::SdfPathVector InstancerData::prototypes() const
{
  pxr::SdfPathVector paths;
  for (auto &m_inst : mesh_instances_.values()) {
    for (auto &p : m_inst.data->submesh_paths()) {
      paths.push_back(p);
    }
  }
  return paths;
}

void InstancerData::available_materials(Set<pxr::SdfPath> &paths) const
{
  for (auto &m_inst : mesh_instances_.values()) {
    m_inst.data->available_materials(paths);
  }
  for (auto &l_inst : nonmesh_instances_.values()) {
    l_inst.data->available_materials(paths);
  }
}

void InstancerData::update_double_sided(MaterialData *mat_data)
{
  for (auto &m_inst : mesh_instances_.values()) {
    m_inst.data->update_double_sided(mat_data);
  }
}

void InstancerData::pre_update()
{
  mesh_transforms_.clear();
  for (auto &m_inst : mesh_instances_.values()) {
    m_inst.indices.clear();
  }
  for (auto &l_inst : nonmesh_instances_.values()) {
    l_inst.transforms.clear();
  }
}

void InstancerData::update_instance(DupliObject *dupli)
{
  Object *object = dupli->ob;
  pxr::SdfPath p_id = object_prim_id(object);
  if (ObjectData::is_mesh(object)) {
    MeshInstance *m_inst = mesh_instance(p_id);
    if (!m_inst) {
      m_inst = &mesh_instances_.lookup_or_add_default(p_id);
      m_inst->data = std::make_unique<MeshData>(scene_delegate_, object, p_id);
      m_inst->data->init();
      m_inst->data->insert();
    }
    else {
      m_inst->data->update();
    }
    ID_LOG(2, "Mesh %s %d", m_inst->data->id->name, int(mesh_transforms_.size()));
    m_inst->indices.push_back(mesh_transforms_.size());
    mesh_transforms_.push_back(gf_matrix_from_transform(dupli->mat));
  }
  else {
    NonmeshInstance *nm_inst = nonmesh_instance(p_id);
    if (!nm_inst) {
      nm_inst = &nonmesh_instances_.lookup_or_add_default(p_id);
      nm_inst->data = ObjectData::create(scene_delegate_, object, p_id);
    }
    ID_LOG(2, "Nonmesh %s %d", nm_inst->data->id->name, int(nm_inst->transforms.size()));
    nm_inst->transforms.push_back(gf_matrix_from_transform(dupli->mat));
  }

  LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
    if (psys_in_edit_mode(scene_delegate_->depsgraph, psys)) {
      continue;
    }
    if (HairData::is_supported(psys) && HairData::is_visible(scene_delegate_, object, psys)) {
      pxr::SdfPath h_id = hair_prim_id(object, psys);
      NonmeshInstance *nm_inst = nonmesh_instance(h_id);
      if (!nm_inst) {
        nm_inst = &nonmesh_instances_.lookup_or_add_default(h_id);
        nm_inst->data = std::make_unique<HairData>(scene_delegate_, object, h_id, psys);
        nm_inst->data->init();
      }
      ID_LOG(2, "Nonmesh %s %d", nm_inst->data->id->name, int(nm_inst->transforms.size()));
      nm_inst->transforms.push_back(gf_matrix_from_transform(psys->imat) *
                                    gf_matrix_from_transform(dupli->mat));
    }
  }
}

void InstancerData::post_update()
{
  /* Remove mesh instances without indices. */
  mesh_instances_.remove_if([&](auto item) {
    bool res = item.value.indices.empty();
    if (res) {
      item.value.data->remove();
    }
    return res;
  });

  /* Update light instances and remove instances without transforms. */
  for (auto &l_inst : nonmesh_instances_.values()) {
    update_nonmesh_instance(l_inst);
  }
  nonmesh_instances_.remove_if([&](auto item) { return item.value.transforms.empty(); });

  /* Insert/remove/update instancer in RenderIndex. */
  pxr::HdRenderIndex &index = scene_delegate_->GetRenderIndex();
  if (mesh_instances_.is_empty()) {
    /* Important: removing instancer when nonmesh_instances_ are empty too */
    if (index.HasInstancer(prim_id) && nonmesh_instances_.is_empty()) {
      index.RemoveInstancer(prim_id);
      ID_LOG(1, "Remove instancer");
    }
  }
  else {
    if (index.HasInstancer(prim_id)) {
      index.GetChangeTracker().MarkInstancerDirty(prim_id, pxr::HdChangeTracker::AllDirty);
      ID_LOG(1, "Update instancer");
    }
    else {
      index.InsertInstancer(scene_delegate_, prim_id);
      ID_LOG(1, "Insert instancer");
    }
  }
}

pxr::SdfPath InstancerData::object_prim_id(Object *object) const
{
  /* Making id of object in form like <prefix>_<pointer in 16 hex digits format> */
  char name[32];
  SNPRINTF(name, "O_%p", object);
  return prim_id.AppendElementString(name);
}

pxr::SdfPath InstancerData::hair_prim_id(Object *parent_obj, const ParticleSystem *psys) const
{
  /* Making id of object in form like <prefix>_<pointer in 16 hex digits format> */
  char name[128];
  SNPRINTF(name, "%s_PS_%p", object_prim_id(parent_obj).GetName().c_str(), psys);
  return prim_id.AppendElementString(name);
}

pxr::SdfPath InstancerData::nonmesh_prim_id(pxr::SdfPath const &prim_id, int index) const
{
  char name[16];
  SNPRINTF(name, "NM_%08d", index);
  return prim_id.AppendElementString(name);
}

int InstancerData::nonmesh_prim_id_index(pxr::SdfPath const &id) const
{
  int index;
  sscanf(id.GetName().c_str(), "NM_%d", &index);
  return index;
}

void InstancerData::update_nonmesh_instance(NonmeshInstance &nm_inst)
{
  ObjectData *obj_data = nm_inst.data.get();
  pxr::SdfPath prev_id = nm_inst.data->prim_id;
  int i;

  /* Remove old Nonmesh instances */
  while (nm_inst.count > nm_inst.transforms.size()) {
    --nm_inst.count;
    obj_data->prim_id = nonmesh_prim_id(prev_id, nm_inst.count);
    obj_data->remove();
  }

  /* NOTE: Special case: recreate instances when prim_type was changed.
   * Doing only update for other Nonmesh objects. */
  LightData *l_data = dynamic_cast<LightData *>(obj_data);
  if (l_data && l_data->prim_type((Light *)((Object *)l_data->id)->data) != l_data->prim_type_) {
    for (i = 0; i < nm_inst.count; ++i) {
      obj_data->prim_id = nonmesh_prim_id(prev_id, i);
      obj_data->remove();
    }
    l_data->init();
    for (i = 0; i < nm_inst.count; ++i) {
      obj_data->prim_id = nonmesh_prim_id(prev_id, i);
      obj_data->insert();
    }
  }
  else {
    for (i = 0; i < nm_inst.count; ++i) {
      obj_data->prim_id = nonmesh_prim_id(prev_id, i);
      obj_data->update();
    }
  }

  /* Add new Nonmesh instances */
  while (nm_inst.count < nm_inst.transforms.size()) {
    obj_data->prim_id = nonmesh_prim_id(prev_id, nm_inst.count);
    obj_data->insert();
    ++nm_inst.count;
  }

  obj_data->prim_id = prev_id;
}

InstancerData::MeshInstance *InstancerData::mesh_instance(pxr::SdfPath const &id) const
{
  auto m_inst = mesh_instances_.lookup_ptr(id.GetPathElementCount() == 4 ? id.GetParentPath() :
                                                                           id);
  if (!m_inst) {
    return nullptr;
  }
  return const_cast<MeshInstance *>(m_inst);
}

InstancerData::NonmeshInstance *InstancerData::nonmesh_instance(pxr::SdfPath const &id) const
{
  auto nm_inst = nonmesh_instances_.lookup_ptr(id.GetPathElementCount() == 4 ? id.GetParentPath() :
                                                                               id);
  if (!nm_inst) {
    return nullptr;
  }
  return const_cast<NonmeshInstance *>(nm_inst);
}

}  // namespace blender::io::hydra
