/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#pragma once

#include "BLI_map.hh"
#include "BLI_set.hh"

#include "mesh.h"

namespace blender::io::hydra {

class InstancerData : public IdData {
  struct MeshInstance {
    std::unique_ptr<MeshData> data;
    pxr::VtIntArray indices;
  };

  struct NonmeshInstance {
    std::unique_ptr<ObjectData> data;
    pxr::VtMatrix4dArray transforms;
    int count = 0;
  };

 private:
  Map<pxr::SdfPath, MeshInstance> mesh_instances_;
  Map<pxr::SdfPath, NonmeshInstance> nonmesh_instances_;
  pxr::VtMatrix4dArray mesh_transforms_;

 public:
  InstancerData(HydraSceneDelegate *scene_delegate, pxr::SdfPath const &prim_id);

  void init() override;
  void insert() override;
  void remove() override;
  void update() override;

  pxr::VtValue get_data(pxr::TfToken const &key) const override;
  pxr::GfMatrix4d transform(pxr::SdfPath const &id) const;
  pxr::HdPrimvarDescriptorVector primvar_descriptors(pxr::HdInterpolation interpolation) const;
  pxr::VtIntArray indices(pxr::SdfPath const &id) const;
  ObjectData *object_data(pxr::SdfPath const &id) const;
  pxr::SdfPathVector prototypes() const;
  void available_materials(Set<pxr::SdfPath> &paths) const;
  void update_double_sided(MaterialData *mat_data);

  /* Following update functions are working together:
   *   pre_update()
   *     update_instance()
   *     update_instance()
   *     ...
   *   post_update() */
  void pre_update();
  void update_instance(DupliObject *dupli);
  void post_update();

 private:
  pxr::SdfPath object_prim_id(Object *object) const;
  pxr::SdfPath nonmesh_prim_id(pxr::SdfPath const &prim_id, int index) const;
  int nonmesh_prim_id_index(pxr::SdfPath const &id) const;
  void update_nonmesh_instance(NonmeshInstance &inst);
  MeshInstance *mesh_instance(pxr::SdfPath const &id) const;
  NonmeshInstance *nonmesh_instance(pxr::SdfPath const &id) const;
};

}  // namespace blender::io::hydra
