/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */
#include "usd.h"

#include "usd_hierarchy_iterator.h"
#include "usd_writer_abstract.h"
#include "usd_writer_armature.h"
#include "usd_writer_blendshape_mesh.h"
#include "usd_writer_camera.h"
#include "usd_writer_curves.h"
#include "usd_writer_hair.h"
#include "usd_writer_light.h"
#include "usd_writer_mesh.h"
#include "usd_writer_metaball.h"
#include "usd_writer_particle.h"
#include "usd_writer_skel_root.h"
#include "usd_writer_skinned_mesh.h"
#include "usd_writer_transform.h"
#include "usd_writer_volume.h"

#include <memory>
#include <string>

#include <pxr/base/tf/stringUtils.h>

#include "BKE_duplilist.h"

#include "BLI_assert.h"
#include "BLI_utildefines.h"

#include "DEG_depsgraph_query.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"

namespace blender::io::usd {

USDHierarchyIterator::USDHierarchyIterator(Main *bmain,
                                           Depsgraph *depsgraph,
                                           pxr::UsdStageRefPtr stage,
                                           const USDExportParams &params)
    : AbstractHierarchyIterator(bmain, depsgraph), stage_(stage), params_(params)
{
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

void USDHierarchyIterator::set_export_frame(float frame_nr)
{
  /* The USD stage is already set up to have FPS time-codes per frame. */
  export_time_ = pxr::UsdTimeCode(frame_nr);
}

std::string USDHierarchyIterator::get_export_file_path() const
{
  /* Returns the same path that was passed to `stage_` object during it's creation (via
   * `pxr::UsdStage::CreateNew` function). */
  const pxr::SdfLayerHandle root_layer = stage_->GetRootLayer();
  const std::string usd_export_file_path = root_layer->GetRealPath();
  return usd_export_file_path;
}

const pxr::UsdTimeCode &USDHierarchyIterator::get_export_time_code() const
{
  return export_time_;
}

USDExporterContext USDHierarchyIterator::create_usd_export_context(const HierarchyContext *context,
                                                                   bool mergeTransformAndShape)
{
  pxr::SdfPath path;
  if (params_.root_prim_path[0] != '\0') {
    path = pxr::SdfPath(params_.root_prim_path + context->export_path);
  }
  else {
    path = pxr::SdfPath(context->export_path);
  }

  // TODO: Somewhat of a workaround. There could be a better way to incoporate this...

  bool can_merge_with_xform = true;
  if (this->params_.export_armatures &&
      (is_skinned_mesh(context->object) || context->object->type == OB_ARMATURE)) {
    can_merge_with_xform = false;
  }

  if (this->params_.export_blendshapes && is_blendshape_mesh(context->object)) {
    can_merge_with_xform = false;
  }

  if (can_merge_with_xform && mergeTransformAndShape) {
    path = path.GetParentPath();
  }

  return USDExporterContext{bmain_, depsgraph_, stage_, path, this, params_};
}

AbstractHierarchyWriter *USDHierarchyIterator::create_transform_writer(
    const HierarchyContext *context)
{
  if (this->params_.export_armatures &&
      (is_skinned_mesh(context->object) || context->object->type == OB_ARMATURE)) {
    return new USDSkelRootWriter(create_usd_export_context(context));
  }

  if (this->params_.export_blendshapes && is_blendshape_mesh(context->object)) {
    return new USDSkelRootWriter(create_usd_export_context(context));
  }

  return new USDTransformWriter(create_usd_export_context(context));
}

AbstractHierarchyWriter *USDHierarchyIterator::create_data_writer(const HierarchyContext *context)
{
  if (context->is_instance() && params_.use_instancing) {
    return nullptr;
  }

  USDExporterContext usd_export_context = create_usd_export_context(
      context, params_.merge_transform_and_shape);
  USDAbstractWriter *data_writer = nullptr;

  switch (context->object->type) {
    case OB_MESH:
      if (usd_export_context.export_params.export_meshes) {
        if (usd_export_context.export_params.export_armatures &&
            is_skinned_mesh(context->object)) {
          data_writer = new USDSkinnedMeshWriter(usd_export_context);
        }
        else if (usd_export_context.export_params.export_blendshapes &&
                 is_blendshape_mesh(context->object)) {
          data_writer = new USDBlendShapeMeshWriter(usd_export_context);
        }
        else {
          data_writer = new USDMeshWriter(usd_export_context);
        }
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
    case OB_ARMATURE:
      if (usd_export_context.export_params.export_armatures) {
        data_writer = new USDArmatureWriter(usd_export_context);
      }
      else
        return nullptr;
      break;
    case OB_VOLUME:
      data_writer = new USDVolumeWriter(usd_export_context);
      break;
    case OB_EMPTY:
    case OB_SURF:
    case OB_FONT:
    case OB_SPEAKER:
    case OB_LIGHTPROBE:
    case OB_LATTICE:
    case OB_GPENCIL_LEGACY:
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
  return new USDHairWriter(create_usd_export_context(context));
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
  return new USDParticleWriter(create_usd_export_context(context));
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

}  // namespace blender::io::usd
