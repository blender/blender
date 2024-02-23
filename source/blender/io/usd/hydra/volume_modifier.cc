/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "volume_modifier.hh"

#include <pxr/usdImaging/usdVolImaging/tokens.h>

#include "DNA_scene_types.h"
#include "DNA_volume_types.h"

#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_mesh.h"
#include "BKE_modifier.hh"

#include "hydra_scene_delegate.hh"

namespace blender::io::hydra {

VolumeModifierData::VolumeModifierData(HydraSceneDelegate *scene_delegate,
                                       const Object *object,
                                       pxr::SdfPath const &prim_id)
    : VolumeData(scene_delegate, object, prim_id)
{
}

bool VolumeModifierData::is_volume_modifier(const Object *object)
{
  if (object->type != OB_MESH) {
    return false;
  }

  const FluidModifierData *modifier = (const FluidModifierData *)BKE_modifiers_findby_type(
      object, eModifierType_Fluid);
  return modifier && modifier->type & MOD_FLUID_TYPE_DOMAIN &&
         modifier->domain->type == FLUID_DOMAIN_TYPE_GAS;
}

void VolumeModifierData::init()
{
  field_descriptors_.clear();

  const Object *object = (const Object *)this->id;
  const ModifierData *md = BKE_modifiers_findby_type(object, eModifierType_Fluid);
  modifier_ = (const FluidModifierData *)BKE_modifier_get_evaluated(
      scene_delegate_->depsgraph, const_cast<Object *>(object), const_cast<ModifierData *>(md));

  if ((modifier_->domain->cache_data_format & FLUID_DOMAIN_FILE_OPENVDB) == 0) {
    CLOG_WARN(LOG_HYDRA_SCENE,
              "Volume %s is't exported: only OpenVDB file format supported",
              prim_id.GetText());
    return;
  }

  filepath_ = get_cached_file_path(modifier_->domain->cache_directory,
                                   scene_delegate_->scene->r.cfra);
  ID_LOG(1, "%s", filepath_.c_str());

  static const pxr::TfToken grid_tokens[] = {pxr::TfToken("density", pxr::TfToken::Immortal),
                                             pxr::TfToken("flame", pxr::TfToken::Immortal),
                                             pxr::TfToken("shadow", pxr::TfToken::Immortal),
                                             pxr::TfToken("temperature", pxr::TfToken::Immortal),
                                             pxr::TfToken("velocity", pxr::TfToken::Immortal)};

  for (const auto &grid_name : grid_tokens) {
    field_descriptors_.emplace_back(grid_name,
                                    pxr::UsdVolImagingTokens->openvdbAsset,
                                    prim_id.AppendElementString("VF_" + grid_name.GetString()));
  }

  write_transform();
  write_materials();
}

void VolumeModifierData::update()
{
  Object *object = (Object *)id;
  if ((id->recalc & ID_RECALC_GEOMETRY) || (((ID *)object->data)->recalc & ID_RECALC_GEOMETRY)) {
    remove();
    init();
    insert();
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

  scene_delegate_->GetRenderIndex().GetChangeTracker().MarkRprimDirty(prim_id, bits);
  ID_LOG(1, "");
}

void VolumeModifierData::write_transform()
{
  Object *object = (Object *)this->id;

  /* set base scaling */
  transform = pxr::GfMatrix4d().SetScale(
      pxr::GfVec3d(modifier_->domain->scale / modifier_->domain->global_size[0],
                   modifier_->domain->scale / modifier_->domain->global_size[1],
                   modifier_->domain->scale / modifier_->domain->global_size[2]));
  /* positioning to center */
  transform *= pxr::GfMatrix4d().SetTranslate(pxr::GfVec3d(-1, -1, -1));

  /* including texspace transform */
  float texspace_loc[3] = {0.0f, 0.0f, 0.0f}, texspace_scale[3] = {1.0f, 1.0f, 1.0f};
  BKE_mesh_texspace_get((Mesh *)object->data, texspace_loc, texspace_scale);
  transform *= pxr::GfMatrix4d(1.0f).SetScale(pxr::GfVec3d(texspace_scale)) *
               pxr::GfMatrix4d(1.0f).SetTranslate(pxr::GfVec3d(texspace_loc));

  /* applying object transform */
  transform *= gf_matrix_from_transform(object->object_to_world().ptr());
}

std::string VolumeModifierData::get_cached_file_path(std::string directory, int frame)
{
  char file_path[FILE_MAX];
  char file_name[32];
  SNPRINTF(file_name, "%s_####%s", FLUID_NAME_DATA, FLUID_DOMAIN_EXTENSION_OPENVDB);
  BLI_path_frame(file_name, sizeof(file_name), frame, 0);
  BLI_path_join(file_path, sizeof(file_path), directory.c_str(), FLUID_DOMAIN_DIR_DATA, file_name);

  return file_path;
}

}  // namespace blender::io::hydra
