/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include "DNA_curves_types.h"

#include "BLI_set.hh"

#include "BKE_duplilist.h"

#include "material.h"
#include "object.h"

namespace blender::io::hydra {

class CurvesData : public ObjectData {
 private:
  pxr::VtIntArray curve_vertex_counts_;
  pxr::VtVec3fArray vertices_;
  pxr::VtVec2fArray uvs_;
  pxr::VtFloatArray widths_;

  MaterialData *mat_data_ = nullptr;

 public:
  CurvesData(HydraSceneDelegate *scene_delegate,
             const Object *object,
             pxr::SdfPath const &prim_id);

  void init() override;
  void insert() override;
  void remove() override;
  void update() override;

  pxr::VtValue get_data(pxr::TfToken const &key) const override;
  pxr::SdfPath material_id() const override;
  void available_materials(Set<pxr::SdfPath> &paths) const override;

  pxr::HdBasisCurvesTopology topology() const;
  pxr::HdPrimvarDescriptorVector primvar_descriptors(pxr::HdInterpolation interpolation) const;

 protected:
  void write_materials() override;

 private:
  void write_curves(const Curves *curves_id);
  void write_uv_maps(const Curves *curves_id);
};

}  // namespace blender::io::hydra
