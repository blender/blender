/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation */

#include "hydra_scene_delegate.h"

#include <bitset>

#include "DNA_scene_types.h"

#include "BLI_set.hh"

#include "DEG_depsgraph_query.h"

namespace blender::io::hydra {

CLG_LOGREF_DECLARE_GLOBAL(LOG_HYDRA_SCENE, "hydra.scene");

bool HydraSceneDelegate::ShadingSettings::operator==(const ShadingSettings &other)
{
  bool ret = use_scene_lights == other.use_scene_lights &&
             use_scene_world == other.use_scene_world;
  if (ret && !use_scene_world) {
    /* compare studiolight settings when studiolight is using */
    ret = studiolight_name == other.studiolight_name &&
          studiolight_rotation == other.studiolight_rotation &&
          studiolight_intensity == other.studiolight_intensity;
  }
  return ret;
}

HydraSceneDelegate::HydraSceneDelegate(pxr::HdRenderIndex *parent_index,
                                       pxr::SdfPath const &delegate_id)
    : HdSceneDelegate(parent_index, delegate_id)
{
  instancer_data_ = std::make_unique<InstancerData>(this, instancer_prim_id());
}

pxr::HdMeshTopology HydraSceneDelegate::GetMeshTopology(pxr::SdfPath const &id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", id.GetText());
  MeshData *m_data = mesh_data(id);
  return m_data->topology(id);
}

pxr::HdBasisCurvesTopology HydraSceneDelegate::GetBasisCurvesTopology(pxr::SdfPath const &id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", id.GetText());
  CurvesData *c_data = curves_data(id);
  return c_data->topology();
};

pxr::GfMatrix4d HydraSceneDelegate::GetTransform(pxr::SdfPath const &id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", id.GetText());
  InstancerData *i_data = instancer_data(id, true);
  if (i_data) {
    return i_data->transform(id);
  }
  ObjectData *obj_data = object_data(id);
  if (obj_data) {
    return obj_data->transform;
  }
  return pxr::GfMatrix4d();
}

pxr::VtValue HydraSceneDelegate::Get(pxr::SdfPath const &id, pxr::TfToken const &key)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s, %s", id.GetText(), key.GetText());
  ObjectData *obj_data = object_data(id);
  if (obj_data) {
    return obj_data->get_data(id, key);
  }
  MaterialData *mat_data = material_data(id);
  if (mat_data) {
    return mat_data->get_data(key);
  }
  InstancerData *i_data = instancer_data(id);
  if (i_data) {
    return i_data->get_data(key);
  }
  return pxr::VtValue();
}

pxr::VtValue HydraSceneDelegate::GetLightParamValue(pxr::SdfPath const &id,
                                                    pxr::TfToken const &key)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s, %s", id.GetText(), key.GetText());
  LightData *l_data = light_data(id);
  if (l_data) {
    return l_data->get_data(key);
  }
  return pxr::VtValue();
}

pxr::HdPrimvarDescriptorVector HydraSceneDelegate::GetPrimvarDescriptors(
    pxr::SdfPath const &id, pxr::HdInterpolation interpolation)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s, %d", id.GetText(), interpolation);
  MeshData *m_data = mesh_data(id);
  if (m_data) {
    return m_data->primvar_descriptors(interpolation);
  }
  CurvesData *c_data = curves_data(id);
  if (c_data) {
    return c_data->primvar_descriptors(interpolation);
  }
  InstancerData *i_data = instancer_data(id);
  if (i_data) {
    return i_data->primvar_descriptors(interpolation);
  }
  return pxr::HdPrimvarDescriptorVector();
}

pxr::SdfPath HydraSceneDelegate::GetMaterialId(pxr::SdfPath const &rprim_id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", rprim_id.GetText());
  ObjectData *obj_data = object_data(rprim_id);
  if (obj_data) {
    return obj_data->material_id(rprim_id);
  }
  return pxr::SdfPath();
}

pxr::VtValue HydraSceneDelegate::GetMaterialResource(pxr::SdfPath const &id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", id.GetText());
  MaterialData *mat_data = material_data(id);
  if (mat_data) {
    return mat_data->get_material_resource();
  }
  return pxr::VtValue();
}

bool HydraSceneDelegate::GetVisible(pxr::SdfPath const &id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", id.GetText());
  if (id == world_prim_id()) {
    return true;
  }
  InstancerData *i_data = instancer_data(id, true);
  if (i_data) {
    return true;
  }
  return object_data(id)->visible;
}

bool HydraSceneDelegate::GetDoubleSided(pxr::SdfPath const &id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", id.GetText());
  return mesh_data(id)->double_sided(id);
}

pxr::HdCullStyle HydraSceneDelegate::GetCullStyle(pxr::SdfPath const &id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", id.GetText());
  return mesh_data(id)->cull_style(id);
}

pxr::SdfPath HydraSceneDelegate::GetInstancerId(pxr::SdfPath const &prim_id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", prim_id.GetText());
  InstancerData *i_data = instancer_data(prim_id, true);
  if (i_data && mesh_data(prim_id)) {
    return i_data->prim_id;
  }
  return pxr::SdfPath();
}

pxr::SdfPathVector HydraSceneDelegate::GetInstancerPrototypes(pxr::SdfPath const &instancer_id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", instancer_id.GetText());
  InstancerData *i_data = instancer_data(instancer_id);
  return i_data->prototypes();
}

pxr::VtIntArray HydraSceneDelegate::GetInstanceIndices(pxr::SdfPath const &instancer_id,
                                                       pxr::SdfPath const &prototype_id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s, %s", instancer_id.GetText(), prototype_id.GetText());
  InstancerData *i_data = instancer_data(instancer_id);
  return i_data->indices(prototype_id);
}

pxr::GfMatrix4d HydraSceneDelegate::GetInstancerTransform(pxr::SdfPath const &instancer_id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", instancer_id.GetText());
  InstancerData *i_data = instancer_data(instancer_id);
  return i_data->transform(instancer_id);
}

pxr::HdVolumeFieldDescriptorVector HydraSceneDelegate::GetVolumeFieldDescriptors(
    pxr::SdfPath const &volume_id)
{
  CLOG_INFO(LOG_HYDRA_SCENE, 3, "%s", volume_id.GetText());
  VolumeData *v_data = volume_data(volume_id);
  return v_data->field_descriptors();
}

void HydraSceneDelegate::populate(Depsgraph *deps, View3D *v3d)
{
  bool is_populated = depsgraph != nullptr;

  depsgraph = deps;
  bmain = DEG_get_bmain(deps);
  scene = DEG_get_input_scene(depsgraph);
  view3d = v3d;

  if (is_populated) {
    check_updates();
  }
  else {
    set_light_shading_settings();
    set_world_shading_settings();
    update_collection();
    update_world();
  }
}

void HydraSceneDelegate::clear()
{
  for (auto &obj_data : objects_.values()) {
    obj_data->remove();
  }
  objects_.clear();
  instancer_data_->remove();
  for (auto &mat_data : materials_.values()) {
    mat_data->remove();
  }
  materials_.clear();

  depsgraph = nullptr;
  bmain = nullptr;
  scene = nullptr;
  view3d = nullptr;
}

pxr::SdfPath HydraSceneDelegate::prim_id(ID *id, const char *prefix) const
{
  /* Making id of object in form like <prefix>_<pointer in 16 hex digits format> */
  char name[32];
  snprintf(name, sizeof(name), "%s_%p", prefix, id);
  return GetDelegateID().AppendElementString(name);
}

pxr::SdfPath HydraSceneDelegate::object_prim_id(Object *object) const
{
  return prim_id((ID *)object, "O");
}

pxr::SdfPath HydraSceneDelegate::material_prim_id(Material *mat) const
{
  return prim_id((ID *)mat, "M");
}

pxr::SdfPath HydraSceneDelegate::instancer_prim_id() const
{
  return GetDelegateID().AppendElementString("Instancer");
}

pxr::SdfPath HydraSceneDelegate::world_prim_id() const
{
  return GetDelegateID().AppendElementString("World");
}

ObjectData *HydraSceneDelegate::object_data(pxr::SdfPath const &id) const
{
  if (id == world_prim_id()) {
    return world_data_.get();
  }
  auto name = id.GetName();
  pxr::SdfPath p_id = (STRPREFIX(name.c_str(), "SM_") || STRPREFIX(name.c_str(), "VF_")) ?
                          id.GetParentPath() :
                          id;
  auto obj_data = objects_.lookup_ptr(p_id);
  if (obj_data) {
    return obj_data->get();
  }

  InstancerData *i_data = instancer_data(p_id, true);
  if (i_data) {
    return i_data->object_data(id);
  }
  return nullptr;
}

MeshData *HydraSceneDelegate::mesh_data(pxr::SdfPath const &id) const
{
  return dynamic_cast<MeshData *>(object_data(id));
}

CurvesData *HydraSceneDelegate::curves_data(pxr::SdfPath const &id) const
{
  return dynamic_cast<CurvesData *>(object_data(id));
}

VolumeData *HydraSceneDelegate::volume_data(pxr::SdfPath const &id) const
{
  return dynamic_cast<VolumeData *>(object_data(id));
}

LightData *HydraSceneDelegate::light_data(pxr::SdfPath const &id) const
{
  return dynamic_cast<LightData *>(object_data(id));
}

MaterialData *HydraSceneDelegate::material_data(pxr::SdfPath const &id) const
{
  auto mat_data = materials_.lookup_ptr(id);
  if (!mat_data) {
    return nullptr;
  }
  return mat_data->get();
}

InstancerData *HydraSceneDelegate::instancer_data(pxr::SdfPath const &id, bool child_id) const
{
  pxr::SdfPath p_id;
  if (child_id) {
    /* Getting instancer path id from child Mesh instance (consist with 3 path elements) and
     * Light instance (consist with 4 path elements) */
    int n = id.GetPathElementCount();
    if (n == 3) {
      p_id = id.GetParentPath();
    }
    else if (n == 4) {
      p_id = id.GetParentPath().GetParentPath();
    }
  }
  else {
    p_id = id;
  }

  if (instancer_data_ && p_id == instancer_data_->prim_id) {
    return instancer_data_.get();
  }
  return nullptr;
}

void HydraSceneDelegate::update_world()
{
  if (!world_data_) {
    if (!shading_settings.use_scene_world || (shading_settings.use_scene_world && scene->world)) {
      world_data_ = std::make_unique<WorldData>(this, world_prim_id());
      world_data_->init();
      world_data_->insert();
    }
  }
  else {
    if (!shading_settings.use_scene_world || (shading_settings.use_scene_world && scene->world)) {
      world_data_->update();
    }
    else {
      world_data_->remove();
      world_data_ = nullptr;
    }
  }
}

void HydraSceneDelegate::check_updates()
{
  bool do_update_collection = false;
  bool do_update_world = false;

  if (set_world_shading_settings()) {
    do_update_world = true;
  }

  if (set_light_shading_settings()) {
    do_update_collection = true;
  }

  DEGIDIterData data = {0};
  data.graph = depsgraph;
  data.only_updated = true;
  ITER_BEGIN (DEG_iterator_ids_begin, DEG_iterator_ids_next, DEG_iterator_ids_end, &data, ID *, id)
  {
    CLOG_INFO(LOG_HYDRA_SCENE,
              0,
              "Update: %s [%s]",
              id->name,
              std::bitset<32>(id->recalc).to_string().c_str());

    switch (GS(id->name)) {
      case ID_OB: {
        do_update_collection = true;
      } break;

      case ID_MA: {
        MaterialData *mat_data = material_data(material_prim_id((Material *)id));
        if (mat_data) {
          mat_data->update();
        }
      } break;

      case ID_WO: {
        if (shading_settings.use_scene_world && id->recalc & ID_RECALC_SHADING) {
          do_update_world = true;
        }
      } break;

      case ID_SCE: {
        if ((id->recalc & ID_RECALC_COPY_ON_WRITE && !(id->recalc & ID_RECALC_SELECT)) ||
            id->recalc & (ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_BASE_FLAGS))
        {
          do_update_collection = true;
        }
        if (id->recalc & ID_RECALC_AUDIO_VOLUME &&
            ((scene->world && !world_data_) || (!scene->world && world_data_)))
        {
          do_update_world = true;
        }
      } break;

      default:
        break;
    }
  }
  ITER_END;

  if (do_update_world) {
    update_world();
  }
  if (do_update_collection) {
    update_collection();
  }
}

void HydraSceneDelegate::update_collection()
{
  Set<std::string> available_objects;

  DEGObjectIterSettings settings = {0};
  settings.depsgraph = depsgraph;
  settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
  DEGObjectIterData data = {0};
  data.settings = &settings;
  data.graph = settings.depsgraph;
  data.flag = settings.flags;

  instancer_data_->pre_update();

  ITER_BEGIN (DEG_iterator_objects_begin,
              DEG_iterator_objects_next,
              DEG_iterator_objects_end,
              &data,
              Object *,
              object)
  {
    if (data.dupli_object_current) {
      DupliObject *dupli = data.dupli_object_current;
      if (!ObjectData::is_supported(dupli->ob) ||
          !ObjectData::is_visible(this, data.dupli_parent, OB_VISIBLE_INSTANCES) ||
          (!shading_settings.use_scene_lights && object->type == OB_LAMP))
      {
        continue;
      }

      instancer_data_->update_instance(dupli);
      continue;
    }

    if (!ObjectData::is_supported(object) || !ObjectData::is_visible(this, object) ||
        (!shading_settings.use_scene_lights && object->type == OB_LAMP))
    {
      continue;
    }

    available_objects.add(object_prim_id(object).GetName());

    pxr::SdfPath id = object_prim_id(object);
    ObjectData *obj_data = object_data(id);
    if (obj_data) {
      obj_data->update();
    }
    else {
      obj_data = objects_.lookup_or_add(id, ObjectData::create(this, object, id)).get();
      obj_data->insert();
    }
  }
  ITER_END;

  instancer_data_->post_update();

  /* Remove unused objects */
  objects_.remove_if([&](auto item) {
    bool ret = !available_objects.contains(item.key.GetName());
    if (ret) {
      item.value->remove();
    }
    return ret;
  });

  /* Remove unused materials */
  Set<pxr::SdfPath> available_materials;
  for (auto &val : objects_.values()) {
    MeshData *m_data = dynamic_cast<MeshData *>(val.get());
    if (m_data) {
      m_data->available_materials(available_materials);
    }
    CurvesData *c_data = dynamic_cast<CurvesData *>(val.get());
    if (c_data) {
      c_data->available_materials(available_materials);
    }
    VolumeData *v_data = dynamic_cast<VolumeData *>(val.get());
    if (v_data) {
      v_data->available_materials(available_materials);
    }
  }
  instancer_data_->available_materials(available_materials);

  materials_.remove_if([&](auto item) {
    bool ret = !available_materials.contains(item.key);
    if (ret) {
      item.value->remove();
    }
    return ret;
  });
}

bool HydraSceneDelegate::set_light_shading_settings()
{
  if (!view3d) {
    return false;
  }
  ShadingSettings prev_settings(shading_settings);
  shading_settings.use_scene_lights = V3D_USES_SCENE_LIGHTS(view3d);
  return !(shading_settings == prev_settings);
}

bool HydraSceneDelegate::set_world_shading_settings()
{
  if (!view3d) {
    return false;
  }
  ShadingSettings prev_settings(shading_settings);
  shading_settings.use_scene_world = V3D_USES_SCENE_WORLD(view3d);
  shading_settings.studiolight_name = view3d->shading.lookdev_light;
  shading_settings.studiolight_rotation = view3d->shading.studiolight_rot_z;
  shading_settings.studiolight_intensity = view3d->shading.studiolight_intensity;
  return !(shading_settings == prev_settings);
}

}  // namespace blender::io::hydra
