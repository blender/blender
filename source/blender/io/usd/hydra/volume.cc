/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <pxr/imaging/hd/bprim.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/volumeFieldSchema.h>
#include <pxr/usd/usdHydra/tokens.h>
#include <pxr/usd/usdVol/tokens.h>
#include <pxr/usdImaging/usdVolImaging/tokens.h>

#include "BKE_material.h"
#include "BKE_volume.h"
#include "BLI_index_range.hh"
#include "DNA_volume_types.h"

#include "hydra_scene_delegate.h"
#include "volume.h"

namespace blender::io::hydra {

VolumeData::VolumeData(HydraSceneDelegate *scene_delegate,
                       const Object *object,
                       pxr::SdfPath const &prim_id)
    : ObjectData(scene_delegate, object, prim_id)
{
}

void VolumeData::init()
{
  field_descriptors_.clear();

  Volume *volume = (Volume *)((const Object *)this->id)->data;
  if (!BKE_volume_load(volume, scene_delegate_->bmain)) {
    return;
  }
  filepath_ = BKE_volume_grids_frame_filepath(volume);
  ID_LOGN(1, "%s", filepath_.c_str());

  if (volume->runtime.grids) {
    const int num_grids = BKE_volume_num_grids(volume);
    if (num_grids) {
      for (const int i : IndexRange(num_grids)) {
        const VolumeGrid *grid = BKE_volume_grid_get_for_read(volume, i);
        const std::string grid_name = BKE_volume_grid_name(grid);

        field_descriptors_.emplace_back(pxr::TfToken(grid_name),
                                        pxr::UsdVolImagingTokens->openvdbAsset,
                                        prim_id.AppendElementString("VF_" + grid_name));
      }
    }
  }
  write_transform();
  write_materials();

  BKE_volume_unload(volume);
}

void VolumeData::insert()
{
  scene_delegate_->GetRenderIndex().InsertRprim(
      pxr::HdPrimTypeTokens->volume, scene_delegate_, prim_id);

  ID_LOGN(1, "");

  for (auto &desc : field_descriptors_) {
    scene_delegate_->GetRenderIndex().InsertBprim(
        desc.fieldPrimType, scene_delegate_, desc.fieldId);
    ID_LOGN(2, "Volume field %s", desc.fieldId.GetText());
  }
}

void VolumeData::remove()
{
  for (auto &desc : field_descriptors_) {
    ID_LOG(2, "%s", desc.fieldId.GetText());
    scene_delegate_->GetRenderIndex().RemoveBprim(desc.fieldPrimType, desc.fieldId);
  }
  ID_LOG(1, "");
  scene_delegate_->GetRenderIndex().RemoveRprim(prim_id);
}

void VolumeData::update()
{
  const Object *object = (const Object *)id;
  pxr::HdDirtyBits bits = pxr::HdChangeTracker::Clean;
  if ((id->recalc & ID_RECALC_GEOMETRY) || (((ID *)object->data)->recalc & ID_RECALC_GEOMETRY)) {
    init();
    bits = pxr::HdChangeTracker::AllDirty;
  }
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

  scene_delegate_->GetRenderIndex().GetChangeTracker().MarkRprimDirty(prim_id, bits);
  ID_LOGN(1, "");
}

pxr::VtValue VolumeData::get_data(pxr::TfToken const &key) const
{
  if (key == pxr::HdVolumeFieldSchemaTokens->filePath) {
    return pxr::VtValue(pxr::SdfAssetPath(filepath_, filepath_));
  }
  if (key == pxr::HdVolumeFieldSchemaTokens->fieldIndex) {
    return pxr::VtValue(0);
  }
  if (key == pxr::UsdHydraTokens->textureMemory) {
    return pxr::VtValue(0.0f);
  }
  return pxr::VtValue();
}

pxr::VtValue VolumeData::get_data(pxr::SdfPath const &id, pxr::TfToken const &key) const
{
  if (key == pxr::HdVolumeFieldSchemaTokens->fieldName) {
    std::string name = id.GetName();
    return pxr::VtValue(pxr::TfToken(name.substr(name.find("VF_") + 3)));
  }

  return get_data(key);
}

pxr::SdfPath VolumeData::material_id() const
{
  if (!mat_data_) {
    return pxr::SdfPath();
  }
  return mat_data_->prim_id;
}

void VolumeData::available_materials(Set<pxr::SdfPath> &paths) const
{
  if (mat_data_ && !mat_data_->prim_id.IsEmpty()) {
    paths.add(mat_data_->prim_id);
  }
}

pxr::HdVolumeFieldDescriptorVector VolumeData::field_descriptors() const
{
  return field_descriptors_;
}

void VolumeData::write_materials()
{
  const Object *object = (Object *)id;
  const Material *mat = nullptr;
  /* TODO: Using only first material. Add support for multi-material. */
  if (BKE_object_material_count_eval(object) > 0) {
    mat = BKE_object_material_get_eval(const_cast<Object *>(object), 0);
  }
  mat_data_ = get_or_create_material(mat);
}

}  // namespace blender::io::hydra
