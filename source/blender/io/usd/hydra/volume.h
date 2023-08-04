/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#pragma once

#include <pxr/imaging/hd/sceneDelegate.h>

#include "object.h"

namespace blender::io::hydra {

class VolumeData : public ObjectData {
 protected:
  std::string filepath_;
  pxr::HdVolumeFieldDescriptorVector field_descriptors_;
  MaterialData *mat_data_ = nullptr;

 public:
  VolumeData(HydraSceneDelegate *scene_delegate,
             const Object *object,
             pxr::SdfPath const &prim_id);

  void init() override;
  void insert() override;
  void remove() override;
  void update() override;

  pxr::VtValue get_data(pxr::TfToken const &key) const override;
  pxr::VtValue get_data(pxr::SdfPath const &id, pxr::TfToken const &key) const override;
  pxr::SdfPath material_id() const override;
  void available_materials(Set<pxr::SdfPath> &paths) const override;

  pxr::HdVolumeFieldDescriptorVector field_descriptors() const;

 protected:
  void write_materials() override;
};

}  // namespace blender::io::hydra
