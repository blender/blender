/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd.hh"

#include "usd_armature_utils.hh"
#include "usd_blend_shape_utils.hh"
#include "usd_hash_types.hh"
#include "usd_hierarchy_iterator.hh"
#include "usd_skel_convert.hh"
#include "usd_skel_root_utils.hh"
#include "usd_utils.hh"
#include "usd_writer_abstract.hh"
#include "usd_writer_armature.hh"
#include "usd_writer_camera.hh"
#include "usd_writer_curves.hh"
#include "usd_writer_hair.hh"
#include "usd_writer_light.hh"
#include "usd_writer_mesh.hh"
#include "usd_writer_metaball.hh"
#include "usd_writer_pointinstancer.hh"
#include "usd_writer_points.hh"
#include "usd_writer_text.hh"
#include "usd_writer_transform.hh"
#include "usd_writer_volume.hh"

#include <string>

#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "BLI_assert.h"

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

  switch (object->type) {
    case OB_EMPTY:
      /* Always assume empties are being exported intentionally. */
      return false;
    case OB_MESH:
    case OB_MBALL:
    case OB_FONT:
      return !params_.export_meshes;
    case OB_CAMERA:
      return !params_.export_cameras;
    case OB_LAMP:
      return !params_.export_lights;
    case OB_CURVES_LEGACY:
    case OB_CURVES:
      return !params_.export_curves;
    case OB_VOLUME:
      return !params_.export_volumes;
    case OB_ARMATURE:
      return !params_.export_armatures;
    case OB_POINTCLOUD:
      return !params_.export_points;

    default:
      /* Assume weak for all other types. */
      return true;
  }
}

void USDHierarchyIterator::release_writer(AbstractHierarchyWriter *writer)
{
  delete static_cast<USDAbstractWriter *>(writer);
}

std::string USDHierarchyIterator::make_valid_name(const std::string &name) const
{
  return make_safe_name(name, params_.allow_unicode);
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
  if (!params_.root_prim_path.empty()) {
    path = pxr::SdfPath(params_.root_prim_path + context->export_path);
  }
  else {
    path = pxr::SdfPath(context->export_path);
  }

  if (params_.merge_parent_xform && context->is_object_data_context && !context->is_parent) {
    bool can_merge_with_xform = true;
    if (params_.export_shapekeys && is_mesh_with_shape_keys(context->object)) {
      can_merge_with_xform = false;
    }

    if (params_.use_instancing && (context->is_prototype() || context->is_instance())) {
      can_merge_with_xform = false;
    }

    if (can_merge_with_xform) {
      path = path.GetParentPath();
    }
  }

  /* Returns the same path that was passed to `stage_` object during it's creation (via
   * `pxr::UsdStage::CreateNew` function). */
  const pxr::SdfLayerHandle root_layer = stage_->GetRootLayer();
  const std::string export_file_path = root_layer->GetRealPath();
  auto get_time_code = [this]() { return this->export_time_; };

  USDExporterContext exporter_context = USDExporterContext{bmain_,
                                                           depsgraph_,
                                                           stage_,
                                                           path,
                                                           get_time_code,
                                                           params_,
                                                           export_file_path,
                                                           nullptr,
                                                           nullptr,
                                                           this};

  /* Provides optional skel mapping hook. Now it's been used in USDPointInstancerWriter for write
   * base layer. */
  exporter_context.add_skel_mapping_fn = [this](const Object *obj, const pxr::SdfPath &usd_path) {
    this->add_usd_skel_export_mapping(obj, usd_path);
  };

  return exporter_context;
}

bool USDHierarchyIterator::determine_point_instancers(const HierarchyContext *context)
{
  if (!context) {
    return true;
  }

  if (context->object->type == OB_ARMATURE) {
    return true;
  }

  bool is_referencing_self = false;
  if (context->is_point_instancer()) {
    /* Mark the point instancer's children as a point instance. */
    USDExporterContext usd_export_context = create_usd_export_context(context);
    const ExportChildren *children = graph_children(context);

    pxr::SdfPath instancer_path;
    if (!params_.root_prim_path.empty()) {
      instancer_path = pxr::SdfPath(params_.root_prim_path + context->export_path);
    }
    else {
      instancer_path = pxr::SdfPath(context->export_path);
    }

    if (children != nullptr) {
      for (HierarchyContext *child_context : *children) {
        if (!child_context->original_export_path.empty()) {
          const pxr::SdfPath parent_export_path(context->export_path);
          const pxr::SdfPath children_original_export_path(child_context->original_export_path);

          /* Detect if the parent is referencing itself via a prototype. */
          if (parent_export_path.HasPrefix(children_original_export_path)) {
            is_referencing_self = true;
            break;
          }
        }

        pxr::SdfPath prototype_path;
        if (child_context->is_instance() && child_context->duplicator != nullptr) {
          /* When the current child context is point instancer's instance, use reference path
           * (original_export_path) as the prototype path. */
          if (!params_.root_prim_path.empty()) {
            prototype_path = pxr::SdfPath(params_.root_prim_path +
                                          child_context->original_export_path);
          }
          else {
            prototype_path = pxr::SdfPath(child_context->original_export_path);
          }

          prototype_paths_.lookup_or_add(instancer_path, {})
              .add(std::make_pair(prototype_path, child_context->object));
          child_context->is_point_instance = true;
        }
        else {
          /* When the current child context is point instancer's prototype, use its own export path
           * (export_path) as the prototype path. */
          if (!params_.root_prim_path.empty()) {
            prototype_path = pxr::SdfPath(params_.root_prim_path + child_context->export_path);
          }
          else {
            prototype_path = pxr::SdfPath(child_context->export_path);
          }

          prototype_paths_.lookup_or_add(instancer_path, {})
              .add(std::make_pair(prototype_path, child_context->object));
          child_context->is_point_proto = true;
        }
      }
    }

    /* MARK: If the "Instance on Points" node uses an Object as a prototype,
     * but the "Object Info" node has not enabled the "As Instance" option,
     * then the generated reference path is incorrect and refers to itself. */
    if (is_referencing_self) {
      BKE_reportf(
          params_.worker_status->reports,
          RPT_WARNING,
          "One or more objects used as prototypes in 'Instance on Points' nodes either do not "
          "have 'As Instance' enabled in their 'Object Info' nodes, or the prototype is the "
          "base geometry input itself. Both cases prevent valid point instancer export. If it's "
          "the former, enable 'As Instance' to avoid incorrect self-referencing.");

      /* Clear any paths which had already been accumulated. */
      Set<std::pair<pxr::SdfPath, Object *>> *paths = prototype_paths_.lookup_ptr(instancer_path);
      if (paths) {
        paths->clear();
      }
      for (HierarchyContext *child_context : *children) {
        child_context->is_point_instance = false;
        child_context->is_point_proto = false;
      }
    }
  }

  return !is_referencing_self;
}

AbstractHierarchyWriter *USDHierarchyIterator::create_transform_writer(
    const HierarchyContext *context)
{
  /* The transform writer is always called before data writers,
   * so determine if the #Xform's children is a point instancer before writing data. */
  if (params_.use_instancing) {
    if (!determine_point_instancers(context)) {
      /* If we could not determine that our point instancing setup is safe, we should not continue
       * writing. Continuing would result in enormous amounts of USD warnings about cyclic
       * references. */
      return nullptr;
    }
  }

  return new USDTransformWriter(create_usd_export_context(context));
}

AbstractHierarchyWriter *USDHierarchyIterator::create_data_writer(const HierarchyContext *context)
{
  USDExporterContext usd_export_context = create_usd_export_context(context);
  USDAbstractWriter *data_writer = nullptr;
  const Set<std::pair<pxr::SdfPath, Object *>> *proto_paths = prototype_paths_.lookup_ptr(
      usd_export_context.usd_path.GetParentPath());
  const bool use_point_instancing = context->is_point_instancer() &&
                                    (proto_paths && !proto_paths->is_empty());

  switch (context->object->type) {
    case OB_MESH:
      if (usd_export_context.export_params.export_meshes) {
        if (params_.use_instancing && use_point_instancing) {
          USDExporterContext mesh_context = create_point_instancer_context(context,
                                                                           usd_export_context);
          std::unique_ptr<USDMeshWriter> mesh_writer = std::make_unique<USDMeshWriter>(
              mesh_context);

          data_writer = new USDPointInstancerWriter(
              usd_export_context, *proto_paths, std::move(mesh_writer));
        }
        else {
          data_writer = new USDMeshWriter(usd_export_context);
        }
      }
      else {
        return nullptr;
      }
      break;
    case OB_CAMERA:
      if (usd_export_context.export_params.export_cameras) {
        data_writer = new USDCameraWriter(usd_export_context);
      }
      else {
        return nullptr;
      }
      break;
    case OB_LAMP:
      if (usd_export_context.export_params.export_lights) {
        data_writer = new USDLightWriter(usd_export_context);
      }
      else {
        return nullptr;
      }
      break;
    case OB_MBALL:
      data_writer = new USDMetaballWriter(usd_export_context);
      break;
    case OB_FONT:
      data_writer = new USDTextWriter(usd_export_context);
      break;
    case OB_CURVES_LEGACY:
    case OB_CURVES:
      if (usd_export_context.export_params.export_curves) {
        if (params_.use_instancing && use_point_instancing) {
          USDExporterContext curves_context = create_point_instancer_context(context,
                                                                             usd_export_context);
          std::unique_ptr<USDCurvesWriter> curves_writer = std::make_unique<USDCurvesWriter>(
              curves_context);

          data_writer = new USDPointInstancerWriter(
              usd_export_context, *proto_paths, std::move(curves_writer));
        }
        else {
          data_writer = new USDCurvesWriter(usd_export_context);
        }
      }
      else {
        return nullptr;
      }
      break;
    case OB_VOLUME:
      if (usd_export_context.export_params.export_volumes) {
        data_writer = new USDVolumeWriter(usd_export_context);
      }
      else {
        return nullptr;
      }
      break;
    case OB_ARMATURE:
      if (usd_export_context.export_params.export_armatures) {
        data_writer = new USDArmatureWriter(usd_export_context);
      }
      else {
        return nullptr;
      }
      break;
    case OB_POINTCLOUD:
      if (usd_export_context.export_params.export_points) {
        if (params_.use_instancing && use_point_instancing) {
          USDExporterContext point_cloud_context = create_point_instancer_context(
              context, usd_export_context);
          std::unique_ptr<USDPointsWriter> point_cloud_writer = std::make_unique<USDPointsWriter>(
              point_cloud_context);

          data_writer = new USDPointInstancerWriter(
              usd_export_context, *proto_paths, std::move(point_cloud_writer));
        }
        else {
          data_writer = new USDPointsWriter(usd_export_context);
        }
      }
      else {
        return nullptr;
      }
      break;

    case OB_EMPTY:
    case OB_SURF:
    case OB_SPEAKER:
    case OB_LIGHTPROBE:
    case OB_LATTICE:
    case OB_GREASE_PENCIL:
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

  return data_writer;
}

AbstractHierarchyWriter *USDHierarchyIterator::create_hair_writer(const HierarchyContext *context)
{
  if (!params_.export_hair) {
    return nullptr;
  }
  return new USDHairWriter(create_usd_export_context(context));
}

AbstractHierarchyWriter *USDHierarchyIterator::create_particle_writer(
    const HierarchyContext * /*context*/)
{
  return nullptr;
}

bool USDHierarchyIterator::include_data_writers(const HierarchyContext *context) const
{
  /* Don't generate data writers for instances. */

  return !(params_.use_instancing && context->is_instance());
}

bool USDHierarchyIterator::include_child_writers(const HierarchyContext *context) const
{
  /* Don't generate writers for children of instances. */

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

const blender::Map<pxr::SdfPath, blender::Vector<ID *>> &USDHierarchyIterator::
    get_exported_prim_map() const
{
  return exported_prim_map_;
}

pxr::UsdStageRefPtr USDHierarchyIterator::get_stage() const
{
  return stage_;
}

void USDHierarchyIterator::add_to_prim_map(const pxr::SdfPath &usd_path, const ID *id) const
{
  if (!id) {
    return;
  }
  ID *local_id = BKE_libblock_find_name(bmain_, GS(id->name), id->name + 2);
  if (local_id) {
    Vector<ID *> &id_list = exported_prim_map_.lookup_or_add_default(usd_path);
    if (!id_list.contains(local_id)) {
      id_list.append(local_id);
    }
  }
}

USDExporterContext USDHierarchyIterator::create_point_instancer_context(
    const HierarchyContext *context, const USDExporterContext &export_context) const
{
  BLI_assert(context && context->object);
  std::string base_name = std::string(BKE_id_name(context->object->id)).append("_base");
  std::string safe_name = make_safe_name(base_name, export_context.export_params.allow_unicode);

  pxr::SdfPath base_path = export_context.usd_path.GetParentPath().AppendChild(
      pxr::TfToken(safe_name));

  return {export_context.bmain,
          export_context.depsgraph,
          export_context.stage,
          base_path,
          export_context.get_time_code,
          export_context.export_params,
          export_context.export_file_path,
          export_context.export_image_fn,
          export_context.add_skel_mapping_fn,
          export_context.hierarchy_iterator};
}

}  // namespace blender::io::usd
