/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DEG_depsgraph_query.h"

#include "curves.h"
#include "hydra_scene_delegate.h"
#include "light.h"
#include "mesh.h"
#include "object.h"
#include "volume.h"

namespace blender::io::hydra {

ObjectData::ObjectData(HydraSceneDelegate *scene_delegate,
                       const Object *object,
                       pxr::SdfPath const &prim_id)
    : IdData(scene_delegate, &object->id, prim_id), transform(pxr::GfMatrix4d(1.0))
{
}

std::unique_ptr<ObjectData> ObjectData::create(HydraSceneDelegate *scene_delegate,
                                               const Object *object,
                                               pxr::SdfPath const &prim_id)
{
  std::unique_ptr<ObjectData> obj_data;
  switch (object->type) {
    case OB_MESH:
    case OB_SURF:
    case OB_FONT:
    case OB_CURVES_LEGACY:
    case OB_MBALL:
      if (VolumeModifierData::is_volume_modifier(object)) {
        obj_data = std::make_unique<VolumeModifierData>(scene_delegate, object, prim_id);
        break;
      }
      obj_data = std::make_unique<MeshData>(scene_delegate, object, prim_id);
      break;
    case OB_CURVES:
      obj_data = std::make_unique<CurvesData>(scene_delegate, object, prim_id);
      break;
    case OB_LAMP:
      obj_data = std::make_unique<LightData>(scene_delegate, object, prim_id);
      break;
    case OB_VOLUME:
      obj_data = std::make_unique<VolumeData>(scene_delegate, object, prim_id);
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  obj_data->init();
  return obj_data;
}

bool ObjectData::is_supported(const Object *object)
{
  switch (object->type) {
    case OB_MESH:
    case OB_SURF:
    case OB_FONT:
    case OB_CURVES:
    case OB_CURVES_LEGACY:
    case OB_MBALL:
    case OB_LAMP:
    case OB_VOLUME:
      return true;

    default:
      break;
  }
  return false;
}

bool ObjectData::is_mesh(const Object *object)
{
  switch (object->type) {
    case OB_MESH:
    case OB_SURF:
    case OB_FONT:
    case OB_CURVES_LEGACY:
    case OB_MBALL:
      if (VolumeModifierData::is_volume_modifier(object)) {
        return false;
      }
      return true;
    default:
      break;
  }
  return false;
}

bool ObjectData::is_visible(HydraSceneDelegate *scene_delegate, const Object *object, int mode)
{
  eEvaluationMode deg_mode = DEG_get_mode(scene_delegate->depsgraph);
  bool ret = BKE_object_visibility(object, deg_mode) & mode;
  if (deg_mode == DAG_EVAL_VIEWPORT) {
    ret &= BKE_object_is_visible_in_viewport(scene_delegate->view3d, object);
  }
  /* Note: visibility for final render we are taking from depsgraph */
  return ret;
}

pxr::VtValue ObjectData::get_data(pxr::SdfPath const & /* id */, pxr::TfToken const &key) const
{
  return get_data(key);
}

pxr::SdfPath ObjectData::material_id() const
{
  return pxr::SdfPath();
}

pxr::SdfPath ObjectData::material_id(pxr::SdfPath const & /* id */) const
{
  return material_id();
}

void ObjectData::available_materials(Set<pxr::SdfPath> & /* paths */) const {}

void ObjectData::write_transform()
{
  transform = gf_matrix_from_transform(((const Object *)id)->object_to_world);
}

void ObjectData::write_materials() {}

MaterialData *ObjectData::get_or_create_material(const Material *mat)
{
  if (!mat) {
    return nullptr;
  }

  pxr::SdfPath p_id = scene_delegate_->material_prim_id(mat);
  MaterialData *mat_data = scene_delegate_->material_data(p_id);
  if (!mat_data) {
    scene_delegate_->materials_.add_new(
        p_id, std::make_unique<MaterialData>(scene_delegate_, mat, p_id));
    mat_data = scene_delegate_->material_data(p_id);
    mat_data->init();
    mat_data->insert();
  }
  return mat_data;
}

pxr::GfMatrix4d gf_matrix_from_transform(const float m[4][4])
{
  pxr::GfMatrix4d ret;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      ret[i][j] = m[i][j];
    }
  }
  return ret;
}

}  // namespace blender::io::hydra
