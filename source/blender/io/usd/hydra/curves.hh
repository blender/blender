/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include "BLI_set.hh"

#include "BKE_duplilist.hh"

#include "DNA_curves_types.h"
#include "DNA_particle_types.h"

#include "material.hh"
#include "object.hh"

namespace blender::io::hydra {

class CurvesData : public ObjectData {
 protected:
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
  virtual void write_curves();
};

class HairData : public CurvesData {
 private:
  ParticleSystem *particle_system_;

 public:
  HairData(HydraSceneDelegate *scene_delegate,
           const Object *object,
           pxr::SdfPath const &prim_id,
           ParticleSystem *particle_system);

  static bool is_supported(const ParticleSystem *particle_system);
  static bool is_visible(HydraSceneDelegate *scene_delegate,
                         Object *object,
                         ParticleSystem *particle_system);

  void update() override;

 protected:
  void write_transform() override;
  void write_curves() override;
};

}  // namespace blender::io::hydra
