/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/hashmap.h>

#include "DNA_object_types.h"

#include "BLI_map.hh"
#include "BLI_set.hh"

#include "BKE_layer.h"
#include "BKE_object.h"

#include "id.h"
#include "material.h"

namespace blender::io::hydra {

class ObjectData : public IdData {
 public:
  pxr::GfMatrix4d transform;
  bool visible = true;

 public:
  ObjectData(HydraSceneDelegate *scene_delegate,
             const Object *object,
             pxr::SdfPath const &prim_id);

  static std::unique_ptr<ObjectData> create(HydraSceneDelegate *scene_delegate,
                                            const Object *object,
                                            pxr::SdfPath const &prim_id);
  static bool is_supported(const Object *object);
  static bool is_mesh(const Object *object);
  static bool is_visible(HydraSceneDelegate *scene_delegate,
                         const Object *object,
                         int mode = OB_VISIBLE_SELF);

  using IdData::get_data;
  virtual pxr::VtValue get_data(pxr::SdfPath const &id, pxr::TfToken const &key) const;
  virtual pxr::SdfPath material_id() const;
  virtual pxr::SdfPath material_id(pxr::SdfPath const &id) const;
  virtual void available_materials(Set<pxr::SdfPath> &paths) const;

 protected:
  virtual void write_transform();
  virtual void write_materials();
  MaterialData *get_or_create_material(const Material *mat);
};

using ObjectDataMap = Map<pxr::SdfPath, std::unique_ptr<ObjectData>>;

pxr::GfMatrix4d gf_matrix_from_transform(const float m[4][4]);

}  // namespace blender::io::hydra
