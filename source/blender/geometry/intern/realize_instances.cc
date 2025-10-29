/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_join_geometries.hh"
#include "GEO_realize_instances.hh"

#include "DNA_object_types.h"

#include "BLI_array_utils.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.hh"
#include "BLI_noise.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_geometry_nodes_gizmos_transforms.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"
#include "BKE_type_conversions.hh"

#include "BLT_translation.hh"

namespace blender::geometry {

using blender::bke::AttrDomain;
using blender::bke::AttributeDomainAndType;
using blender::bke::GSpanAttributeWriter;
using blender::bke::InstanceReference;
using blender::bke::Instances;
using blender::bke::SpanAttributeWriter;

/**
 * An ordered set of attribute ids. Attributes are ordered to avoid name lookups in many places.
 * Once the attributes are ordered, they can just be referred to by index.
 */
struct OrderedAttributes {
  VectorSet<StringRef> ids;
  Vector<AttributeDomainAndType> kinds;

  int size() const
  {
    return this->kinds.size();
  }

  IndexRange index_range() const
  {
    return this->kinds.index_range();
  }
};

struct AttributeFallbacksArray {
  /**
   * Instance attribute values used as fallback when the geometry does not have the
   * corresponding attributes itself. The pointers point to attributes stored in the instances
   * component or in #r_temporary_arrays. The order depends on the corresponding #OrderedAttributes
   * instance.
   */
  Array<const void *> array;

  AttributeFallbacksArray(int size) : array(size, nullptr) {}
};

struct PointCloudRealizeInfo {
  const PointCloud *pointcloud = nullptr;
  /** Matches the order stored in #AllPointCloudsInfo.attributes. */
  Array<std::optional<GVArraySpan>> attributes;
  /** Id attribute on the point cloud. If there are no ids, this #Span is empty. */
  Span<float3> positions;
  VArray<float> radii;
  Span<int> stored_ids;
};

struct RealizePointCloudTask {
  /** Starting index in the final realized point cloud. */
  int start_index;
  /** Preprocessed information about the point cloud. */
  const PointCloudRealizeInfo *pointcloud_info;
  /** Transformation that is applied to all positions. */
  float4x4 transform;
  AttributeFallbacksArray attribute_fallbacks;
  /** Only used when the output contains an output attribute. */
  uint32_t id = 0;
};

/** Start indices in the final output mesh. */
struct MeshElementStartIndices {
  int vertex = 0;
  int edge = 0;
  int face = 0;
  int loop = 0;
};

struct MeshRealizeInfo {
  const Mesh *mesh = nullptr;
  Span<float3> positions;
  Span<int2> edges;
  OffsetIndices<int> faces;
  Span<int> corner_verts;
  Span<int> corner_edges;

  /** Maps old material indices to new material indices. */
  Array<int> material_index_map;
  /** Matches the order in #AllMeshesInfo.attributes. */
  Array<std::optional<GVArraySpan>> attributes;
  /** Vertex ids stored on the mesh. If there are no ids, this #Span is empty. */
  Span<int> stored_vertex_ids;
  VArray<int> material_indices;
  /** Custom normals are rotated based on each instance's transformation. */
  GVArraySpan custom_normal;
};

struct RealizeMeshTask {
  MeshElementStartIndices start_indices;
  const MeshRealizeInfo *mesh_info;
  /** Transformation that is applied to all positions. */
  float4x4 transform;
  AttributeFallbacksArray attribute_fallbacks;
  /** Only used when the output contains an output attribute. */
  uint32_t id = 0;
};

struct RealizeCurveInfo {
  const Curves *curves;
  /**
   * Matches the order in #AllCurvesInfo.attributes.
   */
  Array<std::optional<GVArraySpan>> attributes;

  /** ID attribute on the curves. If there are no ids, this #Span is empty. */
  Span<int> stored_ids;

  /**
   * Handle position attributes must be transformed along with positions. Accessing them in
   * advance isn't necessary theoretically, but is done to simplify other code and to avoid
   * some overhead.
   */
  Span<float3> handle_left;
  Span<float3> handle_right;

  /**
   * The radius attribute must be filled with a default of 1.0 if it
   * doesn't exist on some (but not all) of the input curves data-blocks.
   */
  Span<float> radius;

  /**
   * The resolution attribute must be filled with the default value if it does not exist on some
   * curves.
   */
  VArray<int> resolution;

  /**
   * The resolution attribute must be filled with the default value if it does not exist on some
   * curves.
   */
  Span<float> nurbs_weight;

  /** Custom normals are rotated based on each instance's transformation. */
  Span<float3> custom_normal;
};

/** Start indices in the final output curves data-block. */
struct CurvesElementStartIndices {
  int point = 0;
  int curve = 0;
  int custom_knot = 0;
};

struct RealizeCurveTask {
  CurvesElementStartIndices start_indices;

  const RealizeCurveInfo *curve_info;
  /** Transformation applied to the position of control points and handles. */
  float4x4 transform;
  AttributeFallbacksArray attribute_fallbacks;
  /** Only used when the output contains an output attribute. */
  uint32_t id = 0;
};

struct GreasePencilRealizeInfo {
  const GreasePencil *grease_pencil = nullptr;
  /** Matches the order in #AllGreasePencilsInfo.attributes. */
  Array<std::optional<GVArraySpan>> attributes;
  /** Maps old material indices to new material indices. */
  Array<int> material_index_map;
};

struct RealizeGreasePencilTask {
  /** Index where the first layer is realized in the final grease pencil. */
  int start_index;
  const GreasePencilRealizeInfo *grease_pencil_info;
  float4x4 transform;
  AttributeFallbacksArray attribute_fallbacks;
};

struct RealizeEditDataTask {
  const bke::GeometryComponentEditData *edit_data;
  float4x4 transform;
};

struct AllPointCloudsInfo {
  /** Ordering of all attributes that are propagated to the output point cloud generically. */
  OrderedAttributes attributes;
  /** Ordering of the original point clouds that are joined. */
  VectorSet<const PointCloud *> order;
  /** Preprocessed data about every original point cloud. This is ordered by #order. */
  Array<PointCloudRealizeInfo> realize_info;
  bool create_id_attribute = false;
  bool create_radius_attribute = false;
};

struct AllMeshesInfo {
  /** Ordering of all attributes that are propagated to the output mesh generically. */
  OrderedAttributes attributes;
  /** Ordering of the original meshes that are joined. */
  VectorSet<const Mesh *> order;
  /** Preprocessed data about every original mesh. This is ordered by #order. */
  Array<MeshRealizeInfo> realize_info;
  /** Ordered materials on the output mesh. */
  VectorSet<Material *> materials;
  bool create_id_attribute = false;
  bool create_material_index_attribute = false;
  bke::mesh::NormalJoinInfo custom_normal_info;

  /** True if we know that there are no loose edges in any of the input meshes. */
  bool no_loose_edges_hint = false;
  bool no_loose_verts_hint = false;
  bool no_overlapping_hint = false;
};

struct AllCurvesInfo {
  /** Ordering of all attributes that are propagated to the output curve generically. */
  OrderedAttributes attributes;
  /** Ordering of the original curves that are joined. */
  VectorSet<const Curves *> order;
  /** Preprocessed data about every original curve. This is ordered by #order. */
  Array<RealizeCurveInfo> realize_info;
  bool create_id_attribute = false;
  bool create_handle_postion_attributes = false;
  bool create_radius_attribute = false;
  bool create_custom_normal_attribute = false;
};

struct AllGreasePencilsInfo {
  /** Ordering of all attributes that are propagated to the output grease pencil generically. */
  OrderedAttributes attributes;
  /** Ordering of the original grease pencils that are joined. */
  VectorSet<const GreasePencil *> order;
  /** Preprocessed data about every original grease pencil. This is ordered by #order. */
  Array<GreasePencilRealizeInfo> realize_info;
  /** Ordered materials on the output grease pencil. */
  VectorSet<Material *> materials;
};

struct AllInstancesInfo {
  /** Stores an array of void pointer to attributes for each component. */
  Vector<AttributeFallbacksArray> attribute_fallback;
  /** Instance components to merge for output geometry. */
  Vector<bke::GeometryComponentPtr> instances_components_to_merge;
  /** Base transform for each instance component. */
  Vector<float4x4> instances_components_transforms;
};

/** Collects all tasks that need to be executed to realize all instances. */
struct GatherTasks {
  Vector<RealizePointCloudTask> pointcloud_tasks;
  Vector<RealizeMeshTask> mesh_tasks;
  Vector<RealizeCurveTask> curve_tasks;
  Vector<RealizeGreasePencilTask> grease_pencil_tasks;
  Vector<RealizeEditDataTask> edit_data_tasks;

  /* Volumes only have very simple support currently. Only the first found volume is put into the
   * output. */
  ImplicitSharingPtr<const bke::VolumeComponent> first_volume;
};

/** Current offsets while during the gather operation. */
struct GatherOffsets {
  int64_t pointcloud_offset = 0;
  struct {
    int64_t vertex = 0;
    int64_t edge = 0;
    int64_t face = 0;
    int64_t corner = 0;
  } mesh_offsets;
  struct {
    int64_t point = 0;
    int64_t curve = 0;
    int64_t custom_knot = 0;
  } curves_offsets;
  int64_t grease_pencil_layer_offset = 0;
};

struct GatherTasksInfo {
  /** Static information about all geometries that are joined. */
  const AllPointCloudsInfo &pointclouds;
  const AllMeshesInfo &meshes;
  const AllCurvesInfo &curves;
  const AllGreasePencilsInfo &grease_pencils;
  const OrderedAttributes &instances_attriubutes;
  bool create_id_attribute_on_any_component = false;

  /** Selection for top-level instances to realize. */
  IndexMask selection;

  /** Depth to realize instances for each selected top-level instance. */
  const VArray<int> &depths;

  /**
   * Under some circumstances, temporary arrays need to be allocated during the gather operation.
   * For example, when an instance attribute has to be realized as a different data type. This
   * array owns all the temporary arrays so that they can live until all processing is done.
   * Use #std::unique_ptr to avoid depending on whether #GArray has an inline buffer or not.
   */
  Vector<std::unique_ptr<GArray<>>> &r_temporary_arrays;

  AllInstancesInfo instances;

  /** All gathered tasks. */
  GatherTasks r_tasks;
  /** Current offsets while gathering tasks. */
  GatherOffsets r_offsets;
};

/**
 * Information about the parent instances in the current context.
 */
struct InstanceContext {
  /** Ordered by #AllPointCloudsInfo.attributes. */
  AttributeFallbacksArray pointclouds;
  /** Ordered by #AllMeshesInfo.attributes. */
  AttributeFallbacksArray meshes;
  /** Ordered by #AllCurvesInfo.attributes. */
  AttributeFallbacksArray curves;
  /** Ordered by #AllGreasePencilsInfo.attributes. */
  AttributeFallbacksArray grease_pencils;
  /** Ordered by #AllInstancesInfo.attributes. */
  AttributeFallbacksArray instances;
  /** Id mixed from all parent instances. */
  uint32_t id = 0;

  InstanceContext(const GatherTasksInfo &gather_info)
      : pointclouds(gather_info.pointclouds.attributes.size()),
        meshes(gather_info.meshes.attributes.size()),
        curves(gather_info.curves.attributes.size()),
        grease_pencils(gather_info.grease_pencils.attributes.size()),
        instances(gather_info.instances_attriubutes.size())
  {
    // empty
  }
};

static bool valid_int_num(const int64_t num)
{
  return num >= 0 && num <= INT32_MAX;
}

static int64_t get_final_points_num(const GatherTasks &tasks)
{
  int64_t points_num = 0;
  if (!tasks.pointcloud_tasks.is_empty()) {
    const RealizePointCloudTask &task = tasks.pointcloud_tasks.last();
    points_num += task.start_index + task.pointcloud_info->pointcloud->totpoint;
  }
  if (!tasks.mesh_tasks.is_empty()) {
    const RealizeMeshTask &task = tasks.mesh_tasks.last();
    points_num += task.start_indices.vertex + task.mesh_info->mesh->verts_num;
  }
  if (!tasks.curve_tasks.is_empty()) {
    const RealizeCurveTask &task = tasks.curve_tasks.last();
    points_num += task.start_indices.point + task.curve_info->curves->geometry.point_num;
  }
  return points_num;
}

static bool skip_transform(const float4x4 &transform)
{
  return math::is_equal(transform, float4x4::identity(), 1e-6f);
}

static void threaded_copy(const GSpan src, GMutableSpan dst)
{
  BLI_assert(src.size() == dst.size());
  BLI_assert(src.type() == dst.type());
  threading::parallel_for(IndexRange(src.size()), 1024, [&](const IndexRange range) {
    src.type().copy_construct_n(src.slice(range).data(), dst.slice(range).data(), range.size());
  });
}

static void threaded_fill(const GPointer value, GMutableSpan dst)
{
  BLI_assert(*value.type() == dst.type());
  threading::parallel_for(IndexRange(dst.size()), 1024, [&](const IndexRange range) {
    value.type()->fill_construct_n(value.get(), dst.slice(range).data(), range.size());
  });
}

static void copy_generic_attributes_to_result(
    const Span<std::optional<GVArraySpan>> src_attributes,
    const AttributeFallbacksArray &attribute_fallbacks,
    const OrderedAttributes &ordered_attributes,
    const FunctionRef<IndexRange(bke::AttrDomain)> &range_fn,
    MutableSpan<GSpanAttributeWriter> dst_attribute_writers)
{
  threading::parallel_for(
      dst_attribute_writers.index_range(), 10, [&](const IndexRange attribute_range) {
        for (const int attribute_index : attribute_range) {
          const bke::AttrDomain domain = ordered_attributes.kinds[attribute_index].domain;
          const IndexRange element_slice = range_fn(domain);

          GSpanAttributeWriter &writer = dst_attribute_writers[attribute_index];
          if (!writer) {
            continue;
          }
          GMutableSpan dst_span = writer.span.slice(element_slice);
          if (src_attributes[attribute_index].has_value()) {
            threaded_copy(*src_attributes[attribute_index], dst_span);
          }
          else {
            const CPPType &cpp_type = dst_span.type();
            const void *fallback = attribute_fallbacks.array[attribute_index] == nullptr ?
                                       cpp_type.default_value() :
                                       attribute_fallbacks.array[attribute_index];
            threaded_fill({cpp_type, fallback}, dst_span);
          }
        }
      });
}

static void create_result_ids(const RealizeInstancesOptions &options,
                              const Span<int> stored_ids,
                              const int task_id,
                              MutableSpan<int> dst_ids)
{
  if (options.keep_original_ids) {
    if (stored_ids.is_empty()) {
      dst_ids.fill(0);
    }
    else {
      dst_ids.copy_from(stored_ids);
    }
  }
  else {
    if (stored_ids.is_empty()) {
      threading::parallel_for(dst_ids.index_range(), 1024, [&](const IndexRange range) {
        for (const int i : range) {
          dst_ids[i] = noise::hash(task_id, i);
        }
      });
    }
    else {
      threading::parallel_for(dst_ids.index_range(), 1024, [&](const IndexRange range) {
        for (const int i : range) {
          dst_ids[i] = noise::hash(task_id, stored_ids[i]);
        }
      });
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Gather Realize Tasks
 * \{ */

/* Forward declaration. */
static void gather_realize_tasks_recursive(GatherTasksInfo &gather_info,
                                           const int current_depth,
                                           const int target_depth,
                                           const bke::GeometrySet &geometry_set,
                                           const float4x4 &base_transform,
                                           const InstanceContext &base_instance_context);

/**
 * Checks which of the #ordered_attributes exist on the #instances_component. For each attribute
 * that exists on the instances, a pair is returned that contains the attribute index and the
 * corresponding attribute data.
 */
static Vector<std::pair<int, GSpan>> prepare_attribute_fallbacks(
    GatherTasksInfo &gather_info,
    const Instances &instances,
    const OrderedAttributes &ordered_attributes)
{
  Vector<std::pair<int, GSpan>> attributes_to_override;
  const bke::AttributeAccessor attributes = instances.attributes();
  attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    const int attribute_index = ordered_attributes.ids.index_of_try(iter.name);
    if (attribute_index == -1) {
      /* The attribute is not propagated to the final geometry. */
      return;
    }
    const bke::GAttributeReader attribute = iter.get();
    if (!attribute || !attribute.varray.is_span()) {
      return;
    }
    GSpan span = attribute.varray.get_internal_span();
    const bke::AttrType expected_type = ordered_attributes.kinds[attribute_index].data_type;
    if (iter.data_type != expected_type) {
      const CPPType &from_type = span.type();
      const CPPType &to_type = bke::attribute_type_to_cpp_type(expected_type);
      const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
      if (!conversions.is_convertible(from_type, to_type)) {
        /* Ignore the attribute because it can not be converted to the desired type. */
        return;
      }
      /* Convert the attribute on the instances component to the expected attribute type. */
      std::unique_ptr<GArray<>> temporary_array = std::make_unique<GArray<>>(
          to_type, instances.instances_num());
      conversions.convert_to_initialized_n(span, temporary_array->as_mutable_span());
      span = temporary_array->as_span();
      gather_info.r_temporary_arrays.append(std::move(temporary_array));
    }
    attributes_to_override.append({attribute_index, span});
  });
  return attributes_to_override;
}

/**
 * Calls #fn for every geometry in the given #InstanceReference. Also passes on the transformation
 * that is applied to every instance.
 */
static void foreach_geometry_in_reference(
    const InstanceReference &reference,
    const float4x4 &base_transform,
    const uint32_t id,
    FunctionRef<void(const bke::GeometrySet &geometry_set, const float4x4 &transform, uint32_t id)>
        fn)
{
  bke::GeometrySet geometry_set;
  reference.to_geometry_set(geometry_set);
  fn(geometry_set, base_transform, id);
}

static void gather_realize_tasks_for_instances(GatherTasksInfo &gather_info,
                                               const int current_depth,
                                               const int target_depth,
                                               const Instances &instances,
                                               const float4x4 &base_transform,
                                               const InstanceContext &base_instance_context)
{
  const Span<InstanceReference> references = instances.references();
  const Span<int> handles = instances.reference_handles();
  const Span<float4x4> transforms = instances.transforms();

  Span<int> stored_instance_ids;
  if (gather_info.create_id_attribute_on_any_component) {
    bke::GAttributeReader ids = instances.attributes().lookup("id");
    if (ids && ids.domain == bke::AttrDomain::Instance && ids.varray.type().is<int>() &&
        ids.varray.is_span())
    {
      stored_instance_ids = ids.varray.get_internal_span().typed<int>();
    }
  }

  /* Prepare attribute fallbacks. */
  InstanceContext instance_context = base_instance_context;
  Vector<std::pair<int, GSpan>> pointcloud_attributes_to_override = prepare_attribute_fallbacks(
      gather_info, instances, gather_info.pointclouds.attributes);
  Vector<std::pair<int, GSpan>> mesh_attributes_to_override = prepare_attribute_fallbacks(
      gather_info, instances, gather_info.meshes.attributes);
  Vector<std::pair<int, GSpan>> curve_attributes_to_override = prepare_attribute_fallbacks(
      gather_info, instances, gather_info.curves.attributes);
  Vector<std::pair<int, GSpan>> grease_pencil_attributes_to_override = prepare_attribute_fallbacks(
      gather_info, instances, gather_info.grease_pencils.attributes);
  Vector<std::pair<int, GSpan>> instance_attributes_to_override = prepare_attribute_fallbacks(
      gather_info, instances, gather_info.instances_attriubutes);

  const bool is_top_level = current_depth == 0;
  /* If at top level, get instance indices from selection field, else use all instances. */
  const IndexMask indices = is_top_level ? gather_info.selection :
                                           IndexMask(IndexRange(instances.instances_num()));
  indices.foreach_index([&](const int i) {
    /* If at top level, retrieve depth from gather_info, else continue with target_depth. */
    const int child_target_depth = is_top_level ? gather_info.depths[i] : target_depth;
    const int handle = handles[i];
    const float4x4 &transform = transforms[i];
    const InstanceReference &reference = references[handle];
    const float4x4 new_base_transform = base_transform * transform;

    /* Update attribute fallbacks for the current instance. */
    for (const std::pair<int, GSpan> &pair : pointcloud_attributes_to_override) {
      instance_context.pointclouds.array[pair.first] = pair.second[i];
    }
    for (const std::pair<int, GSpan> &pair : mesh_attributes_to_override) {
      instance_context.meshes.array[pair.first] = pair.second[i];
    }
    for (const std::pair<int, GSpan> &pair : curve_attributes_to_override) {
      instance_context.curves.array[pair.first] = pair.second[i];
    }
    for (const std::pair<int, GSpan> &pair : grease_pencil_attributes_to_override) {
      instance_context.grease_pencils.array[pair.first] = pair.second[i];
    }
    for (const std::pair<int, GSpan> &pair : instance_attributes_to_override) {
      instance_context.instances.array[pair.first] = pair.second[i];
    }

    uint32_t local_instance_id = 0;
    if (gather_info.create_id_attribute_on_any_component) {
      if (stored_instance_ids.is_empty()) {
        local_instance_id = uint32_t(i);
      }
      else {
        local_instance_id = uint32_t(stored_instance_ids[i]);
      }
    }
    const uint32_t instance_id = noise::hash(base_instance_context.id, local_instance_id);

    /* Add realize tasks for all referenced geometry sets recursively. */
    foreach_geometry_in_reference(reference,
                                  new_base_transform,
                                  instance_id,
                                  [&](const bke::GeometrySet &instance_geometry_set,
                                      const float4x4 &transform,
                                      const uint32_t id) {
                                    instance_context.id = id;
                                    gather_realize_tasks_recursive(gather_info,
                                                                   current_depth + 1,
                                                                   child_target_depth,
                                                                   instance_geometry_set,
                                                                   transform,
                                                                   instance_context);
                                  });
  });
}

/**
 * Gather tasks for all geometries in the #geometry_set.
 */
static void gather_realize_tasks_recursive(GatherTasksInfo &gather_info,
                                           const int current_depth,
                                           const int target_depth,
                                           const bke::GeometrySet &geometry_set,
                                           const float4x4 &base_transform,
                                           const InstanceContext &base_instance_context)
{
  for (const bke::GeometryComponent *component : geometry_set.get_components()) {
    const bke::GeometryComponent::Type type = component->type();
    switch (type) {
      case bke::GeometryComponent::Type::Mesh: {
        const Mesh *mesh = (*static_cast<const bke::MeshComponent *>(component)).get();
        if (mesh != nullptr && mesh->verts_num > 0) {
          const int mesh_index = gather_info.meshes.order.index_of(mesh);
          const MeshRealizeInfo &mesh_info = gather_info.meshes.realize_info[mesh_index];
          gather_info.r_tasks.mesh_tasks.append({{int(gather_info.r_offsets.mesh_offsets.vertex),
                                                  int(gather_info.r_offsets.mesh_offsets.edge),
                                                  int(gather_info.r_offsets.mesh_offsets.face),
                                                  int(gather_info.r_offsets.mesh_offsets.corner)},
                                                 &mesh_info,
                                                 base_transform,
                                                 base_instance_context.meshes,
                                                 base_instance_context.id});
          gather_info.r_offsets.mesh_offsets.vertex += mesh->verts_num;
          gather_info.r_offsets.mesh_offsets.edge += mesh->edges_num;
          gather_info.r_offsets.mesh_offsets.corner += mesh->corners_num;
          gather_info.r_offsets.mesh_offsets.face += mesh->faces_num;
        }
        break;
      }
      case bke::GeometryComponent::Type::PointCloud: {
        const auto &pointcloud_component = *static_cast<const bke::PointCloudComponent *>(
            component);
        const PointCloud *pointcloud = pointcloud_component.get();
        if (pointcloud != nullptr && pointcloud->totpoint > 0) {
          const int pointcloud_index = gather_info.pointclouds.order.index_of(pointcloud);
          const PointCloudRealizeInfo &pointcloud_info =
              gather_info.pointclouds.realize_info[pointcloud_index];
          gather_info.r_tasks.pointcloud_tasks.append(
              {int(gather_info.r_offsets.pointcloud_offset),
               &pointcloud_info,
               base_transform,
               base_instance_context.pointclouds,
               base_instance_context.id});
          gather_info.r_offsets.pointcloud_offset += pointcloud->totpoint;
        }
        break;
      }
      case bke::GeometryComponent::Type::Curve: {
        const auto &curve_component = *static_cast<const bke::CurveComponent *>(component);
        const Curves *curves = curve_component.get();
        if (curves != nullptr && curves->geometry.curve_num > 0) {
          const int curve_index = gather_info.curves.order.index_of(curves);
          const RealizeCurveInfo &curve_info = gather_info.curves.realize_info[curve_index];
          gather_info.r_tasks.curve_tasks.append(
              {{int(gather_info.r_offsets.curves_offsets.point),
                int(gather_info.r_offsets.curves_offsets.curve),
                int(gather_info.r_offsets.curves_offsets.custom_knot)},
               &curve_info,
               base_transform,
               base_instance_context.curves,
               base_instance_context.id});
          gather_info.r_offsets.curves_offsets.point += curves->geometry.point_num;
          gather_info.r_offsets.curves_offsets.curve += curves->geometry.curve_num;
          gather_info.r_offsets.curves_offsets.custom_knot += curves->geometry.custom_knot_num;
        }
        break;
      }
      case bke::GeometryComponent::Type::GreasePencil: {
        const auto &grease_pencil_component = *static_cast<const bke::GreasePencilComponent *>(
            component);
        const GreasePencil *grease_pencil = grease_pencil_component.get();
        if (grease_pencil != nullptr && !grease_pencil->layers().is_empty()) {
          const int grease_pencil_index = gather_info.grease_pencils.order.index_of(grease_pencil);
          const GreasePencilRealizeInfo &grease_pencil_info =
              gather_info.grease_pencils.realize_info[grease_pencil_index];
          gather_info.r_tasks.grease_pencil_tasks.append(
              {int(gather_info.r_offsets.grease_pencil_layer_offset),
               &grease_pencil_info,
               base_transform,
               base_instance_context.grease_pencils});
          gather_info.r_offsets.grease_pencil_layer_offset += grease_pencil->layers().size();
        }
        break;
      }
      case bke::GeometryComponent::Type::Instance: {
        if (current_depth == target_depth) {
          gather_info.instances.attribute_fallback.append(base_instance_context.instances);
          gather_info.instances.instances_components_to_merge.append(component->copy());
          gather_info.instances.instances_components_transforms.append(base_transform);
        }
        else {
          const Instances *instances =
              (*static_cast<const bke::InstancesComponent *>(component)).get();
          if (instances != nullptr && instances->instances_num() > 0) {
            gather_realize_tasks_for_instances(gather_info,
                                               current_depth,
                                               target_depth,
                                               *instances,
                                               base_transform,
                                               base_instance_context);
          }
        }
        break;
      }
      case bke::GeometryComponent::Type::Volume: {
        if (!gather_info.r_tasks.first_volume) {
          const bke::VolumeComponent *volume_component = static_cast<const bke::VolumeComponent *>(
              component);
          volume_component->add_user();
          gather_info.r_tasks.first_volume = ImplicitSharingPtr<const bke::VolumeComponent>(
              volume_component);
        }
        break;
      }
      case bke::GeometryComponent::Type::Edit: {
        const auto *edit_component = static_cast<const bke::GeometryComponentEditData *>(
            component);
        if (edit_component->gizmo_edit_hints_ || edit_component->curves_edit_hints_) {
          gather_info.r_tasks.edit_data_tasks.append({edit_component, base_transform});
        }
        break;
      }
    }
  }
}

static void gather_attribute_propagation_components(
    const bke::GeometrySet &geometry,
    const bke::GeometryComponent::Type component_type,
    const RealizeInstancesOptions &options,
    const int current_depth,
    const std::optional<int> max_depth,
    Set<bke::GeometryComponentPtr> &r_components)
{
  if (const bke::GeometryComponent *component = geometry.get_component(component_type)) {
    if (r_components.add_as(component)) {
      component->add_user();
    }
  }
  if (current_depth == max_depth) {
    return;
  }
  const auto *instances_component = geometry.get_component<bke::InstancesComponent>();
  if (!instances_component) {
    return;
  }
  const bke::Instances *instances = instances_component->get();
  if (!instances) {
    return;
  }
  if (options.realize_instance_attributes) {
    if (r_components.add_as(instances_component)) {
      instances_component->add_user();
    }
  }
  for (const bke::InstanceReference &reference : instances->references()) {
    bke::GeometrySet reference_geometry;
    reference.to_geometry_set(reference_geometry);
    gather_attribute_propagation_components(
        reference_geometry, component_type, options, current_depth + 1, max_depth, r_components);
  }
}

static void gather_attribute_propagation_components_with_custom_depths(
    const bke::GeometrySet &geometry,
    const bke::GeometryComponent::Type component_type,
    const RealizeInstancesOptions &options,
    const VariedDepthOptions &varied_depth_option,
    Set<bke::GeometryComponentPtr> &r_components)
{
  if (const bke::GeometryComponent *component = geometry.get_component(component_type)) {
    if (r_components.add_as(component)) {
      component->add_user();
    }
  }
  const auto *instances_component = geometry.get_component<bke::InstancesComponent>();
  if (!instances_component) {
    return;
  }
  const bke::Instances *instances = instances_component->get();
  if (!instances) {
    return;
  }

  const Span<bke::InstanceReference> references = instances->references();
  const Span<int> handles = instances->reference_handles();
  const int references_num = references.size();
  Array<std::optional<int>> max_reference_depth(references_num, 0);

  varied_depth_option.selection.foreach_index([&](const int instance_i) {
    const int reference_i = handles[instance_i];
    const int instance_depth = varied_depth_option.depths[instance_i];
    std::optional<int> &max_depth = max_reference_depth[reference_i];
    if (!max_depth.has_value()) {
      /* Is already at max depth. */
      return;
    }
    if (instance_depth == VariedDepthOptions::MAX_DEPTH) {
      max_depth.reset();
      return;
    }
    max_depth = std::max<int>(*max_depth, instance_depth);
  });

  bool is_anything_realized = false;
  for (const int reference_i : IndexRange(references_num)) {
    const std::optional<int> max_depth = max_reference_depth[reference_i];
    if (max_depth == 0) {
      continue;
    }
    const bke::InstanceReference &reference = references[reference_i];
    bke::GeometrySet reference_geometry;
    reference.to_geometry_set(reference_geometry);
    gather_attribute_propagation_components(
        reference_geometry, component_type, options, 1, max_depth, r_components);
    is_anything_realized = true;
  }

  if (is_anything_realized) {
    if (options.realize_instance_attributes) {
      if (r_components.add_as(instances_component)) {
        instances_component->add_user();
      }
    }
  }
}

static bke::GeometrySet::GatheredAttributes gather_attributes_to_propagate(
    const bke::GeometrySet &geometry,
    const bke::GeometryComponent::Type component_type,
    const RealizeInstancesOptions &options,
    const VariedDepthOptions &varied_depth_option)
{
  const bke::AttributeFilter &attribute_filter = options.attribute_filter;

  const bke::InstancesComponent *top_level_instances_component =
      geometry.get_component<bke::InstancesComponent>();
  const int top_level_instances_num = top_level_instances_component ?
                                          top_level_instances_component->attribute_domain_size(
                                              AttrDomain::Instance) :
                                          0;

  /* Needs to take ownership because some components are only temporary otherwise. */
  Set<bke::GeometryComponentPtr> components;
  if (varied_depth_option.depths.get_if_single() == VariedDepthOptions::MAX_DEPTH &&
      varied_depth_option.selection.size() == top_level_instances_num)
  {
    /* In this case we don't have to iterate over all instances, just over the references. */
    gather_attribute_propagation_components(
        geometry, component_type, options, 0, std::nullopt, components);
  }
  else {
    gather_attribute_propagation_components_with_custom_depths(
        geometry, component_type, options, varied_depth_option, components);
  }

  /* Actually gather the attributes to propagate from the found components. */
  bke::GeometrySet::GatheredAttributes attributes_to_propagate;
  for (const bke::GeometryComponentPtr &component : components) {
    const bke::AttributeAccessor attributes = *component->attributes();
    attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
      if (iter.is_builtin) {
        if (!bke::attribute_is_builtin_on_component_type(component_type, iter.name)) {
          /* Don't propagate built-in attributes that are not built-in on the
           * destination component. */
          return;
        }
      }
      if (component->type() == bke::GeometryComponent::Type::Instance) {
        if (ELEM(iter.name, "instance_transform", ".reference_index")) {
          /* These attributes reference potentially temporary instance components in the set above.
           * If we added these names, the string references in the result map would outlive the
           * attributes they reference. */
          return;
        }
      }
      if (iter.data_type == bke::AttrType::String) {
        /* Propagating string attributes is not supported yet. */
        return;
      }
      if (attribute_filter.allow_skip(iter.name)) {
        return;
      }
      AttrDomain dst_domain = iter.domain;
      if (component_type != bke::GeometryComponent::Type::Instance &&
          dst_domain == AttrDomain::Instance)
      {
        if (component_type == bke::GeometryComponent::Type::GreasePencil) {
          /* For Grease Pencil, we want to propagate the instance attributes to the layers. */
          dst_domain = AttrDomain::Layer;
        }
        else {
          /* Other instance attributes are realized on the point domain currently. */
          dst_domain = AttrDomain::Point;
        }
      }
      attributes_to_propagate.add(iter.name, AttributeDomainAndType{dst_domain, iter.data_type});
    });
  }

  return attributes_to_propagate;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Instance
 * \{ */

static OrderedAttributes gather_generic_instance_attributes_to_propagate(
    const bke::GeometrySet &in_geometry_set,
    const RealizeInstancesOptions &options,
    const VariedDepthOptions &varied_depth_option)
{
  bke::GeometrySet::GatheredAttributes attributes_to_propagate = gather_attributes_to_propagate(
      in_geometry_set, bke::GeometryComponent::Type::Instance, options, varied_depth_option);
  OrderedAttributes ordered_attributes;
  for (const int i : attributes_to_propagate.names.index_range()) {
    if (attributes_to_propagate.names[i] == "id") {
      continue;
    }
    ordered_attributes.ids.add_new(attributes_to_propagate.names[i]);
    ordered_attributes.kinds.append(attributes_to_propagate.kinds[i]);
  }
  return ordered_attributes;
}

static void execute_instances_tasks(
    const Span<bke::GeometryComponentPtr> src_components,
    const Span<blender::float4x4> src_base_transforms,
    const OrderedAttributes &all_instances_attributes,
    const Span<blender::geometry::AttributeFallbacksArray> attribute_fallback,
    bke::GeometrySet &r_realized_geometry)
{
  BLI_assert(src_components.size() == src_base_transforms.size() &&
             src_components.size() == attribute_fallback.size());
  if (src_components.is_empty()) {
    return;
  }

  Array<int> offsets_data(src_components.size() + 1);
  for (const int component_index : src_components.index_range()) {
    const bke::InstancesComponent &src_component = static_cast<const bke::InstancesComponent &>(
        *src_components[component_index]);
    offsets_data[component_index] = src_component.get()->instances_num();
  }
  const OffsetIndices offsets = offset_indices::accumulate_counts_to_offsets(offsets_data);

  std::unique_ptr<bke::Instances> dst_instances = std::make_unique<bke::Instances>();
  dst_instances->resize(offsets.total_size());

  /* Makes sure generic output attributes exists. */
  for (const int attribute_index : all_instances_attributes.index_range()) {
    const bke::AttrDomain domain = bke::AttrDomain::Instance;
    const StringRef id = all_instances_attributes.ids[attribute_index];
    const bke::AttrType type = all_instances_attributes.kinds[attribute_index].data_type;
    dst_instances->attributes_for_write()
        .lookup_or_add_for_write_only_span(id, domain, type)
        .finish();
  }

  MutableSpan<float4x4> all_transforms = dst_instances->transforms_for_write();
  MutableSpan<int> all_handles = dst_instances->reference_handles_for_write();

  for (const int component_index : src_components.index_range()) {
    const bke::InstancesComponent &src_component = static_cast<const bke::InstancesComponent &>(
        *src_components[component_index]);
    const bke::Instances &src_instances = *src_component.get();
    const blender::float4x4 &src_base_transform = src_base_transforms[component_index];
    const Span<const void *> attribute_fallback_array = attribute_fallback[component_index].array;
    const Span<bke::InstanceReference> src_references = src_instances.references();
    Array<int> handle_map(src_references.size());

    for (const int src_handle : src_references.index_range()) {
      handle_map[src_handle] = dst_instances->add_reference(src_references[src_handle]);
    }
    const IndexRange dst_range = offsets[component_index];
    for (const int attribute_index : all_instances_attributes.index_range()) {
      const StringRef id = all_instances_attributes.ids[attribute_index];
      const bke::AttrType type = all_instances_attributes.kinds[attribute_index].data_type;
      const CPPType &cpp_type = bke::attribute_type_to_cpp_type(type);
      bke::GSpanAttributeWriter write_attribute =
          dst_instances->attributes_for_write().lookup_for_write_span(id);
      GMutableSpan dst_span = write_attribute.span;

      const void *attribute_ptr;
      if (attribute_fallback_array[attribute_index] != nullptr) {
        attribute_ptr = attribute_fallback_array[attribute_index];
      }
      else {
        attribute_ptr = cpp_type.default_value();
      }

      cpp_type.fill_assign_n(attribute_ptr, dst_span.slice(dst_range).data(), dst_range.size());
      write_attribute.finish();
    }

    const Span<int> src_handles = src_instances.reference_handles();
    array_utils::gather(handle_map.as_span(), src_handles, all_handles.slice(dst_range));
    array_utils::copy(src_instances.transforms(), all_transforms.slice(dst_range));

    for (blender::float4x4 &transform : all_transforms.slice(dst_range)) {
      transform = src_base_transform * transform;
    }
  }

  r_realized_geometry.replace_instances(dst_instances.release());
  auto &dst_component = r_realized_geometry.get_component_for_write<bke::InstancesComponent>();

  Vector<const bke::GeometryComponent *> for_join_attributes;
  for (bke::GeometryComponentPtr component : src_components) {
    for_join_attributes.append(component.get());
  }
  /* Join attribute values from the 'unselected' instances, as they aren't included otherwise.
   * Omit instance_transform and .reference_index to prevent them from overwriting the correct
   * attributes of the realized instances. */
  join_attributes(for_join_attributes, dst_component, {".reference_index", "instance_transform"});
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Point Cloud
 * \{ */

static OrderedAttributes gather_generic_pointcloud_attributes_to_propagate(
    const bke::GeometrySet &in_geometry_set,
    const RealizeInstancesOptions &options,
    const VariedDepthOptions &varied_depth_option,
    bool &r_create_radii,
    bool &r_create_id)
{
  bke::GeometrySet::GatheredAttributes attributes_to_propagate = gather_attributes_to_propagate(
      in_geometry_set, bke::GeometryComponent::Type::PointCloud, options, varied_depth_option);
  OrderedAttributes ordered_attributes;
  for (const int i : attributes_to_propagate.names.index_range()) {
    if (attributes_to_propagate.names[i] == "position") {
      continue;
    }
    if (attributes_to_propagate.names[i] == "id") {
      r_create_id = true;
      continue;
    }
    if (attributes_to_propagate.names[i] == "radius") {
      r_create_radii = true;
      continue;
    }
    ordered_attributes.ids.add_new(attributes_to_propagate.names[i]);
    ordered_attributes.kinds.append(attributes_to_propagate.kinds[i]);
  }
  return ordered_attributes;
}

static void gather_pointclouds_to_realize(const bke::GeometrySet &geometry_set,
                                          VectorSet<const PointCloud *> &r_pointclouds)
{
  if (const PointCloud *pointcloud = geometry_set.get_pointcloud()) {
    if (pointcloud->totpoint > 0) {
      r_pointclouds.add(pointcloud);
    }
  }
  if (const Instances *instances = geometry_set.get_instances()) {
    instances->foreach_referenced_geometry([&](const bke::GeometrySet &instance_geometry_set) {
      gather_pointclouds_to_realize(instance_geometry_set, r_pointclouds);
    });
  }
}

static AllPointCloudsInfo preprocess_pointclouds(const bke::GeometrySet &geometry_set,
                                                 const RealizeInstancesOptions &options,
                                                 const VariedDepthOptions &varied_depth_option)
{
  AllPointCloudsInfo info;
  info.attributes = gather_generic_pointcloud_attributes_to_propagate(geometry_set,
                                                                      options,
                                                                      varied_depth_option,
                                                                      info.create_radius_attribute,
                                                                      info.create_id_attribute);

  gather_pointclouds_to_realize(geometry_set, info.order);
  info.realize_info.reinitialize(info.order.size());
  for (const int pointcloud_index : info.realize_info.index_range()) {
    PointCloudRealizeInfo &pointcloud_info = info.realize_info[pointcloud_index];
    const PointCloud *pointcloud = info.order[pointcloud_index];
    pointcloud_info.pointcloud = pointcloud;

    /* Access attributes. */
    bke::AttributeAccessor attributes = pointcloud->attributes();
    pointcloud_info.attributes.reinitialize(info.attributes.size());
    for (const int attribute_index : info.attributes.index_range()) {
      const StringRef attribute_id = info.attributes.ids[attribute_index];
      const bke::AttrType data_type = info.attributes.kinds[attribute_index].data_type;
      const bke::AttrDomain domain = info.attributes.kinds[attribute_index].domain;
      if (attributes.contains(attribute_id)) {
        GVArray attribute = *attributes.lookup_or_default(attribute_id, domain, data_type);
        pointcloud_info.attributes[attribute_index].emplace(std::move(attribute));
      }
    }
    if (info.create_id_attribute) {
      bke::GAttributeReader ids_attribute = attributes.lookup("id");
      if (ids_attribute && ids_attribute.domain == bke::AttrDomain::Point &&
          ids_attribute.varray.type().is<int>() && ids_attribute.varray.is_span())
      {
        pointcloud_info.stored_ids = ids_attribute.varray.get_internal_span().typed<int>();
      }
    }
    if (info.create_radius_attribute) {
      pointcloud_info.radii = *attributes.lookup_or_default(
          "radius", bke::AttrDomain::Point, 0.01f);
    }
    const VArray<float3> position_attribute = *attributes.lookup_or_default<float3>(
        "position", bke::AttrDomain::Point, float3(0));
    pointcloud_info.positions = position_attribute.get_internal_span();
  }
  return info;
}

static void execute_realize_pointcloud_task(
    const RealizeInstancesOptions &options,
    const RealizePointCloudTask &task,
    const OrderedAttributes &ordered_attributes,
    MutableSpan<GSpanAttributeWriter> dst_attribute_writers,
    MutableSpan<float> all_dst_radii,
    MutableSpan<int> all_dst_ids,
    MutableSpan<float3> all_dst_positions)
{
  const PointCloudRealizeInfo &pointcloud_info = *task.pointcloud_info;
  const PointCloud &pointcloud = *pointcloud_info.pointcloud;
  const IndexRange point_slice{task.start_index, pointcloud.totpoint};

  math::transform_points(
      pointcloud_info.positions, task.transform, all_dst_positions.slice(point_slice));

  /* Create point ids. */
  if (!all_dst_ids.is_empty()) {
    create_result_ids(
        options, pointcloud_info.stored_ids, task.id, all_dst_ids.slice(point_slice));
  }
  if (!all_dst_radii.is_empty()) {
    pointcloud_info.radii.materialize(all_dst_radii.slice(point_slice));
  }

  copy_generic_attributes_to_result(
      pointcloud_info.attributes,
      task.attribute_fallbacks,
      ordered_attributes,
      [&](const bke::AttrDomain domain) {
        BLI_assert(domain == bke::AttrDomain::Point);
        UNUSED_VARS_NDEBUG(domain);
        return point_slice;
      },
      dst_attribute_writers);
}

static void add_instance_attributes_to_single_geometry(
    const OrderedAttributes &ordered_attributes,
    const AttributeFallbacksArray &attribute_fallbacks,
    bke::MutableAttributeAccessor attributes)
{
  for (const int attribute_index : ordered_attributes.index_range()) {
    const void *value = attribute_fallbacks.array[attribute_index];
    if (!value) {
      continue;
    }
    const bke::AttrDomain domain = ordered_attributes.kinds[attribute_index].domain;
    const bke::AttrType data_type = ordered_attributes.kinds[attribute_index].data_type;
    const CPPType &cpp_type = bke::attribute_type_to_cpp_type(data_type);
    GVArray gvaray(GVArray::from_single(cpp_type, attributes.domain_size(domain), value));
    attributes.add(ordered_attributes.ids[attribute_index],
                   domain,
                   data_type,
                   bke::AttributeInitVArray(std::move(gvaray)));
  }
}
static void execute_realize_pointcloud_tasks(const RealizeInstancesOptions &options,
                                             const GatherOffsets &offsets,
                                             const AllPointCloudsInfo &all_pointclouds_info,
                                             const Span<RealizePointCloudTask> tasks,
                                             const OrderedAttributes &ordered_attributes,
                                             RealizeInstancesResult &r_result)
{
  if (tasks.is_empty()) {
    return;
  }

  if (tasks.size() == 1) {
    const RealizePointCloudTask &task = tasks.first();
    PointCloud *new_points = BKE_pointcloud_copy_for_eval(task.pointcloud_info->pointcloud);
    if (!skip_transform(task.transform)) {
      math::transform_points(task.transform, new_points->positions_for_write());
      new_points->tag_positions_changed();
    }
    add_instance_attributes_to_single_geometry(
        ordered_attributes, task.attribute_fallbacks, new_points->attributes_for_write());
    r_result.geometry.replace_pointcloud(new_points);
    return;
  }

  const int64_t tot_points = offsets.pointcloud_offset;
  if (!valid_int_num(tot_points)) {
    r_result.errors.append(RPT_("Realized point cloud has too many points."));
    return;
  }

  /* Allocate new point cloud. */
  PointCloud *dst_pointcloud = BKE_pointcloud_new_nomain(tot_points);
  r_result.geometry.replace_pointcloud(dst_pointcloud);
  bke::MutableAttributeAccessor dst_attributes = dst_pointcloud->attributes_for_write();

  const RealizePointCloudTask &first_task = tasks.first();
  const PointCloud &first_pointcloud = *first_task.pointcloud_info->pointcloud;
  dst_pointcloud->mat = static_cast<Material **>(MEM_dupallocN(first_pointcloud.mat));
  dst_pointcloud->totcol = first_pointcloud.totcol;

  SpanAttributeWriter<float3> positions = dst_attributes.lookup_or_add_for_write_only_span<float3>(
      "position", bke::AttrDomain::Point);

  /* Prepare id attribute. */
  SpanAttributeWriter<int> point_ids;
  if (all_pointclouds_info.create_id_attribute) {
    point_ids = dst_attributes.lookup_or_add_for_write_only_span<int>("id",
                                                                      bke::AttrDomain::Point);
  }
  SpanAttributeWriter<float> point_radii;
  if (all_pointclouds_info.create_radius_attribute) {
    point_radii = dst_attributes.lookup_or_add_for_write_only_span<float>("radius",
                                                                          bke::AttrDomain::Point);
  }

  /* Prepare generic output attributes. */
  Vector<GSpanAttributeWriter> dst_attribute_writers;
  for (const int attribute_index : ordered_attributes.index_range()) {
    const StringRef attribute_id = ordered_attributes.ids[attribute_index];
    const bke::AttrType data_type = ordered_attributes.kinds[attribute_index].data_type;
    dst_attribute_writers.append(dst_attributes.lookup_or_add_for_write_only_span(
        attribute_id, bke::AttrDomain::Point, data_type));
  }

  /* Actually execute all tasks. */
  threading::parallel_for(tasks.index_range(), 100, [&](const IndexRange task_range) {
    for (const int task_index : task_range) {
      const RealizePointCloudTask &task = tasks[task_index];
      execute_realize_pointcloud_task(options,
                                      task,
                                      ordered_attributes,
                                      dst_attribute_writers,
                                      point_radii.span,
                                      point_ids.span,
                                      positions.span);
    }
  });

  /* Tag modified attributes. */
  for (GSpanAttributeWriter &dst_attribute : dst_attribute_writers) {
    dst_attribute.finish();
  }
  positions.finish();
  point_radii.finish();
  point_ids.finish();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh
 * \{ */

static OrderedAttributes gather_generic_mesh_attributes_to_propagate(
    const bke::GeometrySet &in_geometry_set,
    const RealizeInstancesOptions &options,
    const VariedDepthOptions &varied_depth_option,
    bool &r_create_id,
    bool &r_create_material_index)
{
  bke::GeometrySet::GatheredAttributes attributes_to_propagate = gather_attributes_to_propagate(
      in_geometry_set, bke::GeometryComponent::Type::Mesh, options, varied_depth_option);
  OrderedAttributes ordered_attributes;
  for (const int i : attributes_to_propagate.names.index_range()) {
    if (ELEM(attributes_to_propagate.names[i],
             "position",
             ".edge_verts",
             ".corner_vert",
             ".corner_edge",
             "custom_normal"))
    {
      continue;
    }
    if (attributes_to_propagate.names[i] == "id") {
      r_create_id = true;
      continue;
    }
    if (attributes_to_propagate.names[i] == "material_index") {
      r_create_material_index = true;
      continue;
    }
    ordered_attributes.ids.add_new(attributes_to_propagate.names[i]);
    ordered_attributes.kinds.append(attributes_to_propagate.kinds[i]);
  }
  return ordered_attributes;
}

static void gather_meshes_to_realize(const bke::GeometrySet &geometry_set,
                                     VectorSet<const Mesh *> &r_meshes)
{
  if (const Mesh *mesh = geometry_set.get_mesh()) {
    if (mesh->verts_num > 0) {
      r_meshes.add(mesh);
    }
  }
  if (const Instances *instances = geometry_set.get_instances()) {
    instances->foreach_referenced_geometry([&](const bke::GeometrySet &instance_geometry_set) {
      gather_meshes_to_realize(instance_geometry_set, r_meshes);
    });
  }
}

static AllMeshesInfo preprocess_meshes(const bke::GeometrySet &geometry_set,
                                       const RealizeInstancesOptions &options,
                                       const VariedDepthOptions &varied_depth_option)
{
  AllMeshesInfo info;
  info.attributes = gather_generic_mesh_attributes_to_propagate(
      geometry_set,
      options,
      varied_depth_option,
      info.create_id_attribute,
      info.create_material_index_attribute);

  gather_meshes_to_realize(geometry_set, info.order);
  for (const Mesh *mesh : info.order) {
    if (mesh->totcol == 0) {
      /* Add an empty material slot for the default material. */
      info.materials.add(nullptr);
    }
    else {
      for (const int slot_index : IndexRange(mesh->totcol)) {
        Material *material = mesh->mat[slot_index];
        info.materials.add(material);
      }
    }
  }

  for (const Mesh *mesh : info.order) {
    info.custom_normal_info.add_mesh(*mesh);
  }

  info.create_material_index_attribute |= info.materials.size() > 1;
  info.realize_info.reinitialize(info.order.size());
  for (const int mesh_index : info.realize_info.index_range()) {
    MeshRealizeInfo &mesh_info = info.realize_info[mesh_index];
    const Mesh *mesh = info.order[mesh_index];
    mesh_info.mesh = mesh;
    mesh_info.positions = mesh->vert_positions();
    mesh_info.edges = mesh->edges();
    mesh_info.faces = mesh->faces();
    mesh_info.corner_verts = mesh->corner_verts();
    mesh_info.corner_edges = mesh->corner_edges();

    /* Create material index mapping. */
    mesh_info.material_index_map.reinitialize(std::max<int>(mesh->totcol, 1));
    if (mesh->totcol == 0) {
      mesh_info.material_index_map.first() = info.materials.index_of(nullptr);
    }
    else {
      for (const int old_slot_index : IndexRange(mesh->totcol)) {
        Material *material = mesh->mat[old_slot_index];
        const int new_slot_index = info.materials.index_of(material);
        mesh_info.material_index_map[old_slot_index] = new_slot_index;
      }
    }

    /* Access attributes. */
    bke::AttributeAccessor attributes = mesh->attributes();
    mesh_info.attributes.reinitialize(info.attributes.size());
    for (const int attribute_index : info.attributes.index_range()) {
      const StringRef attribute_id = info.attributes.ids[attribute_index];
      const bke::AttrType data_type = info.attributes.kinds[attribute_index].data_type;
      const bke::AttrDomain domain = info.attributes.kinds[attribute_index].domain;
      if (attributes.contains(attribute_id)) {
        GVArray attribute = *attributes.lookup_or_default(attribute_id, domain, data_type);
        mesh_info.attributes[attribute_index].emplace(std::move(attribute));
      }
    }
    if (info.create_id_attribute) {
      bke::GAttributeReader ids_attribute = attributes.lookup("id");
      if (ids_attribute && ids_attribute.domain == bke::AttrDomain::Point &&
          ids_attribute.varray.type().is<int>() && ids_attribute.varray.is_span())
      {
        mesh_info.stored_vertex_ids = ids_attribute.varray.get_internal_span().typed<int>();
      }
    }
    mesh_info.material_indices = *attributes.lookup_or_default<int>(
        "material_index", bke::AttrDomain::Face, 0);

    switch (info.custom_normal_info.result_type) {
      case bke::mesh::NormalJoinInfo::Output::None: {
        break;
      }
      case bke::mesh::NormalJoinInfo::Output::CornerFan: {
        if (const bke::GAttributeReader custom_normal = attributes.lookup("custom_normal")) {
          const bke::AttributeMetaData meta_data{
              custom_normal.domain, bke::cpp_type_to_attribute_type(custom_normal.varray.type())};
          if (bke::mesh::is_corner_fan_normals(meta_data)) {
            mesh_info.custom_normal = custom_normal.varray.typed<short2>();
          }
        }
        break;
      }
      case bke::mesh::NormalJoinInfo::Output::Free: {
        switch (*info.custom_normal_info.result_domain) {
          case bke::AttrDomain::Point:
            mesh_info.custom_normal = VArray<float3>::from_span(mesh->vert_normals());
            break;
          case bke::AttrDomain::Face:
            mesh_info.custom_normal = VArray<float3>::from_span(mesh->face_normals());
            break;
          case bke::AttrDomain::Corner:
            mesh_info.custom_normal = VArray<float3>::from_span(mesh->corner_normals());
            break;
          default:
            BLI_assert_unreachable();
        }
        break;
      }
    }
  }

  info.no_loose_edges_hint = std::all_of(
      info.order.begin(), info.order.end(), [](const Mesh *mesh) {
        return mesh->runtime->loose_edges_cache.is_cached() && mesh->loose_edges().count == 0;
      });
  info.no_loose_verts_hint = std::all_of(
      info.order.begin(), info.order.end(), [](const Mesh *mesh) {
        return mesh->runtime->loose_verts_cache.is_cached() && mesh->loose_verts().count == 0;
      });
  info.no_overlapping_hint = std::all_of(
      info.order.begin(), info.order.end(), [](const Mesh *mesh) {
        return mesh->no_overlapping_topology();
      });

  return info;
}

static void execute_realize_mesh_task(const RealizeInstancesOptions &options,
                                      const RealizeMeshTask &task,
                                      const OrderedAttributes &ordered_attributes,
                                      MutableSpan<GSpanAttributeWriter> dst_attribute_writers,
                                      MutableSpan<float3> all_dst_positions,
                                      MutableSpan<int2> all_dst_edges,
                                      MutableSpan<int> all_dst_face_offsets,
                                      MutableSpan<int> all_dst_corner_verts,
                                      MutableSpan<int> all_dst_corner_edges,
                                      MutableSpan<int> all_dst_vertex_ids,
                                      MutableSpan<int> all_dst_material_indices,
                                      GSpanAttributeWriter &all_dst_custom_normals)
{
  const MeshRealizeInfo &mesh_info = *task.mesh_info;
  const Mesh &mesh = *mesh_info.mesh;

  const Span<float3> src_positions = mesh_info.positions;
  const Span<int2> src_edges = mesh_info.edges;
  const OffsetIndices src_faces = mesh_info.faces;
  const Span<int> src_corner_verts = mesh_info.corner_verts;
  const Span<int> src_corner_edges = mesh_info.corner_edges;

  const IndexRange dst_vert_range(task.start_indices.vertex, src_positions.size());
  const IndexRange dst_edge_range(task.start_indices.edge, src_edges.size());
  const IndexRange dst_face_range(task.start_indices.face, src_faces.size());
  const IndexRange dst_loop_range(task.start_indices.loop, src_corner_verts.size());

  MutableSpan<float3> dst_positions = all_dst_positions.slice(dst_vert_range);
  MutableSpan<int2> dst_edges = all_dst_edges.slice(dst_edge_range);
  MutableSpan<int> dst_face_offsets = all_dst_face_offsets.slice(dst_face_range);
  MutableSpan<int> dst_corner_verts = all_dst_corner_verts.slice(dst_loop_range);
  MutableSpan<int> dst_corner_edges = all_dst_corner_edges.slice(dst_loop_range);

  math::transform_points(src_positions, task.transform, dst_positions);

  threading::parallel_for(src_edges.index_range(), 1024, [&](const IndexRange edge_range) {
    for (const int i : edge_range) {
      dst_edges[i] = src_edges[i] + task.start_indices.vertex;
    }
  });
  threading::parallel_for(src_corner_verts.index_range(), 1024, [&](const IndexRange loop_range) {
    for (const int i : loop_range) {
      dst_corner_verts[i] = src_corner_verts[i] + task.start_indices.vertex;
    }
  });
  threading::parallel_for(src_corner_edges.index_range(), 1024, [&](const IndexRange loop_range) {
    for (const int i : loop_range) {
      dst_corner_edges[i] = src_corner_edges[i] + task.start_indices.edge;
    }
  });
  threading::parallel_for(src_faces.index_range(), 1024, [&](const IndexRange face_range) {
    for (const int i : face_range) {
      dst_face_offsets[i] = src_faces[i].start() + task.start_indices.loop;
    }
  });
  if (!all_dst_material_indices.is_empty()) {
    const Span<int> material_index_map = mesh_info.material_index_map;
    MutableSpan<int> dst_material_indices = all_dst_material_indices.slice(dst_face_range);
    if (mesh.totcol == 0) {
      /* The material index map contains the index of the null material in the result. */
      dst_material_indices.fill(material_index_map.first());
    }
    else {
      if (mesh_info.material_indices.is_single()) {
        const int src_index = mesh_info.material_indices.get_internal_single();
        const bool valid = IndexRange(mesh.totcol).contains(src_index);
        dst_material_indices.fill(valid ? material_index_map[src_index] : 0);
      }
      else {
        VArraySpan<int> indices_span(mesh_info.material_indices);
        threading::parallel_for(src_faces.index_range(), 1024, [&](const IndexRange face_range) {
          for (const int i : face_range) {
            const int src_index = indices_span[i];
            const bool valid = IndexRange(mesh.totcol).contains(src_index);
            dst_material_indices[i] = valid ? material_index_map[src_index] : 0;
          }
        });
      }
    }
  }

  if (!all_dst_vertex_ids.is_empty()) {
    create_result_ids(options,
                      mesh_info.stored_vertex_ids,
                      task.id,
                      all_dst_vertex_ids.slice(task.start_indices.vertex, mesh.verts_num));
  }

  const auto domain_to_range = [&](const bke::AttrDomain domain) {
    switch (domain) {
      case bke::AttrDomain::Point:
        return dst_vert_range;
      case bke::AttrDomain::Edge:
        return dst_edge_range;
      case bke::AttrDomain::Face:
        return dst_face_range;
      case bke::AttrDomain::Corner:
        return dst_loop_range;
      default:
        BLI_assert_unreachable();
        return IndexRange();
    }
  };

  if (all_dst_custom_normals) {
    if (all_dst_custom_normals.span.type().is<short2>()) {
      if (mesh_info.custom_normal.is_empty()) {
        all_dst_custom_normals.span.typed<short2>().slice(dst_loop_range).fill(short2(0));
      }
      else {
        all_dst_custom_normals.span.typed<short2>()
            .slice(dst_loop_range)
            .copy_from(mesh_info.custom_normal.typed<short2>());
      }
    }
    else {
      const IndexRange dst_range = domain_to_range(all_dst_custom_normals.domain);
      math::transform_normals(mesh_info.custom_normal.typed<float3>(),
                              float3x3(task.transform),
                              all_dst_custom_normals.span.typed<float3>().slice(dst_range));
    }
  }

  copy_generic_attributes_to_result(mesh_info.attributes,
                                    task.attribute_fallbacks,
                                    ordered_attributes,
                                    domain_to_range,
                                    dst_attribute_writers);
}
static void copy_vertex_group_name(ListBase *dst_deform_group,
                                   const OrderedAttributes &ordered_attributes,
                                   const bDeformGroup &src_deform_group)
{
  const StringRef src_name = src_deform_group.name;
  const int attribute_index = ordered_attributes.ids.index_of_try(src_name);
  if (attribute_index == -1) {
    /* The attribute is not propagated to the result (possibly because the mesh isn't included
     * in the realized output because of the #VariedDepthOptions input). */
    return;
  }
  const bke::AttributeDomainAndType kind = ordered_attributes.kinds[attribute_index];
  if (kind.domain != bke::AttrDomain::Point || kind.data_type != bke::AttrType::Float) {
    /* Skip if the source attribute can't possibly contain vertex weights. */
    return;
  }
  bDeformGroup *dst = MEM_callocN<bDeformGroup>(__func__);
  src_name.copy_utf8_truncated(dst->name);
  BLI_addtail(dst_deform_group, dst);
}

static void copy_vertex_group_names(Mesh &dst_mesh,
                                    const OrderedAttributes &ordered_attributes,
                                    const Span<const Mesh *> src_meshes)
{
  Set<StringRef> existing_names;
  LISTBASE_FOREACH (const bDeformGroup *, defgroup, &dst_mesh.vertex_group_names) {
    existing_names.add(defgroup->name);
  }
  for (const Mesh *mesh : src_meshes) {
    LISTBASE_FOREACH (const bDeformGroup *, src, &mesh->vertex_group_names) {
      if (existing_names.contains(src->name)) {
        continue;
      }
      copy_vertex_group_name(&dst_mesh.vertex_group_names, ordered_attributes, *src);
    }
  }
}

static void execute_realize_mesh_tasks(const RealizeInstancesOptions &options,
                                       const GatherOffsets &offsets,
                                       const AllMeshesInfo &all_meshes_info,
                                       const Span<RealizeMeshTask> tasks,
                                       const OrderedAttributes &ordered_attributes,
                                       const VectorSet<Material *> &ordered_materials,
                                       RealizeInstancesResult &r_result)
{
  if (tasks.is_empty()) {
    return;
  }

  if (tasks.size() == 1) {
    const RealizeMeshTask &task = tasks.first();
    Mesh *new_mesh = BKE_mesh_copy_for_eval(*task.mesh_info->mesh);
    if (!skip_transform(task.transform)) {
      bke::mesh_transform(*new_mesh, task.transform, false);
    }
    add_instance_attributes_to_single_geometry(
        ordered_attributes, task.attribute_fallbacks, new_mesh->attributes_for_write());
    r_result.geometry.replace_mesh(new_mesh);
    return;
  }

  const int64_t tot_vertices = offsets.mesh_offsets.vertex;
  const int64_t tot_edges = offsets.mesh_offsets.edge;
  const int64_t tot_loops = offsets.mesh_offsets.corner;
  const int64_t tot_faces = offsets.mesh_offsets.face;

  if (!valid_int_num(tot_vertices) || !valid_int_num(tot_edges) || !valid_int_num(tot_loops) ||
      !valid_int_num(tot_faces))
  {
    r_result.errors.append(RPT_("Realized mesh has too many elements."));
    return;
  }

  Mesh *dst_mesh = BKE_mesh_new_nomain(tot_vertices, tot_edges, tot_faces, tot_loops);
  r_result.geometry.replace_mesh(dst_mesh);
  bke::MutableAttributeAccessor dst_attributes = dst_mesh->attributes_for_write();
  MutableSpan<float3> dst_positions = dst_mesh->vert_positions_for_write();
  MutableSpan<int2> dst_edges = dst_mesh->edges_for_write();
  MutableSpan<int> dst_face_offsets = dst_mesh->face_offsets_for_write();
  MutableSpan<int> dst_corner_verts = dst_mesh->corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = dst_mesh->corner_edges_for_write();

  /* Copy settings from the first input geometry set with a mesh. */
  const RealizeMeshTask &first_task = tasks.first();
  const Mesh &first_mesh = *first_task.mesh_info->mesh;
  BKE_mesh_copy_parameters_for_eval(dst_mesh, &first_mesh);

  BLI_assert(BLI_listbase_count(&dst_mesh->vertex_group_names) ==
             BLI_listbase_count(&first_mesh.vertex_group_names));
  copy_vertex_group_names(
      *dst_mesh, ordered_attributes, all_meshes_info.order.as_span().drop_front(1));
  dst_mesh->vertex_group_active_index = first_mesh.vertex_group_active_index;

  /* Add materials. */
  for (const int i : IndexRange(ordered_materials.size())) {
    Material *material = ordered_materials[i];
    BKE_id_material_eval_assign(&dst_mesh->id, i + 1, material);
  }

  /* Prepare id attribute. */
  SpanAttributeWriter<int> vertex_ids;
  if (all_meshes_info.create_id_attribute) {
    vertex_ids = dst_attributes.lookup_or_add_for_write_only_span<int>("id",
                                                                       bke::AttrDomain::Point);
  }
  /* Prepare material indices. */
  SpanAttributeWriter<int> material_indices;
  if (all_meshes_info.create_material_index_attribute) {
    material_indices = dst_attributes.lookup_or_add_for_write_only_span<int>(
        "material_index", bke::AttrDomain::Face);
  }

  GSpanAttributeWriter custom_normals;
  switch (all_meshes_info.custom_normal_info.result_type) {
    case bke::mesh::NormalJoinInfo::Output::None: {
      break;
    }
    case bke::mesh::NormalJoinInfo::Output::CornerFan: {
      custom_normals = dst_attributes.lookup_or_add_for_write_only_span(
          "custom_normal", bke::AttrDomain::Corner, bke::AttrType::Int16_2D);
      break;
    }
    case bke::mesh::NormalJoinInfo::Output::Free: {
      const bke::AttrDomain domain = *all_meshes_info.custom_normal_info.result_domain;
      custom_normals = dst_attributes.lookup_or_add_for_write_only_span(
          "custom_normal", domain, bke::AttrType::Float3);
      break;
    }
  }

  /* Prepare generic output attributes. */
  Vector<GSpanAttributeWriter> dst_attribute_writers;
  for (const int attribute_index : ordered_attributes.index_range()) {
    const StringRef attribute_id = ordered_attributes.ids[attribute_index];
    const bke::AttrDomain domain = ordered_attributes.kinds[attribute_index].domain;
    const bke::AttrType data_type = ordered_attributes.kinds[attribute_index].data_type;
    dst_attribute_writers.append(
        dst_attributes.lookup_or_add_for_write_only_span(attribute_id, domain, data_type));
  }
  const char *active_layer = CustomData_get_active_layer_name(&first_mesh.corner_data,
                                                              CD_PROP_FLOAT2);
  if (active_layer != nullptr) {
    int id = CustomData_get_named_layer(&dst_mesh->corner_data, CD_PROP_FLOAT2, active_layer);
    if (id >= 0) {
      CustomData_set_layer_active(&dst_mesh->corner_data, CD_PROP_FLOAT2, id);
    }
  }
  const char *render_layer = CustomData_get_render_layer_name(&first_mesh.corner_data,
                                                              CD_PROP_FLOAT2);
  if (render_layer != nullptr) {
    int id = CustomData_get_named_layer(&dst_mesh->corner_data, CD_PROP_FLOAT2, render_layer);
    if (id >= 0) {
      CustomData_set_layer_render(&dst_mesh->corner_data, CD_PROP_FLOAT2, id);
    }
  }
  /* Actually execute all tasks. */
  threading::parallel_for(tasks.index_range(), 100, [&](const IndexRange task_range) {
    for (const int task_index : task_range) {
      const RealizeMeshTask &task = tasks[task_index];
      execute_realize_mesh_task(options,
                                task,
                                ordered_attributes,
                                dst_attribute_writers,
                                dst_positions,
                                dst_edges,
                                dst_face_offsets,
                                dst_corner_verts,
                                dst_corner_edges,
                                vertex_ids.span,
                                material_indices.span,
                                custom_normals);
    }
  });

  /* Tag modified attributes. */
  for (GSpanAttributeWriter &dst_attribute : dst_attribute_writers) {
    dst_attribute.finish();
  }
  vertex_ids.finish();
  material_indices.finish();
  custom_normals.finish();

  if (all_meshes_info.no_loose_edges_hint) {
    dst_mesh->tag_loose_edges_none();
  }
  if (all_meshes_info.no_loose_verts_hint) {
    dst_mesh->tag_loose_verts_none();
  }
  if (all_meshes_info.no_overlapping_hint) {
    dst_mesh->tag_overlapping_none();
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curves
 * \{ */

static OrderedAttributes gather_generic_curve_attributes_to_propagate(
    const bke::GeometrySet &in_geometry_set,
    const RealizeInstancesOptions &options,
    const VariedDepthOptions &varied_depth_option,
    bool &r_create_id)
{
  bke::GeometrySet::GatheredAttributes attributes_to_propagate = gather_attributes_to_propagate(
      in_geometry_set, bke::GeometryComponent::Type::Curve, options, varied_depth_option);
  OrderedAttributes ordered_attributes;
  for (const int i : attributes_to_propagate.names.index_range()) {
    if (ELEM(attributes_to_propagate.names[i],
             "position",
             "radius",
             "handle_left",
             "handle_right",
             "custom_normal"))
    {
      continue;
    }
    if (attributes_to_propagate.names[i] == "id") {
      r_create_id = true;
      continue;
    }
    ordered_attributes.ids.add_new(attributes_to_propagate.names[i]);
    ordered_attributes.kinds.append(attributes_to_propagate.kinds[i]);
  }
  return ordered_attributes;
}

static void gather_curves_to_realize(const bke::GeometrySet &geometry_set,
                                     VectorSet<const Curves *> &r_curves)
{
  if (const Curves *curves = geometry_set.get_curves()) {
    if (curves->geometry.curve_num != 0) {
      r_curves.add(curves);
    }
  }
  if (const Instances *instances = geometry_set.get_instances()) {
    instances->foreach_referenced_geometry([&](const bke::GeometrySet &instance_geometry_set) {
      gather_curves_to_realize(instance_geometry_set, r_curves);
    });
  }
}

static AllCurvesInfo preprocess_curves(const bke::GeometrySet &geometry_set,
                                       const RealizeInstancesOptions &options,
                                       const VariedDepthOptions &varied_depth_option)
{
  AllCurvesInfo info;
  info.attributes = gather_generic_curve_attributes_to_propagate(
      geometry_set, options, varied_depth_option, info.create_id_attribute);

  gather_curves_to_realize(geometry_set, info.order);
  info.realize_info.reinitialize(info.order.size());
  for (const int curve_index : info.realize_info.index_range()) {
    RealizeCurveInfo &curve_info = info.realize_info[curve_index];
    const Curves *curves_id = info.order[curve_index];
    const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    curve_info.curves = curves_id;

    /* Access attributes. */
    bke::AttributeAccessor attributes = curves.attributes();
    curve_info.attributes.reinitialize(info.attributes.size());
    for (const int attribute_index : info.attributes.index_range()) {
      const bke::AttrDomain domain = info.attributes.kinds[attribute_index].domain;
      const StringRef attribute_id = info.attributes.ids[attribute_index];
      const bke::AttrType data_type = info.attributes.kinds[attribute_index].data_type;
      if (attributes.contains(attribute_id)) {
        GVArray attribute = *attributes.lookup_or_default(attribute_id, domain, data_type);
        curve_info.attributes[attribute_index].emplace(std::move(attribute));
      }
    }
    if (info.create_id_attribute) {
      bke::GAttributeReader id_attribute = attributes.lookup("id");
      if (id_attribute && id_attribute.domain == bke::AttrDomain::Point &&
          id_attribute.varray.type().is<int>() && id_attribute.varray.is_span())
      {
        curve_info.stored_ids = id_attribute.varray.get_internal_span().typed<int>();
      }
    }

    if (attributes.contains("radius")) {
      curve_info.radius =
          attributes.lookup<float>("radius", bke::AttrDomain::Point).varray.get_internal_span();
      info.create_radius_attribute = true;
    }
    if (attributes.contains("handle_right")) {
      curve_info.handle_left = attributes.lookup<float3>("handle_left", bke::AttrDomain::Point)
                                   .varray.get_internal_span();
      curve_info.handle_right = attributes.lookup<float3>("handle_right", bke::AttrDomain::Point)
                                    .varray.get_internal_span();
      info.create_handle_postion_attributes = true;
    }
    if (attributes.contains("custom_normal")) {
      curve_info.custom_normal = attributes.lookup<float3>("custom_normal", bke::AttrDomain::Point)
                                     .varray.get_internal_span();
      info.create_custom_normal_attribute = true;
    }
  }
  return info;
}

static void initialize_curves_builtin_attribute_defaults(const AllCurvesInfo &all_curves_info,
                                                         InstanceContext &attribute_fallbacks)
{
  if (all_curves_info.order.is_empty()) {
    return;
  }
  const Curves *first = all_curves_info.order[0];
  const bke::CurvesGeometry &first_curves = first->geometry.wrap();
  for (const int attribute_i : attribute_fallbacks.curves.array.index_range()) {
    const StringRef attribute_id = all_curves_info.attributes.ids[attribute_i];
    if (first_curves.attributes().is_builtin(attribute_id)) {
      attribute_fallbacks.curves.array[attribute_i] =
          first_curves.attributes().get_builtin_default(attribute_id).get();
    }
  }
}

static void execute_realize_curve_task(const RealizeInstancesOptions &options,
                                       const AllCurvesInfo &all_curves_info,
                                       const RealizeCurveTask &task,
                                       const OrderedAttributes &ordered_attributes,
                                       bke::CurvesGeometry &dst_curves,
                                       MutableSpan<GSpanAttributeWriter> dst_attribute_writers,
                                       MutableSpan<int> all_dst_ids,
                                       MutableSpan<float3> all_handle_left,
                                       MutableSpan<float3> all_handle_right,
                                       MutableSpan<float> all_radii,
                                       MutableSpan<float3> all_custom_normals)
{
  const RealizeCurveInfo &curves_info = *task.curve_info;
  const Curves &curves_id = *curves_info.curves;
  const bke::CurvesGeometry &curves = curves_id.geometry.wrap();

  const IndexRange dst_point_range{task.start_indices.point, curves.points_num()};
  const IndexRange dst_curve_range{task.start_indices.curve, curves.curves_num()};
  const IndexRange dst_custom_knot_range{task.start_indices.custom_knot,
                                         curves.nurbs_custom_knots_by_curve().total_size()};

  math::transform_points(
      curves.positions(), task.transform, dst_curves.positions_for_write().slice(dst_point_range));

  /* Copy and transform handle positions if necessary. */
  if (all_curves_info.create_handle_postion_attributes) {
    if (curves_info.handle_left.is_empty()) {
      all_handle_left.slice(dst_point_range).fill(float3(0));
    }
    else {
      math::transform_points(
          curves_info.handle_left, task.transform, all_handle_left.slice(dst_point_range));
    }
    if (curves_info.handle_right.is_empty()) {
      all_handle_right.slice(dst_point_range).fill(float3(0));
    }
    else {
      math::transform_points(
          curves_info.handle_right, task.transform, all_handle_right.slice(dst_point_range));
    }
  }

  if (all_curves_info.create_radius_attribute) {
    if (curves_info.radius.is_empty()) {
      all_radii.slice(dst_point_range).fill(1.0f);
    }
    else {
      all_radii.slice(dst_point_range).copy_from(curves_info.radius);
    }
  }

  if (all_curves_info.create_custom_normal_attribute) {
    if (curves_info.custom_normal.is_empty()) {
      all_custom_normals.slice(dst_point_range).fill(float3(0, 0, 1));
    }
    else {
      math::transform_normals(curves_info.custom_normal,
                              float3x3(task.transform),
                              all_custom_normals.slice(dst_point_range));
    }
  }

  /* Copy curve offsets. */
  const Span<int> src_offsets = curves.offsets();
  const MutableSpan<int> dst_offsets = dst_curves.offsets_for_write().slice(dst_curve_range);
  threading::parallel_for(curves.curves_range(), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      dst_offsets[i] = task.start_indices.point + src_offsets[i];
    }
  });

  dst_curves.nurbs_custom_knots_for_write()
      .slice(dst_custom_knot_range)
      .copy_from(curves.nurbs_custom_knots());

  if (!all_dst_ids.is_empty()) {
    create_result_ids(
        options, curves_info.stored_ids, task.id, all_dst_ids.slice(dst_point_range));
  }

  copy_generic_attributes_to_result(
      curves_info.attributes,
      task.attribute_fallbacks,
      ordered_attributes,
      [&](const bke::AttrDomain domain) {
        switch (domain) {
          case bke::AttrDomain::Point:
            return IndexRange(task.start_indices.point, curves.points_num());
          case bke::AttrDomain::Curve:
            return IndexRange(task.start_indices.curve, curves.curves_num());
          default:
            BLI_assert_unreachable();
            return IndexRange();
        }
      },
      dst_attribute_writers);
}

static void copy_vertex_group_names(CurvesGeometry &dst_curve,
                                    const OrderedAttributes &ordered_attributes,
                                    const Span<const Curves *> src_curves)
{
  Set<StringRef> existing_names;
  LISTBASE_FOREACH (const bDeformGroup *, defgroup, &dst_curve.vertex_group_names) {
    existing_names.add(defgroup->name);
  }
  for (const Curves *src_curve : src_curves) {
    LISTBASE_FOREACH (const bDeformGroup *, src, &src_curve->geometry.vertex_group_names) {
      if (existing_names.contains(src->name)) {
        continue;
      }
      copy_vertex_group_name(&dst_curve.vertex_group_names, ordered_attributes, *src);
      existing_names.add(src->name);
    }
  }
}

static void execute_realize_curve_tasks(const RealizeInstancesOptions &options,
                                        const GatherOffsets &offsets,
                                        const AllCurvesInfo &all_curves_info,
                                        const Span<RealizeCurveTask> tasks,
                                        const OrderedAttributes &ordered_attributes,
                                        RealizeInstancesResult &r_result)
{
  if (tasks.is_empty()) {
    return;
  }

  if (tasks.size() == 1) {
    const RealizeCurveTask &task = tasks.first();
    Curves *new_curves = BKE_curves_copy_for_eval(task.curve_info->curves);
    if (!skip_transform(task.transform)) {
      new_curves->geometry.wrap().transform(task.transform);
    }
    add_instance_attributes_to_single_geometry(ordered_attributes,
                                               task.attribute_fallbacks,
                                               new_curves->geometry.wrap().attributes_for_write());
    r_result.geometry.replace_curves(new_curves);
    return;
  }

  const int64_t points_num = offsets.curves_offsets.point;
  const int64_t curves_num = offsets.curves_offsets.curve;
  const int64_t custom_knot_num = offsets.curves_offsets.custom_knot;

  if (!valid_int_num(points_num) || !valid_int_num(curves_num) || !valid_int_num(custom_knot_num))
  {
    r_result.errors.append(RPT_("Realized curves data has too many elements."));
    return;
  }

  /* Allocate new curves data-block. */
  Curves *dst_curves_id = bke::curves_new_nomain(points_num, curves_num);
  bke::CurvesGeometry &dst_curves = dst_curves_id->geometry.wrap();
  if (custom_knot_num) {
    dst_curves.nurbs_custom_knots_resize(custom_knot_num);
  }
  dst_curves.offsets_for_write().last() = points_num;
  r_result.geometry.replace_curves(dst_curves_id);
  bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();

  /* Copy settings from the first input geometry set with curves. */
  const RealizeCurveTask &first_task = tasks.first();
  const Curves &first_curves_id = *first_task.curve_info->curves;
  bke::curves_copy_parameters(first_curves_id, *dst_curves_id);

  copy_vertex_group_names(dst_curves, ordered_attributes, all_curves_info.order);

  /* Prepare id attribute. */
  SpanAttributeWriter<int> point_ids;
  if (all_curves_info.create_id_attribute) {
    point_ids = dst_attributes.lookup_or_add_for_write_only_span<int>("id",
                                                                      bke::AttrDomain::Point);
  }

  /* Prepare generic output attributes. */
  Vector<GSpanAttributeWriter> dst_attribute_writers;
  for (const int attribute_index : ordered_attributes.index_range()) {
    const StringRef attribute_id = ordered_attributes.ids[attribute_index];
    const bke::AttrDomain domain = ordered_attributes.kinds[attribute_index].domain;
    const bke::AttrType data_type = ordered_attributes.kinds[attribute_index].data_type;
    dst_attribute_writers.append(
        dst_attributes.lookup_or_add_for_write_only_span(attribute_id, domain, data_type));
  }

  /* Prepare handle position attributes if necessary. */
  SpanAttributeWriter<float3> handle_left;
  SpanAttributeWriter<float3> handle_right;
  if (all_curves_info.create_handle_postion_attributes) {
    handle_left = dst_attributes.lookup_or_add_for_write_only_span<float3>("handle_left",
                                                                           bke::AttrDomain::Point);
    handle_right = dst_attributes.lookup_or_add_for_write_only_span<float3>(
        "handle_right", bke::AttrDomain::Point);
  }

  SpanAttributeWriter<float> radius;
  if (all_curves_info.create_radius_attribute) {
    radius = dst_attributes.lookup_or_add_for_write_only_span<float>("radius",
                                                                     bke::AttrDomain::Point);
  }
  SpanAttributeWriter<float3> custom_normal;
  if (all_curves_info.create_custom_normal_attribute) {
    custom_normal = dst_attributes.lookup_or_add_for_write_only_span<float3>(
        "custom_normal", bke::AttrDomain::Point);
  }

  /* Actually execute all tasks. */
  threading::parallel_for(tasks.index_range(), 100, [&](const IndexRange task_range) {
    for (const int task_index : task_range) {
      const RealizeCurveTask &task = tasks[task_index];
      execute_realize_curve_task(options,
                                 all_curves_info,
                                 task,
                                 ordered_attributes,
                                 dst_curves,
                                 dst_attribute_writers,
                                 point_ids.span,
                                 handle_left.span,
                                 handle_right.span,
                                 radius.span,
                                 custom_normal.span);
    }
  });

  /* Type counts have to be updated eagerly. */
  dst_curves.runtime->type_counts.fill(0);
  for (const RealizeCurveTask &task : tasks) {
    for (const int i : IndexRange(CURVE_TYPES_NUM)) {
      dst_curves.runtime->type_counts[i] +=
          task.curve_info->curves->geometry.runtime->type_counts[i];
    }
  }

  /* Tag modified attributes. */
  for (GSpanAttributeWriter &dst_attribute : dst_attribute_writers) {
    dst_attribute.finish();
  }
  point_ids.finish();
  radius.finish();
  handle_left.finish();
  handle_right.finish();
  custom_normal.finish();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil
 * \{ */

static OrderedAttributes gather_generic_grease_pencil_attributes_to_propagate(
    const bke::GeometrySet &in_geometry_set,
    const RealizeInstancesOptions &options,
    const VariedDepthOptions &varied_depth_options)
{
  bke::GeometrySet::GatheredAttributes attributes_to_propagate = gather_attributes_to_propagate(
      in_geometry_set, bke::GeometryComponent::Type::GreasePencil, options, varied_depth_options);
  OrderedAttributes ordered_attributes;
  for (const int i : attributes_to_propagate.names.index_range()) {
    ordered_attributes.ids.add_new(attributes_to_propagate.names[i]);
    ordered_attributes.kinds.append(attributes_to_propagate.kinds[i]);
  }
  return ordered_attributes;
}

static void gather_grease_pencils_to_realize(const bke::GeometrySet &geometry_set,
                                             VectorSet<const GreasePencil *> &r_grease_pencils)
{
  if (const GreasePencil *grease_pencil = geometry_set.get_grease_pencil()) {
    if (!grease_pencil->layers().is_empty()) {
      r_grease_pencils.add(grease_pencil);
    }
  }
  if (const Instances *instances = geometry_set.get_instances()) {
    instances->foreach_referenced_geometry([&](const bke::GeometrySet &instance_geometry_set) {
      gather_grease_pencils_to_realize(instance_geometry_set, r_grease_pencils);
    });
  }
}

static AllGreasePencilsInfo preprocess_grease_pencils(
    const bke::GeometrySet &geometry_set,
    const RealizeInstancesOptions &options,
    const VariedDepthOptions &varied_depth_options)
{
  AllGreasePencilsInfo info;
  info.attributes = gather_generic_grease_pencil_attributes_to_propagate(
      geometry_set, options, varied_depth_options);

  gather_grease_pencils_to_realize(geometry_set, info.order);
  info.realize_info.reinitialize(info.order.size());
  for (const int grease_pencil_index : info.realize_info.index_range()) {
    GreasePencilRealizeInfo &grease_pencil_info = info.realize_info[grease_pencil_index];
    const GreasePencil *grease_pencil = info.order[grease_pencil_index];
    grease_pencil_info.grease_pencil = grease_pencil;

    bke::AttributeAccessor attributes = grease_pencil->attributes();
    grease_pencil_info.attributes.reinitialize(info.attributes.size());
    for (const int attribute_index : info.attributes.index_range()) {
      const StringRef attribute_id = info.attributes.ids[attribute_index];
      const bke::AttrType data_type = info.attributes.kinds[attribute_index].data_type;
      const bke::AttrDomain domain = info.attributes.kinds[attribute_index].domain;
      if (attributes.contains(attribute_id)) {
        GVArray attribute = *attributes.lookup_or_default(attribute_id, domain, data_type);
        grease_pencil_info.attributes[attribute_index].emplace(std::move(attribute));
      }
    }

    grease_pencil_info.material_index_map.reinitialize(grease_pencil->material_array_num);
    for (const int i : IndexRange(grease_pencil->material_array_num)) {
      Material *material = grease_pencil->material_array[i];
      grease_pencil_info.material_index_map[i] = info.materials.index_of_or_add(material);
    }
  }
  return info;
}

static void execute_realize_grease_pencil_task(
    const RealizeGreasePencilTask &task,
    const OrderedAttributes &ordered_attributes,
    GreasePencil &dst_grease_pencil,
    MutableSpan<GSpanAttributeWriter> dst_attribute_writers)
{
  const GreasePencilRealizeInfo &grease_pencil_info = *task.grease_pencil_info;
  const GreasePencil &src_grease_pencil = *grease_pencil_info.grease_pencil;
  const Span<const bke::greasepencil::Layer *> src_layers = src_grease_pencil.layers();
  const IndexRange dst_layers_slice{task.start_index, src_layers.size()};
  const Span<bke::greasepencil::Layer *> dst_layers = dst_grease_pencil.layers_for_write().slice(
      dst_layers_slice);

  for (const int layer_i : src_layers.index_range()) {
    const bke::greasepencil::Layer &src_layer = *src_layers[layer_i];
    bke::greasepencil::Layer &dst_layer = *dst_layers[layer_i];
    BKE_grease_pencil_copy_layer_parameters(src_layer, dst_layer);

    dst_layer.set_name(src_layer.name());
    dst_layer.set_local_transform(task.transform * src_layer.local_transform());

    const bke::greasepencil::Drawing *src_drawing = src_grease_pencil.get_eval_drawing(src_layer);
    if (!src_drawing) {
      continue;
    }
    bke::greasepencil::Drawing &dst_drawing = *dst_grease_pencil.get_eval_drawing(dst_layer);

    const bke::CurvesGeometry &src_curves = src_drawing->strokes();
    bke::CurvesGeometry &dst_curves = dst_drawing.strokes_for_write();
    dst_curves = src_curves;

    /* Remap materials. */
    bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
    bke::SpanAttributeWriter<int> material_indices =
        dst_attributes.lookup_or_add_for_write_span<int>("material_index", bke::AttrDomain::Curve);
    for (int &material_index : material_indices.span) {
      if (material_index >= 0 && material_index < src_grease_pencil.material_array_num) {
        material_index = grease_pencil_info.material_index_map[material_index];
      }
    }
    material_indices.finish();
  }

  copy_generic_attributes_to_result(
      grease_pencil_info.attributes,
      task.attribute_fallbacks,
      ordered_attributes,
      [&](const bke::AttrDomain domain) {
        BLI_assert(domain == bke::AttrDomain::Layer);
        UNUSED_VARS_NDEBUG(domain);
        return dst_layers_slice;
      },
      dst_attribute_writers);
}

static void transform_grease_pencil_layers(Span<bke::greasepencil::Layer *> layers,
                                           const float4x4 &transform)
{
  for (bke::greasepencil::Layer *layer : layers) {
    layer->set_local_transform(transform * layer->local_transform());
  }
}

static void execute_realize_grease_pencil_tasks(
    const AllGreasePencilsInfo &all_grease_pencils_info,
    const GatherOffsets &offsets,
    const Span<RealizeGreasePencilTask> tasks,
    const OrderedAttributes &ordered_attributes,
    RealizeInstancesResult &r_result)
{
  if (tasks.is_empty()) {
    return;
  }

  if (tasks.size() == 1) {
    const RealizeGreasePencilTask &task = tasks.first();
    GreasePencil *new_gp = BKE_grease_pencil_copy_for_eval(task.grease_pencil_info->grease_pencil);
    if (!skip_transform(task.transform)) {
      transform_grease_pencil_layers(new_gp->layers_for_write(), task.transform);
    }
    add_instance_attributes_to_single_geometry(
        ordered_attributes, task.attribute_fallbacks, new_gp->attributes_for_write());
    r_result.geometry.replace_grease_pencil(new_gp);
    return;
  }

  const int64_t new_layers_num = offsets.grease_pencil_layer_offset;
  if (!valid_int_num(new_layers_num)) {
    r_result.errors.append(RPT_("Realized grease pencil has too many layers."));
    return;
  }

  /* Allocate new grease pencil. */
  GreasePencil *dst_grease_pencil = BKE_grease_pencil_new_nomain();
  BKE_grease_pencil_copy_parameters(*tasks.first().grease_pencil_info->grease_pencil,
                                    *dst_grease_pencil);
  r_result.geometry.replace_grease_pencil(dst_grease_pencil);

  /* Allocate all layers. */
  dst_grease_pencil->add_layers_with_empty_drawings_for_eval(new_layers_num);

  /* Transfer material pointers. The material indices are updated for each task separately. */
  if (!all_grease_pencils_info.materials.is_empty()) {
    MEM_SAFE_FREE(dst_grease_pencil->material_array);
    dst_grease_pencil->material_array_num = all_grease_pencils_info.materials.size();
    dst_grease_pencil->material_array = MEM_calloc_arrayN<Material *>(
        dst_grease_pencil->material_array_num, __func__);
    uninitialized_copy_n(all_grease_pencils_info.materials.data(),
                         dst_grease_pencil->material_array_num,
                         dst_grease_pencil->material_array);
  }

  /* Prepare generic output attributes. */
  bke::MutableAttributeAccessor dst_attributes = dst_grease_pencil->attributes_for_write();
  Vector<GSpanAttributeWriter> dst_attribute_writers;
  for (const int attribute_index : ordered_attributes.index_range()) {
    const StringRef attribute_id = ordered_attributes.ids[attribute_index];
    const bke::AttrType data_type = ordered_attributes.kinds[attribute_index].data_type;
    dst_attribute_writers.append(dst_attributes.lookup_or_add_for_write_only_span(
        attribute_id, bke::AttrDomain::Layer, data_type));
  }

  /* Actually execute all tasks. */
  threading::parallel_for(tasks.index_range(), 100, [&](const IndexRange task_range) {
    for (const int task_index : task_range) {
      const RealizeGreasePencilTask &task = tasks[task_index];
      execute_realize_grease_pencil_task(
          task, ordered_attributes, *dst_grease_pencil, dst_attribute_writers);
    }
  });

  /* Tag modified attributes. */
  for (GSpanAttributeWriter &dst_attribute : dst_attribute_writers) {
    dst_attribute.finish();
  }
}
/* -------------------------------------------------------------------- */
/** \name Edit Data
 * \{ */

static void execute_realize_edit_data_tasks(const Span<RealizeEditDataTask> tasks,
                                            bke::GeometrySet &r_realized_geometry)
{
  if (tasks.is_empty()) {
    return;
  }

  auto &component = r_realized_geometry.get_component_for_write<bke::GeometryComponentEditData>();
  for (const RealizeEditDataTask &task : tasks) {
    if (!component.curves_edit_hints_) {
      if (task.edit_data->curves_edit_hints_) {
        component.curves_edit_hints_ = std::make_unique<bke::CurvesEditHints>(
            *task.edit_data->curves_edit_hints_);
      }
    }
    if (const bke::GizmoEditHints *src_gizmo_edit_hints = task.edit_data->gizmo_edit_hints_.get())
    {
      if (!component.gizmo_edit_hints_) {
        component.gizmo_edit_hints_ = std::make_unique<bke::GizmoEditHints>();
      }
      for (auto item : src_gizmo_edit_hints->gizmo_transforms.items()) {
        component.gizmo_edit_hints_->gizmo_transforms.add(item.key, task.transform * item.value);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Realize Instances
 * \{ */

static void remove_id_attribute_from_instances(bke::GeometrySet &geometry_set)
{
  Instances *instances = geometry_set.get_instances_for_write();
  if (!instances) {
    return;
  }
  instances->attributes_for_write().remove("id");
  instances->ensure_geometry_instances();
  for (bke::InstanceReference &reference : instances->references_for_write()) {
    if (reference.type() == bke::InstanceReference::Type::GeometrySet) {
      remove_id_attribute_from_instances(reference.geometry_set());
    }
  }
}

/** Propagate instances from the old geometry set to the new geometry set if they are not
 * realized.
 */
static void propagate_instances_to_keep(const bke::GeometrySet &geometry_set,
                                        const IndexMask &selection,
                                        bke::GeometrySet &new_geometry_set,
                                        const bke::AttributeFilter &attribute_filter)
{
  const Instances &instances = *geometry_set.get_instances();
  IndexMaskMemory inverse_selection_indices;
  const IndexMask inverse_selection = selection.complement(IndexRange(instances.instances_num()),
                                                           inverse_selection_indices);
  /* Check not all instances are being realized. */
  if (inverse_selection.is_empty()) {
    return;
  }

  std::unique_ptr<Instances> new_instances = std::make_unique<Instances>(instances);
  new_instances->remove(inverse_selection, attribute_filter);

  bke::InstancesComponent &new_instances_components =
      new_geometry_set.get_component_for_write<bke::InstancesComponent>();
  new_instances_components.replace(new_instances.release(), bke::GeometryOwnershipType::Owned);
}

RealizeInstancesResult realize_instances(bke::GeometrySet geometry_set,
                                         const RealizeInstancesOptions &options)
{
  if (!geometry_set.has_instances()) {
    return {geometry_set};
  }

  VariedDepthOptions all_instances;
  all_instances.depths = VArray<int>::from_single(VariedDepthOptions::MAX_DEPTH,
                                                  geometry_set.get_instances()->instances_num());
  all_instances.selection = IndexMask(geometry_set.get_instances()->instances_num());
  return realize_instances(geometry_set, options, all_instances);
}

RealizeInstancesResult realize_instances(bke::GeometrySet geometry_set,
                                         const RealizeInstancesOptions &options,
                                         const VariedDepthOptions &varied_depth_option)
{
  /* The algorithm works in three steps:
   * 1. Preprocess each unique geometry that is instanced (e.g. each `Mesh`).
   * 2. Gather "tasks" that need to be executed to realize the instances. Each task corresponds
   * to instances of the previously preprocessed geometry.
   * 3. Execute all tasks in parallel.
   */

  if (!geometry_set.has_instances()) {
    return {geometry_set};
  }

  bke::GeometrySet not_to_realize_set;
  propagate_instances_to_keep(
      geometry_set, varied_depth_option.selection, not_to_realize_set, options.attribute_filter);

  if (options.keep_original_ids) {
    remove_id_attribute_from_instances(geometry_set);
  }

  AllPointCloudsInfo all_pointclouds_info = preprocess_pointclouds(
      geometry_set, options, varied_depth_option);
  AllMeshesInfo all_meshes_info = preprocess_meshes(geometry_set, options, varied_depth_option);
  AllCurvesInfo all_curves_info = preprocess_curves(geometry_set, options, varied_depth_option);
  AllGreasePencilsInfo all_grease_pencils_info = preprocess_grease_pencils(
      geometry_set, options, varied_depth_option);
  OrderedAttributes all_instance_attributes = gather_generic_instance_attributes_to_propagate(
      geometry_set, options, varied_depth_option);

  const bool create_id_attribute = all_pointclouds_info.create_id_attribute ||
                                   all_meshes_info.create_id_attribute ||
                                   all_curves_info.create_id_attribute;
  Vector<std::unique_ptr<GArray<>>> temporary_arrays;
  GatherTasksInfo gather_info = {all_pointclouds_info,
                                 all_meshes_info,
                                 all_curves_info,
                                 all_grease_pencils_info,
                                 all_instance_attributes,
                                 create_id_attribute,
                                 varied_depth_option.selection,
                                 varied_depth_option.depths,
                                 temporary_arrays};

  if (not_to_realize_set.has_instances()) {
    gather_info.instances.instances_components_to_merge.append(
        not_to_realize_set.get_component_for_write<bke::InstancesComponent>().copy());
    gather_info.instances.instances_components_transforms.append(float4x4::identity());
    gather_info.instances.attribute_fallback.append(gather_info.instances_attriubutes.size());
  }

  const float4x4 transform = float4x4::identity();
  InstanceContext attribute_fallbacks(gather_info);

  initialize_curves_builtin_attribute_defaults(all_curves_info, attribute_fallbacks);

  gather_realize_tasks_recursive(
      gather_info, 0, VariedDepthOptions::MAX_DEPTH, geometry_set, transform, attribute_fallbacks);

  RealizeInstancesResult result;
  execute_instances_tasks(gather_info.instances.instances_components_to_merge,
                          gather_info.instances.instances_components_transforms,
                          all_instance_attributes,
                          gather_info.instances.attribute_fallback,
                          result.geometry);

  const int64_t total_points_num = get_final_points_num(gather_info.r_tasks);
  /* This doesn't have to be exact at all, it's just a rough estimate to make decisions about
   * multi-threading (overhead). */
  const int64_t approximate_used_bytes_num = total_points_num * 32;
  threading::memory_bandwidth_bound_task(approximate_used_bytes_num, [&]() {
    execute_realize_pointcloud_tasks(options,
                                     gather_info.r_offsets,
                                     all_pointclouds_info,
                                     gather_info.r_tasks.pointcloud_tasks,
                                     all_pointclouds_info.attributes,
                                     result);
    execute_realize_mesh_tasks(options,
                               gather_info.r_offsets,
                               all_meshes_info,
                               gather_info.r_tasks.mesh_tasks,
                               all_meshes_info.attributes,
                               all_meshes_info.materials,
                               result);
    execute_realize_curve_tasks(options,
                                gather_info.r_offsets,
                                all_curves_info,
                                gather_info.r_tasks.curve_tasks,
                                all_curves_info.attributes,
                                result);
    execute_realize_grease_pencil_tasks(all_grease_pencils_info,
                                        gather_info.r_offsets,
                                        gather_info.r_tasks.grease_pencil_tasks,
                                        all_grease_pencils_info.attributes,
                                        result);
    execute_realize_edit_data_tasks(gather_info.r_tasks.edit_data_tasks, result.geometry);
  });
  if (gather_info.r_tasks.first_volume) {
    result.geometry.add(*gather_info.r_tasks.first_volume);
  }

  return result;
}

/** \} */

}  // namespace blender::geometry
