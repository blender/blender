/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd.hh"

#include "usd_armature_utils.hh"
#include "usd_blend_shape_utils.hh"
#include "usd_hierarchy_iterator.hh"
#include "usd_skel_convert.hh"
#include "usd_skel_root_utils.hh"
#include "usd_writer_abstract.hh"
#include "usd_writer_armature.hh"
#include "usd_writer_camera.hh"
#include "usd_writer_curves.hh"
#include "usd_writer_hair.hh"
#include "usd_writer_light.hh"
#include "usd_writer_mesh.hh"
#include "usd_writer_metaball.hh"
#include "usd_writer_particle.h"
#include "usd_writer_transform.hh"
#include "usd_writer_volume.hh"

#include <memory>
#include <string>

#include <pxr/base/tf/stringUtils.h>

#include "BKE_duplilist.hh"
#include "BKE_material.h"

#include "BLI_assert.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "DEG_depsgraph_query.hh"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"

#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"

static const Material** get_materials_from_data(const Object* object)
{
  Material ***mat_ar_p = BKE_object_material_array_p(const_cast<Object*>(object));
  return mat_ar_p ? const_cast<const Material**>(*mat_ar_p) : nullptr;
}

static short get_materials_count_from_data(const Object* object)
{
  short *len_p = BKE_object_material_len_p(const_cast<Object*>(object));
  return len_p ? *len_p : 0;
}



namespace blender::io::usd {

USDHierarchyIterator::USDHierarchyIterator(Main *bmain,
                                           Depsgraph *depsgraph,
                                           pxr::UsdStageRefPtr stage,
                                           const USDExportParams &params)
    : AbstractHierarchyIterator(bmain, depsgraph), stage_(stage), params_(params)
{
  DEGObjectIterSettings deg_iter_settings{};
  deg_iter_settings.depsgraph = depsgraph_;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
    process_names_for_object(object);
  }
  DEG_OBJECT_ITER_END;
}

bool USDHierarchyIterator::mark_as_weak_export(const Object *object) const
{
  if (params_.selected_objects_only && (object->base_flag & BASE_SELECTED) == 0) {
    return true;
  }
  return false;
}

void USDHierarchyIterator::release_writer(AbstractHierarchyWriter *writer)
{
  delete static_cast<USDAbstractWriter *>(writer);
}

std::string USDHierarchyIterator::make_valid_name(const std::string &name) const
{
  return pxr::TfMakeValidIdentifier(name);
}

std::string USDHierarchyIterator::get_id_name(const ID* id) const
{
  if (id == nullptr) {
    return "";
  }

  std::string name = find_name(id);
  if (!name.empty()) {
    return name;
  }

  return make_valid_name(std::string(id->name + 2));
}

void USDHierarchyIterator::process_usd_skel() const
{
  skel_export_chaser(stage_,
                     armature_export_map_,
                     skinned_mesh_export_map_,
                     shape_key_mesh_export_map_,
                     depsgraph_);

  create_skel_roots(stage_, params_);
}

void USDHierarchyIterator::set_export_frame(float frame_nr)
{
  /* The USD stage is already set up to have FPS time-codes per frame. */
  export_time_ = pxr::UsdTimeCode(frame_nr);
}

USDExporterContext USDHierarchyIterator::create_usd_export_context(const HierarchyContext *context)
{
  pxr::SdfPath path;
  if (params_.root_prim_path[0] != '\0') {
    path = pxr::SdfPath(params_.root_prim_path + context->export_path);
  }
  else {
    path = pxr::SdfPath(context->export_path);
  }

  bool can_merge_with_xform = true;
  if (this->params_.export_armatures && (can_export_skinned_mesh(*context->object, depsgraph_) ||
                                         context->object->type == OB_ARMATURE))
  {
    can_merge_with_xform = false;
  }

  if (this->params_.export_shapekeys && is_mesh_with_shape_keys(context->object)) {
    can_merge_with_xform = false;
  }

  if (can_merge_with_xform && this->params_.merge_transform_and_shape) {
    path = path.GetParentPath();
  }

  /* Returns the same path that was passed to `stage_` object during it's creation (via
   * `pxr::UsdStage::CreateNew` function). */
  const pxr::SdfLayerHandle root_layer = stage_->GetRootLayer();
  const std::string export_file_path = root_layer->GetRealPath();
  auto get_time_code = [this]() { return this->export_time_; };

  return USDExporterContext{
      bmain_, depsgraph_, stage_, path, get_time_code, params_, export_file_path};
}

AbstractHierarchyWriter *USDHierarchyIterator::create_transform_writer(
    const HierarchyContext *context)
{
  USDTransformWriter *ret = new USDTransformWriter(create_usd_export_context(context));
  ret->set_iterator(this);
  return ret;
}

AbstractHierarchyWriter *USDHierarchyIterator::create_data_writer(const HierarchyContext *context)
{
  if (context->is_instance() && params_.use_instancing) {
    return nullptr;
  }

  USDExporterContext usd_export_context = create_usd_export_context(context);
  USDAbstractWriter *data_writer = nullptr;

  switch (context->object->type) {
    case OB_MESH:
      if (usd_export_context.export_params.export_meshes) {
        data_writer = new USDMeshWriter(usd_export_context);
      }
      else
        return nullptr;
      break;
    case OB_CAMERA:
      if (usd_export_context.export_params.export_cameras)
        data_writer = new USDCameraWriter(usd_export_context);
      else
        return nullptr;
      break;
    case OB_LAMP:
      if (usd_export_context.export_params.export_lights)
        data_writer = new USDLightWriter(usd_export_context);
      else
        return nullptr;
      break;
    case OB_MBALL:
      data_writer = new USDMetaballWriter(usd_export_context);
      break;
    case OB_CURVES_LEGACY:
    case OB_CURVES:
      if (usd_export_context.export_params.export_curves) {
        data_writer = new USDCurvesWriter(usd_export_context);
      }
      else {
        return nullptr;
      }
      break;
    case OB_VOLUME:
      data_writer = new USDVolumeWriter(usd_export_context);
      break;
    case OB_ARMATURE:
      if (usd_export_context.export_params.export_armatures) {
        data_writer = new USDArmatureWriter(usd_export_context);
      }
      else {
        return nullptr;
      }
      break;
    case OB_EMPTY:
    case OB_SURF:
    case OB_FONT:
    case OB_SPEAKER:
    case OB_LIGHTPROBE:
    case OB_LATTICE:
    case OB_GPENCIL_LEGACY:
    case OB_GREASE_PENCIL:
    case OB_POINTCLOUD:
      return nullptr;
    case OB_TYPE_MAX:
      BLI_assert_msg(0, "OB_TYPE_MAX should not be used");
      return nullptr;
    default:
      BLI_assert_unreachable();
      return nullptr;
  }

  if (data_writer && !data_writer->is_supported(context)) {
    delete data_writer;
    return nullptr;
  }

  if (data_writer && (params_.export_armatures || params_.export_shapekeys)) {
    add_usd_skel_export_mapping(context->object, data_writer->usd_path());
  }

  data_writer->set_iterator(this);

  return data_writer;
}

AbstractHierarchyWriter *USDHierarchyIterator::create_hair_writer(const HierarchyContext *context)
{
  if (context->is_instance() && params_.use_instancing) {
    return nullptr;
  }

  if (!params_.export_hair) {
    return nullptr;
  }
  USDHairWriter *ret = new USDHairWriter(create_usd_export_context(context));
  ret->set_iterator(this);
  return ret;
}

AbstractHierarchyWriter *USDHierarchyIterator::create_particle_writer(
    const HierarchyContext *context)
{
  if (context->is_instance() && params_.use_instancing) {
    return nullptr;
  }

  if (!params_.export_particles) {
    return nullptr;
  }

  USDParticleWriter *ret = new USDParticleWriter(create_usd_export_context(context));
  ret->set_iterator(this);
  return ret;
}

/* Don't generate data writers for instances. */
bool USDHierarchyIterator::include_data_writers(const HierarchyContext *context) const
{
  if (!context) {
    return false;
  }

  return !(params_.use_instancing && context->is_instance());
}

/* Don't generate writers for children of instances. */
bool USDHierarchyIterator::include_child_writers(const HierarchyContext *context) const
{
  if (!context) {
    return false;
  }

  return !(params_.use_instancing && context->is_instance());
}

void USDHierarchyIterator::add_usd_skel_export_mapping(const Object *obj, const pxr::SdfPath &path)
{
  if (params_.export_shapekeys && is_mesh_with_shape_keys(obj)) {
    shape_key_mesh_export_map_.add(obj, path);
  }

  if (params_.export_armatures && obj->type == OB_ARMATURE) {
    armature_export_map_.add(obj, path);
  }

  if (params_.export_armatures && obj->type == OB_MESH &&
      can_export_skinned_mesh(*obj, depsgraph_))
  {
    skinned_mesh_export_map_.add(obj, path);
  }
}

bool USDHierarchyIterator::id_needs_display_name(const ID *id) const
{
  size_t length_in_bytes = 0;
  const std::string id_name(id->name + 2);
  const size_t length_in_characters = BLI_strlen_utf8_ex(id_name.c_str(), &length_in_bytes);
  if (length_in_bytes != length_in_characters) {
    /* Length is shorter likely due to unicode characters. */
    return true;
  }

  if (id_name != pxr::TfMakeValidIdentifier(id_name)) {
    /* Something invalid was converted into an underscore. */
    return true;
  }

  return false;
}

bool USDHierarchyIterator::object_needs_display_name(const Object *object) const
{
  return id_needs_display_name(reinterpret_cast<const ID *>(object));
}

bool USDHierarchyIterator::object_data_needs_display_name(const Object *object) const
{
  if (!object->data) {
    return false;
  }

  return id_needs_display_name(reinterpret_cast<const ID *>(object->data));
}

std::string USDHierarchyIterator::generate_unique_name(const std::string token)
{
  char name[64];
  int count = 0;
  BLI_snprintf(name, 64, "%s", token.c_str());

  while (prim_names_.contains(name)) {
    count += 1;
    BLI_snprintf(name, 64, "%s_%03d", token.c_str(), count);
  }

  return std::string(name);
}

void USDHierarchyIterator::store_name(const ID *id, const std::string name)
{
  prim_names_map_.add(id, name);
  prim_names_.add(name);
}

std::string USDHierarchyIterator::find_name(const ID *id) const
{
  return prim_names_map_.lookup_default(id, std::string());
}

void USDHierarchyIterator::process_names_for_object(const Object *object)
{
  const short id_code = (object->data ? GS(reinterpret_cast<const ID *>(object->data)->name) :
                                        GS(object->id.name));
  const std::string token(BKE_idtype_idcode_to_name(id_code));

  if (object_needs_display_name(object)) {
    const std::string obj_name = generate_unique_name(token);
    store_name(&object->id, obj_name);

    if (object->totcol) {
      process_materials(const_cast<const Material **>(object->mat), object->totcol);
    }
  }

  if (object->data) {
    if (object_data_needs_display_name(object)) {
      ID* object_data = static_cast<ID*>(object->data);
      const std::string data_name = generate_unique_name(token + "_Data");
      store_name(object_data, data_name);
    }

    const Material **data_mats = get_materials_from_data(object);
    size_t data_count = get_materials_count_from_data(object);
    process_materials(data_mats, data_count);
  }
}

void USDHierarchyIterator::process_materials(const Material **materials, const size_t count)
{
  for (int m = 0; m < count; m++) {
    const Material *mat = materials[m];
    if (!mat) {
      continue;
    }

    if (id_needs_display_name(&mat->id)) {
      const std::string data_name = generate_unique_name("Material");
      store_name(&mat->id, data_name);
    }
  }
}

std::string USDHierarchyIterator::get_object_computed_name(const Object *object) const
{
  if (!object) {
    return "";
  }
  return find_name(&object->id);
}

std::string USDHierarchyIterator::get_object_data_computed_name(const Object *object) const
{
  if (!(object && object->data)) {
    return "";
  }

  ID* object_data = static_cast<ID*>(object->data);
  return find_name(object_data);
}

}  // namespace blender::io::usd
