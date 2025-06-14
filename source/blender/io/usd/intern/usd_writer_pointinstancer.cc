/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "usd_writer_pointinstancer.hh"
#include "usd_attribute_utils.hh"
#include "usd_utils.hh"
#include "usd_writer_curves.hh"
#include "usd_writer_mesh.hh"
#include "usd_writer_points.hh"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_collection.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "BLI_math_euler.hh"
#include "BLI_math_matrix.hh"

#include "DNA_collection_types.h"
#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"

#include <pxr/base/gf/math.h>
#include <pxr/base/gf/matrix3f.h>
#include <pxr/base/gf/quatd.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/range3f.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/scope.h>
#include <pxr/usd/usdGeom/xform.h>

#include "DEG_depsgraph_query.hh"
#include "IO_abstract_hierarchy_iterator.h"
#include <fmt/format.h>

namespace blender::io::usd {

USDPointInstancerWriter::USDPointInstancerWriter(
    const USDExporterContext &ctx,
    std::set<std::pair<pxr::SdfPath, Object *>> &prototype_paths,
    std::unique_ptr<USDAbstractWriter> base_writer)
    : USDAbstractWriter(ctx),
      base_writer_(std::move(base_writer)),
      prototype_paths_(prototype_paths)
{
}

void USDPointInstancerWriter::do_write(HierarchyContext &context)
{
  /* Write the base data first (e.g., mesh, curves, points) */
  if (base_writer_) {
    base_writer_->write(context);

    if (usd_export_context_.add_skel_mapping_fn &&
        (usd_export_context_.export_params.export_armatures ||
         usd_export_context_.export_params.export_shapekeys))
    {
      usd_export_context_.add_skel_mapping_fn(context.object, base_writer_->usd_path());
    }
  }

  const pxr::UsdStageRefPtr stage = usd_export_context_.stage;
  Object *object_eval = context.object;
  bke::GeometrySet instance_geometry_set = bke::object_get_evaluated_geometry_set(*object_eval);

  const bke::GeometryComponent *component = instance_geometry_set.get_component(
      bke::GeometryComponent::Type::Instance);

  const bke::Instances *instances = static_cast<const bke::InstancesComponent &>(*component).get();

  int instance_num = instances->instances_num();
  const pxr::SdfPath &usd_path = usd_export_context_.usd_path;
  const pxr::UsdGeomPointInstancer usd_instancer = pxr::UsdGeomPointInstancer::Define(stage,
                                                                                      usd_path);
  const pxr::UsdTimeCode timecode = get_export_time_code();

  Span<float4x4> transforms = instances->transforms();
  BLI_assert(transforms.size() >= instance_num);

  if (transforms.size() != instance_num) {
    BKE_reportf(this->reports(),
                RPT_ERROR,
                "Instances number '%d' doesn't match transforms size '%d'",
                instance_num,
                int(transforms.size()));
    return;
  }

  /* evaluated positions */
  pxr::UsdAttribute position_attr = usd_instancer.CreatePositionsAttr();
  pxr::VtArray<pxr::GfVec3f> positions(instance_num);
  for (int i = 0; i < instance_num; i++) {
    const float3 &pos = transforms[i].location();
    positions[i] = pxr::GfVec3f(pos.x, pos.y, pos.z);
  }
  blender::io::usd::set_attribute(position_attr, positions, timecode, usd_value_writer_);

  /* orientations */
  pxr::UsdAttribute orientations_attr = usd_instancer.CreateOrientationsAttr();
  pxr::VtArray<pxr::GfQuath> orientation(instance_num);
  for (int i = 0; i < instance_num; i++) {
    const float3 euler = float3(math::to_euler(math::normalize(transforms[i])));
    const math::Quaternion quat = math::to_quaternion(math::EulerXYZ(euler));
    orientation[i] = pxr::GfQuath(quat.w, pxr::GfVec3h(quat.x, quat.y, quat.z));
  }
  blender::io::usd::set_attribute(orientations_attr, orientation, timecode, usd_value_writer_);

  /* scales */
  pxr::UsdAttribute scales_attr = usd_instancer.CreateScalesAttr();
  pxr::VtArray<pxr::GfVec3f> scales(instance_num);
  for (int i = 0; i < instance_num; i++) {
    const MatBase<float, 4, 4> &mat = transforms[i];
    blender::float3 scale_vec = math::to_scale<true>(mat);
    scales[i] = pxr::GfVec3f(scale_vec.x, scale_vec.y, scale_vec.z);
  }
  blender::io::usd::set_attribute(scales_attr, scales, timecode, usd_value_writer_);

  /* other attr */
  bke::AttributeAccessor attributes_eval = *component->attributes();
  attributes_eval.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.name[0] == '.' || blender::bke::attribute_name_is_anonymous(iter.name) ||
        ELEM(iter.name, "instance_transform") || ELEM(iter.name, "scale") ||
        ELEM(iter.name, "orientation") || ELEM(iter.name, "mask") ||
        ELEM(iter.name, "proto_index") || ELEM(iter.name, "id"))
    {
      return;
    }

    this->write_attribute_data(iter, usd_instancer, timecode);
  });

  /* prototypes relations */
  const pxr::SdfPath protoParentPath = usd_path.AppendChild(pxr::TfToken("Prototypes"));
  pxr::UsdPrim prototypesOver = stage->DefinePrim(protoParentPath);
  pxr::SdfPathVector proto_wrapper_paths;

  std::map<std::string, int> proto_index_map;
  std::map<std::string, pxr::SdfPath> proto_path_map;

  if (!prototype_paths_.empty() && usd_instancer) {
    int iter = 0;

    for (const std::pair<pxr::SdfPath, Object *> &entry : prototype_paths_) {
      const pxr::SdfPath &source_path = entry.first;
      Object *obj = entry.second;

      if (source_path.IsEmpty()) {
        continue;
      }

      const pxr::SdfPath proto_path = protoParentPath.AppendChild(
          pxr::TfToken(proto_name_ + "_" + std::to_string(iter)));

      pxr::UsdPrim prim = stage->DefinePrim(proto_path);

      /* To avoid USD error of Unresolved reference prim path, make sure the referenced path
       * exists.  */
      stage->DefinePrim(source_path);
      prim.GetReferences().AddReference(pxr::SdfReference("", source_path));
      proto_wrapper_paths.push_back(proto_path);

      std::string ob_name = BKE_id_name(obj->id);
      proto_index_map[ob_name] = iter;
      proto_path_map[ob_name] = proto_path;

      ++iter;
    }
    usd_instancer.GetPrototypesRel().SetTargets(proto_wrapper_paths);
    prototypesOver.GetPrim().SetSpecifier(pxr::SdfSpecifierOver);
    stage->GetRootLayer()->Save();
  }

  /* proto indices */
  /* must be the last to populate */
  pxr::UsdAttribute proto_indices_attr = usd_instancer.CreateProtoIndicesAttr();
  pxr::VtArray<int> proto_indices;
  std::vector<std::pair<int, int>> collection_instance_object_count_map;

  Span<int> reference_handles = instances->reference_handles();
  Span<bke::InstanceReference> references = instances->references();
  std::map<std::string, int> final_proto_index_map;

  for (int i = 0; i < instance_num; i++) {
    bke::InstanceReference reference = references[reference_handles[i]];

    process_instance_reference(reference,
                               i,
                               proto_index_map,
                               final_proto_index_map,
                               proto_path_map,
                               stage,
                               proto_indices,
                               collection_instance_object_count_map);
  }

  blender::io::usd::set_attribute(proto_indices_attr, proto_indices, timecode, usd_value_writer_);

  /* Handle Collection Prototypes */
  if (!collection_instance_object_count_map.empty()) {
    handle_collection_prototypes(
        usd_instancer, timecode, instance_num, collection_instance_object_count_map);
  }

  /* Clean unused prototype. When finding prototype paths under the context of a point instancer,
   * all the prototypes are collected, even those used by lower-level nested child PointInstancers.
   * It can happen that different levels in nested PointInstancers share the same prototypes, but
   * if not, we need to clean the extra prototypes from the prototype relationship for a cleaner
   * USD export. */
  compact_prototypes(usd_instancer, timecode, proto_wrapper_paths);

  stage->GetRootLayer()->Save();
}

void USDPointInstancerWriter::process_instance_reference(
    const bke::InstanceReference &reference,
    int instance_index,
    std::map<std::string, int> &proto_index_map,
    std::map<std::string, int> &final_proto_index_map,
    std::map<std::string, pxr::SdfPath> &proto_path_map,
    pxr::UsdStageRefPtr stage,
    pxr::VtArray<int> &proto_indices,
    std::vector<std::pair<int, int>> &collection_instance_object_count_map)
{
  switch (reference.type()) {
    case bke::InstanceReference::Type::Object: {
      Object &object = reference.object();
      std::string ob_name = BKE_id_name(object.id);

      if (proto_index_map.find(ob_name) != proto_index_map.end()) {
        proto_indices.push_back(proto_index_map[ob_name]);

        final_proto_index_map[ob_name] = proto_index_map[ob_name];

        /* If the reference is Object, clear prototype's local transform to identity to avoid
         * double transforms. The PointInstancer will fully control instance placement. */
        override_transform(stage, proto_path_map[ob_name], float4x4::identity());
      }
      break;
    }

    case bke::InstanceReference::Type::Collection: {
      Collection &collection = reference.collection();
      int object_num = 0;
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (&collection, object) {
        std::string ob_name = BKE_id_name(object->id);

        if (proto_index_map.find(ob_name) != proto_index_map.end()) {
          object_num += 1;
          proto_indices.push_back(proto_index_map[ob_name]);

          final_proto_index_map[ob_name] = proto_index_map[ob_name];
        }
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
      collection_instance_object_count_map.push_back(std::make_pair(instance_index, object_num));
      break;
    }

    case bke::InstanceReference::Type::GeometrySet: {
      bke::GeometrySet geometry_set = reference.geometry_set();
      std::string set_name = geometry_set.name;

      if (proto_index_map.find(set_name) != proto_index_map.end()) {
        proto_indices.push_back(proto_index_map[set_name]);

        final_proto_index_map[set_name] = proto_index_map[set_name];
      }

      Vector<const bke::GeometryComponent *> components = geometry_set.get_components();
      for (const bke::GeometryComponent *comp : components) {
        if (const bke::Instances *instances =
                static_cast<const bke::InstancesComponent &>(*comp).get())
        {
          Span<int> ref_handles = instances->reference_handles();
          Span<bke::InstanceReference> refs = instances->references();

          /* If the top-level GeometrySet is not in proto_index_map, recursively traverse child
           * InstanceReferences to resolve prototype indices. If the name matches proto_index_map,
           * skip traversal to avoid duplicates, since GeometrySet names may overlap with object
           * names. */
          if (proto_index_map.find(set_name) == proto_index_map.end()) {
            for (int index = 0; index < ref_handles.size(); ++index) {
              const bke::InstanceReference &child_ref = refs[ref_handles[index]];

              /* Recursively traverse nested GeometrySets to resolve prototype indices for all
               * instances. */
              process_instance_reference(child_ref,
                                         instance_index,
                                         proto_index_map,
                                         final_proto_index_map,
                                         proto_path_map,
                                         stage,
                                         proto_indices,
                                         collection_instance_object_count_map);
            }
          }

          /* If the reference is GeometrySet, then override the transform with the transform of the
           * Instance inside this GeometrySet. */
          Span<float4x4> transforms = instances->transforms();
          if (transforms.size() == 1) {
            if (proto_path_map.find(set_name) != proto_path_map.end()) {
              override_transform(stage, proto_path_map[set_name], transforms[0]);
            }
          }
        }
      }
      break;
    }

    case bke::InstanceReference::Type::None:
    default:
      break;
  }
}

void USDPointInstancerWriter::compact_prototypes(const pxr::UsdGeomPointInstancer &usd_instancer,
                                                 const pxr::UsdTimeCode timecode,
                                                 const pxr::SdfPathVector &proto_paths)
{
  pxr::UsdAttribute proto_indices_attr = usd_instancer.GetProtoIndicesAttr();
  pxr::VtArray<int> proto_indices;
  if (!proto_indices_attr.Get(&proto_indices, timecode)) {
    return;
  }

  ///* Find actually used prototype indices */
  std::set<int> used_proto_indices(proto_indices.begin(), proto_indices.end());

  std::map<int, int> remap;
  int new_index = 0;
  for (int i = 0; i < proto_paths.size(); ++i) {
    if (used_proto_indices.count(i)) {
      remap[i] = new_index++;
    }
  }

  ///* Remap protoIndices */
  for (int &idx : proto_indices) {
    idx = remap[idx];
  }
  proto_indices_attr.Set(proto_indices, timecode);

  pxr::SdfPathVector compact_proto_paths;
  for (int i = 0; i < proto_paths.size(); ++i) {
    if (used_proto_indices.count(i)) {
      compact_proto_paths.push_back(proto_paths[i]);
    }
  }

  usd_instancer.GetPrototypesRel().SetTargets(compact_proto_paths);
}

void USDPointInstancerWriter::override_transform(pxr::UsdStageRefPtr stage,
                                                 const pxr::SdfPath &proto_path,
                                                 const float4x4 &transform)
{
  // Extract translation
  const float3 &pos = transform.location();
  pxr::GfVec3d override_position(pos.x, pos.y, pos.z);

  // Extract rotation
  const float3 euler = float3(math::to_euler(math::normalize(transform)));
  pxr::GfVec3f override_rotation(euler.x, euler.y, euler.z);

  // Extract scale
  const float3 scale_vec = math::to_scale<true>(transform);
  pxr::GfVec3f override_scale(scale_vec.x, scale_vec.y, scale_vec.z);

  pxr::UsdPrim prim = stage->GetPrimAtPath(proto_path);
  if (!prim) {
    return;
  }

  pxr::UsdGeomXformable xformable(prim);
  xformable.ClearXformOpOrder();
  xformable.AddTranslateOp().Set(override_position);
  xformable.AddRotateXYZOp().Set(override_rotation);
  xformable.AddScaleOp().Set(override_scale);
}

template<typename T>
static pxr::VtArray<T> DuplicateArray(const pxr::VtArray<T> &original, size_t copies)
{
  pxr::VtArray<T> newArray;
  size_t originalSize = original.size();
  newArray.resize(originalSize * copies);
  for (size_t i = 0; i < copies; ++i) {
    std::copy(original.begin(), original.end(), newArray.begin() + i * originalSize);
  }
  return newArray;
}

template<typename T, typename GetterFunc, typename CreatorFunc>
static void DuplicatePerInstanceAttribute(const GetterFunc &getter,
                                          const CreatorFunc &creator,
                                          size_t copies,
                                          const pxr::UsdTimeCode &timecode)
{
  pxr::VtArray<T> values;
  if (getter().Get(&values, timecode) && !values.empty()) {
    auto newValues = DuplicateArray(values, copies);
    creator().Set(newValues, timecode);
  }
}

template<typename T, typename GetterFunc, typename CreatorFunc>
static void ExpandAttributePerInstance(const GetterFunc &getter,
                                       const CreatorFunc &creator,
                                       const std::vector<std::pair<int, int>> &instance_object_map,
                                       const pxr::UsdTimeCode &timecode)
{
  // MARK: Handle Collection Prototypes
  // -----------------------------------------------------------------------------
  // In Blender, a Collection is not an actual Object type. When exporting, the iterator
  // flattens the Collection hierarchy, treating each object inside the Collection as an
  // individual prototype. However, all these prototypes share the same instance attributes
  // (e.g., positions, orientations, scales).
  //
  // To ensure correct arrangement, reading, and drawing in OpenUSD, we need to explicitly
  // duplicate the instance attributes across all prototypes derived from the Collection.
  pxr::VtArray<T> original_values;
  if (!getter().Get(&original_values, timecode) || original_values.empty()) {
    return;
  }

  pxr::VtArray<T> expanded_values;
  for (const auto &[instance_index, object_count] : instance_object_map) {
    if (instance_index < static_cast<int>(original_values.size())) {
      for (int i = 0; i < object_count; ++i) {
        expanded_values.push_back(original_values[instance_index]);
      }
    }
  }

  creator().Set(expanded_values, timecode);
}

void USDPointInstancerWriter::handle_collection_prototypes(
    const pxr::UsdGeomPointInstancer &usd_instancer,
    const pxr::UsdTimeCode timecode,
    int instance_num,
    const std::vector<std::pair<int, int>> &collection_instance_object_count_map)
{
  // Duplicate attributes
  if (usd_instancer.GetPositionsAttr().HasAuthoredValue()) {
    ExpandAttributePerInstance<pxr::GfVec3f>([&]() { return usd_instancer.GetPositionsAttr(); },
                                             [&]() { return usd_instancer.CreatePositionsAttr(); },
                                             collection_instance_object_count_map,
                                             timecode);
  }
  if (usd_instancer.GetOrientationsAttr().HasAuthoredValue()) {
    ExpandAttributePerInstance<pxr::GfQuath>(
        [&]() { return usd_instancer.GetOrientationsAttr(); },
        [&]() { return usd_instancer.CreateOrientationsAttr(); },
        collection_instance_object_count_map,
        timecode);
  }
  if (usd_instancer.GetScalesAttr().HasAuthoredValue()) {
    ExpandAttributePerInstance<pxr::GfVec3f>([&]() { return usd_instancer.GetScalesAttr(); },
                                             [&]() { return usd_instancer.CreateScalesAttr(); },
                                             collection_instance_object_count_map,
                                             timecode);
  }
  if (usd_instancer.GetVelocitiesAttr().HasAuthoredValue()) {
    ExpandAttributePerInstance<pxr::GfVec3f>(
        [&]() { return usd_instancer.GetVelocitiesAttr(); },
        [&]() { return usd_instancer.CreateVelocitiesAttr(); },
        collection_instance_object_count_map,
        timecode);
  }
  if (usd_instancer.GetAngularVelocitiesAttr().HasAuthoredValue()) {
    ExpandAttributePerInstance<pxr::GfVec3f>(
        [&]() { return usd_instancer.GetAngularVelocitiesAttr(); },
        [&]() { return usd_instancer.CreateAngularVelocitiesAttr(); },
        collection_instance_object_count_map,
        timecode);
  }

  // Duplicate Primvars
  const pxr::UsdGeomPrimvarsAPI primvars_api(usd_instancer);
  std::vector<pxr::UsdGeomPrimvar> primvars = primvars_api.GetPrimvars();
  for (const pxr::UsdGeomPrimvar &primvar : primvars) {
    if (!primvar.HasAuthoredValue()) {
      continue;
    }
    const pxr::TfToken name = primvar.GetPrimvarName();
    const pxr::SdfValueTypeName type = primvar.GetTypeName();
    const pxr::TfToken interp = primvar.GetInterpolation();
    auto create = [&]() { return primvars_api.CreatePrimvar(name, type, interp); };

    if (type == pxr::SdfValueTypeNames->FloatArray) {
      ExpandAttributePerInstance<float>(
          [&]() { return primvar; }, create, collection_instance_object_count_map, timecode);
    }
    else if (type == pxr::SdfValueTypeNames->IntArray) {
      ExpandAttributePerInstance<int>(
          [&]() { return primvar; }, create, collection_instance_object_count_map, timecode);
    }
    else if (type == pxr::SdfValueTypeNames->UCharArray) {
      ExpandAttributePerInstance<unsigned char>(
          [&]() { return primvar; }, create, collection_instance_object_count_map, timecode);
    }
    else if (type == pxr::SdfValueTypeNames->Float2Array) {
      ExpandAttributePerInstance<pxr::GfVec2f>(
          [&]() { return primvar; }, create, collection_instance_object_count_map, timecode);
    }
    else if (type == pxr::SdfValueTypeNames->Float3Array ||
             type == pxr::SdfValueTypeNames->Color3fArray ||
             type == pxr::SdfValueTypeNames->Color4fArray)
    {
      ExpandAttributePerInstance<pxr::GfVec3f>(
          [&]() { return primvar; }, create, collection_instance_object_count_map, timecode);
    }
    else if (type == pxr::SdfValueTypeNames->QuatfArray) {
      ExpandAttributePerInstance<pxr::GfQuatf>(
          [&]() { return primvar; }, create, collection_instance_object_count_map, timecode);
    }
    else if (type == pxr::SdfValueTypeNames->BoolArray) {
      ExpandAttributePerInstance<bool>(
          [&]() { return primvar; }, create, collection_instance_object_count_map, timecode);
    }
    else if (type == pxr::SdfValueTypeNames->StringArray) {
      ExpandAttributePerInstance<std::string>(
          [&]() { return primvar; }, create, collection_instance_object_count_map, timecode);
    }
  }

  // MARK: Ensure Instance Indices Exist
  // -----------------------------------------------------------------------------
  // If the PointInstancer has no authored instance indices, manually generate a default
  // sequence of indices to ensure the PointInstancer functions correctly in OpenUSD.
  // This guarantees that each instance can correctly reference its prototype.
  pxr::UsdAttribute proto_indices_attr = usd_instancer.GetProtoIndicesAttr();
  if (!proto_indices_attr.HasAuthoredValue()) {
    std::vector<int> index;
    for (int i = 0; i < prototype_paths_.size(); i++) {
      std::vector<int> current_proto_index(instance_num, i);
      index.insert(index.end(), current_proto_index.begin(), current_proto_index.end());
    }

    proto_indices_attr.Set(pxr::VtArray<int>(index.begin(), index.end()));
  }
}

void USDPointInstancerWriter::write_attribute_data(const bke::AttributeIter &attr,
                                                   const pxr::UsdGeomPointInstancer &usd_instancer,
                                                   const pxr::UsdTimeCode timecode)
{
  const std::optional<pxr::SdfValueTypeName> pv_type = convert_blender_type_to_usd(attr.data_type);

  if (!pv_type) {
    BKE_reportf(this->reports(),
                RPT_WARNING,
                "Attribute '%s' (Blender domain %d, type %d) cannot be converted to USD",
                attr.name.c_str(),
                int(attr.domain),
                attr.data_type);
    return;
  }

  const GVArray attribute = *attr.get();
  if (attribute.is_empty()) {
    return;
  }

  if (attr.name == "mask") {
    pxr::UsdAttribute idsAttr = usd_instancer.GetIdsAttr();
    if (!idsAttr) {
      idsAttr = usd_instancer.CreateIdsAttr();
    }

    pxr::UsdAttribute invisibleIdsAttr = usd_instancer.GetInvisibleIdsAttr();
    if (!invisibleIdsAttr) {
      invisibleIdsAttr = usd_instancer.CreateInvisibleIdsAttr();
    }

    const GVArray attribute = *attr.get();
    /// Retrieve mask values, store as int8_t to avoid std::vector<bool>.data() issues
    std::vector<int8_t> mask_values(attribute.size());
    attribute.materialize(IndexMask(attribute.size()), mask_values.data());

    pxr::VtArray<int64_t> ids;
    pxr::VtArray<int64_t> invisibleIds;
    ids.reserve(mask_values.size());

    for (int64_t i = 0; i < static_cast<int64_t>(mask_values.size()); ++i) {
      ids.push_back(i);
      if (mask_values[i] == 0) {
        invisibleIds.push_back(i);
      }
    }

    blender::io::usd::set_attribute(idsAttr, ids, timecode, usd_value_writer_);
    blender::io::usd::set_attribute(invisibleIdsAttr, invisibleIds, timecode, usd_value_writer_);
  }

  const pxr::TfToken pv_name(
      make_safe_name(attr.name, usd_export_context_.export_params.allow_unicode));
  const pxr::UsdGeomPrimvarsAPI pv_api = pxr::UsdGeomPrimvarsAPI(usd_instancer);

  pxr::UsdGeomPrimvar pv_attr = pv_api.CreatePrimvar(pv_name, *pv_type);

  copy_blender_attribute_to_primvar(
      attribute, attr.data_type, timecode, pv_attr, usd_value_writer_);
}

}  // namespace blender::io::usd
