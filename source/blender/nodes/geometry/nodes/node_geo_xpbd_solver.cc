/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_bvhutils.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.hh"
#include "BKE_mesh_sample.hh"
#include "BKE_pointcloud.hh"

#include "BLI_math_geom.h"
#include "BLI_stack.hh"
#include "BLI_virtual_array_range_spans.hh"

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"

#include "GEO_xpbd_constraint_collision_edge.hh"
#include "GEO_xpbd_constraint_collision_face.hh"
#include "GEO_xpbd_constraint_damping_angular.hh"
#include "GEO_xpbd_constraint_damping_linear.hh"
#include "GEO_xpbd_constraint_distance.hh"
#include "GEO_xpbd_constraint_friction_edge.hh"
#include "GEO_xpbd_constraint_friction_face.hh"
#include "GEO_xpbd_constraint_pin_position.hh"
#include "GEO_xpbd_constraint_pin_rotation.hh"
#include "GEO_xpbd_constraint_rod_bend_twist.hh"
#include "GEO_xpbd_constraint_rod_stretch_shear.hh"

#include "NOD_geo_tag_filter.hh"
#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_geometry_nodes_physics_bundles.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_xpbd_solver_cc {

using namespace physics_bundles;

namespace attribute_names {

constexpr StringRefNull position = "position";
constexpr StringRefNull velocity = "velocity";
constexpr StringRefNull rotation = "rotation";
constexpr StringRefNull angular_velocity = "angular_velocity";
constexpr StringRefNull external_force = "external_force";
constexpr StringRefNull external_torque = "external_torque";
constexpr StringRefNull mass = "mass";
constexpr StringRefNull moment_of_inertia = "moment_of_inertia";
constexpr StringRefNull static_friction = "static_friction";
constexpr StringRefNull dynamic_friction = "dynamic_friction";
constexpr StringRefNull radius = "radius";

}  // namespace attribute_names

class XPBDSolverDataBundle {
 public:
  static constexpr StringRefNull name = "Blender.XPBDSolverData";
  static const FlatBundleTypePtr &get_bundle_type();
};

const FlatBundleTypePtr &XPBDSolverDataBundle::get_bundle_type()
{
  static const FlatBundleTypePtr bundle_type = []() {
    FlatBundleTypeBuilder b(XPBDSolverDataBundle::name);
    b.add<decl::Float>("residual_error"_ustr)
        .min(0.0f)
        .description(
            "Average remaining relative error, values smaller than one are below the constraint "
            "threshold.");
    const FlatBundleTypePtr bundle_type = b.build();
    BundleTypeRegistry::register_type(bundle_type);
    return bundle_type;
  }();
  return bundle_type;
}

static NestedBundleTypePtr make_world_type()
{
  Vector<std::shared_ptr<const FlatBundleType>> types;
  types.append(XPBDSolverDataBundle::get_bundle_type());
  types.append(DampingBundle::get_bundle_type());
  types.append(MeshColliderBundle::get_bundle_type());
  types.append(CollisionContactsBundle::get_bundle_type());
  types.append(RodStretchShearBundle::get_bundle_type());
  types.append(RodBendTwistBundle::get_bundle_type());
  types.append(PinPositionBundle::get_bundle_type());
  types.append(PinRotationBundle::get_bundle_type());
  types.append(EdgeLengthConstraintBundle::get_bundle_type());
  types.append(CrossEdgeLengthConstraintBundle::get_bundle_type());

  /* Not actually used by the node but only registered here. */
  ForceBundle::get_bundle_type();
  CustomGeometryEffector::get_bundle_type();
  CustomWorldEffector::get_bundle_type();

  NestedBundleTypePtr world_type = std::make_shared<const NestedBundleType>(
      "Blender.XPBDSolverWorld", std::move(types));
  BundleTypeRegistry::register_type(world_type);
  return world_type;
}

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  static NestedBundleTypePtr world_type = make_world_type();
  b.add_input<decl::Bundle>("World"_ustr)
      .bundle_type(world_type)
      .evaluated_geometry_field()
      .structure_type(StructureType::Single)
      .description("World state that is updated by the solver");
  b.add_output<decl::Bundle>("World"_ustr).pass_through_input_index(0).align_with_previous();
  b.add_input<decl::Float>("Delta Time"_ustr)
      .min(0)
      .default_value(1 / 25.0f)
      .subtype(PROP_TIME_ABSOLUTE);

  b.add_input<decl::String>("Filter"_ustr)
      .optional_label()
      .description("Filters the geometry sets to process based on their tags");

  b.add_input<decl::Matrix>("Simulation to World"_ustr);

  {
    auto &solver_panel = b.add_panel("Solver"_ustr).default_closed(true);
    solver_panel.add_input<decl::Int>("Substeps"_ustr).default_value(10).min(1);
    solver_panel.add_input<decl::Int>("Constraint Iterations"_ustr).default_value(1).min(1);
    solver_panel.add_input<decl::String>("Solver Path"_ustr)
        .default_value("")
        .description("Optional output path in the world bundle for solver data");
  }
  {
    auto &p = b.add_panel("Interpolation Range"_ustr).default_closed(true);
    p.add_input<decl::Float>("Begin"_ustr).default_value(0.0).min(0.0);
    p.add_input<decl::Float>("End"_ustr).default_value(1.0).min(0.0);
  }
}

struct DataKey {
  int geo_bundle_i;
  bke::GeometryComponent::Type type;
  std::optional<int> layer_i;

  uint64_t hash() const
  {
    return get_default_hash(this->geo_bundle_i, this->type, this->layer_i.value_or(0));
  }

  friend bool operator==(const DataKey &a, const DataKey &b) = default;
};

struct GeometryDataChunk {
  int data_key_i;
  IndexRange points_range;
  /** Only used when the geometry data has curves. */
  std::optional<IndexRange> curves_range;
};

struct DampingConstraint {
  std::string path;
};
struct DampingConstraintUsage {
  /** Index of corresponding #DampingConstraint. */
  int constraint_i;
  VArrayRangeSpans<float> linear_dampings;
  VArrayRangeSpans<float> angular_dampings;
  MutableSpan<float> linear_damping_lambdas;
  MutableSpan<float> angular_damping_lambdas;
};

struct RodStretchShearConstraint {
  std::string path;
  float error_threshold;
  std::string lambda_pos_attr;
  std::string lambda_rot_attr;
};
struct RodStretchShearConstraintUsage {
  /** Index of corresponding #RodStretchShearConstraint. */
  int constraint_i;
  bool is_valid = false;
  VArraySpan<float> rest_lengths;
  VArrayRangeSpans<float> compliances;
  MutableSpan<float3> lambdas_pos;
  MutableSpan<float3> lambdas_rot;
};

struct RodBendTwistConstraint {
  std::string path;
  float error_threshold;
};
struct RodBendTwistConstraintUsage {
  /** Index of corresponding #RodBendTwistConstraint. */
  int constraint_i;
  VArraySpan<math::Quaternion> rest_bend_rotations;
  VArrayRangeSpans<float> compliances;
  MutableSpan<float4> lambdas;
};

struct PinPositionConstraint {
  std::string path;
  float error_threshold;
  std::string lambda_attr;
};
struct PinPositionConstraintUsage {
  /** Index of corresponding #PinPositionConstraint. */
  int constraint_i;

  Span<int> points;
  Span<float3> begin_positions;
  Span<float3> end_positions;
  Span<float> compliances;

  MutableSpan<float3> current_positions;
  MutableSpan<float> lambdas;
};
struct PinPositionConstraintChunkUsage {
  /** Index of the corresponding #PinPositionConstraintUsage. */
  int constraint_usage_i;
  IndexRange pin_range;
};

struct PinRotationConstraint {
  std::string path;
  float error_threshold;
};
struct PinRotationConstraintUsage {
  /** Index of corresponding #PinRotationConstraint. */
  int constraint_i;

  Span<int> points;
  Span<math::Quaternion> begin_rotations;
  Span<math::Quaternion> end_rotations;
  Span<float> compliances;

  MutableSpan<math::Quaternion> current_rotations;
  MutableSpan<float4> lambdas;
};
struct PinRotationConstraintChunkUsage {
  /** Index of the corresponding #PinRotationConstraintUsage. */
  int constraint_usage_i;
  IndexRange pin_range;
};

struct EdgeLengthConstraint {
  std::string path;
  float error_threshold;
};
struct EdgeLengthConstraintUsage {
  /** Index of corresponding #EdgeLengthConstraint. */
  int constraint_i;
  bool is_valid = false;
  VArraySpan<float> rest_lengths;
  VArraySpan<float> compliances;
  MutableSpan<float> lambdas;

  xpbd::ConstraintColoring coloring;
};

struct CrossEdgeLengthConstraint {
  std::string path;
  float error_threshold;
};
struct CrossEdgeLengthConstraintUsage {
  /** Index of the corresponding #CrossEdgeLengthConstraint. */
  int constraint_i;

  Vector<int2> cross_edge_points;
  Vector<float> distances;
  Vector<float> compliances;
  MutableSpan<float> lambdas;

  xpbd::ConstraintColoring coloring;
};

struct StaticMeshInfo {
  const Mesh *mesh;
  bke::BVHTreeFromMesh corner_tris_bvh;
  bke::BVHTreeFromMesh edges_bvh;
};

struct DeformingMeshInfo {
  /** This is one longer than the number of substeps because it also contains the initial mesh. */
  Vector<const Mesh *> substep_meshes;
  /**
   * This contains one bvh tree per substep. A future optimization could be to not build
   * independent BVH trees for each mesh because the are usually very similar.
   */
  Vector<bke::BVHTreeFromMesh> substep_corner_tris_bvh_trees;
  Vector<bke::BVHTreeFromMesh> substep_edges_bvh_trees;
};

struct MeshCollider {
  std::string path;
  Vector<int> instance_ids;
  std::variant<StaticMeshInfo, DeformingMeshInfo> mesh;
  float4x4 begin_transform;
  float4x4 end_transform;
  float margin;
  float friction;
  float compliance;
  bool use_edge_contacts;
  /** If true, the points are expected to stay inside of the mesh instead of being pushed out. */
  bool is_boundary;
  float error_threshold;
};
struct MeshColliderUsage {
  /** Index of corresponding #MeshCollider. */
  int constraint_i;
};

struct CollisionContacts {
  std::string path;
};

struct ConstraintWithColoring {
  xpbd::ConstraintSet *constraint;
  xpbd::ConstraintColoring coloring;
};

struct GeometryData {
  bke::MutableAttributeAccessor attributes;
  AttrDomain domain;
  int size;
  bool uses_rotation = false;
  /** Easy access to curve data for curves and grease pencil layers. */
  bke::CurvesGeometry *curves = nullptr;

  IndexRange chunks;
  /**
   * Allows fast lookup of which chunk a point belongs to. This allows e.g. grouping collisions by
   * chunk efficiently.
   */
  Array<int> point_to_chunk;

  bke::SpanAttributeWriter<float3> position_attr;
  bke::SpanAttributeWriter<float3> velocity_attr;
  bke::SpanAttributeWriter<math::Quaternion> rotation_attr;
  bke::SpanAttributeWriter<float3> angular_velocity_attr;

  /**
   * Temporary arrays for positions and rotations. This is necessary because xpbd requires the old
   * and new positions in the end to compute the new velocities.
   */
  Array<float3> temp_positions;
  Array<math::Quaternion> temp_rotations;

  VArray<float> masses;
  VArraySpan<float3> moments_of_inertia;

  VArraySpan<float3> external_force_attr;
  VArraySpan<float3> external_torque_attr;

  CacheMutex load_friction_mutex;
  VArray<float> static_frictions;
  VArray<float> dynamic_frictions;

  CacheMutex load_radius_mutex;
  VArray<float> radii;
  /** Actually the same as radii for now. */
  VArray<float> prev_radii;

  Array<float> inv_masses;
  Array<float3> inv_moments_of_inertia;

  Array<bool> is_hard_pinned;

  Vector<DampingConstraintUsage> damping_constraints;
  Vector<PinPositionConstraintUsage> pin_position_constraints;
  Vector<PinRotationConstraintUsage> pin_rotation_constraints;
  Vector<EdgeLengthConstraintUsage> edge_length_constraints;
  Vector<CrossEdgeLengthConstraintUsage> cross_edge_length_constraints;
  Vector<MeshColliderUsage> mesh_colliders;
  Vector<RodStretchShearConstraintUsage> rod_stretch_shear_constraints;
  Vector<RodBendTwistConstraintUsage> rod_bend_twist_constraints;

  Vector<ConstraintWithColoring> static_constraints;

  GeometryData(const bke::MutableAttributeAccessor attributes) : attributes(attributes) {}
};

struct GeometrySetData {
  std::string path;
  /**
   * The geometry is moved out of the world bundle for local processing and is moved back in the
   * end.
   */
  GeometrySet geometry;
  Set<std::string> tags;
};

struct Geometries {
  Vector<GeometrySetData> geometry_sets;

  VectorSet<DataKey> data_keys;
  Vector<GeometryData *> data;

  /**
   * By having static chunks, across the entire solve step allows for more efficient
   * multi-threading (and preparation for multi-threading).
   */
  Vector<GeometryDataChunk> chunks;
  int max_chunk_size = -1;
  int64_t total_points_num = -1;

  /**
   * Two arrays of geometry references are used because the arrays containing the previous and
   * current positions/rotations are swapped for different time steps to avoid unnecessary copying.
   *
   * Index 0: Previous data is stored in temporary arrays, current data in the geometry attributes.
   * Index 1: Previous data is stored in the geometry attributes, current data in temporary arrays.
   */
  std::array<Array<xpbd::GeometryRef>, 2> solver_refs;
};

struct SubstepInterval {
  int current_i;
  float interpolate_begin;
  float interpolate_end;

  SubstepInterval(const int substeps,
                  const int current_i,
                  const float interpolation_begin,
                  const float interpolation_end)
      : current_i(current_i),
        interpolate_begin(float(current_i) / substeps * (interpolation_end - interpolation_begin) +
                          interpolation_begin),
        interpolate_end(float(current_i + 1) / substeps *
                            (interpolation_end - interpolation_begin) +
                        interpolation_begin)
  {
  }
};

struct MeshContactId {
  int mesh_collider_i;
  int point_i;

  uint64_t hash() const
  {
    return get_default_hash(this->mesh_collider_i, this->point_i);
  }

  friend bool operator==(const MeshContactId &a, const MeshContactId &b) = default;
};

struct ExternalFaceContacts {
  Map<MeshContactId, int> mesh_contact_indices;

  Vector<int> points;
  Vector<float> point_radii;
  Vector<float3> positions_on_face;
  /* The movement of the collider in the current substep. */
  Vector<float3> collider_motion;
  Vector<float3> collider_velocities;
  Vector<float3> face_normals;
  Vector<float> face_margins;
  Vector<float> static_frictions;
  Vector<float> dynamic_frictions;
  Vector<float> compliance_terms;
  Vector<float> error_scales;

  Vector<bool> active_states;
  Vector<float> lambdas_normal;
  Vector<float> lambdas_friction;

  void init_or_preserve_state(const ExternalFaceContacts &prev_contacts,
                              const std::optional<int> &prev_i)
  {
    if (prev_i) {
      this->active_states.append(prev_contacts.active_states[*prev_i]);
      this->lambdas_normal.append(prev_contacts.lambdas_normal[*prev_i]);
      this->lambdas_friction.append(prev_contacts.lambdas_friction[*prev_i]);
    }
    else {
      this->active_states.append(false);
      this->lambdas_normal.append(0.0f);
      this->lambdas_friction.append(0.0f);
    }
  }
};

struct ExternalEdgeContacts {
  Map<MeshContactId, int> mesh_contact_indices;

  Vector<int2> point_pairs;
  Vector<float2> point_radii;
  Vector<float3> positions_on_edge;
  /* The movement of the collider in the current substep. */
  Vector<float3> collider_motion;
  Vector<float3> collider_velocities;
  Vector<float3> edge_directions;
  Vector<float3> edge_normals;
  Vector<float> edge_margins;
  Vector<float> static_frictions;
  Vector<float> dynamic_frictions;
  Vector<float> compliance_terms;
  Vector<float> error_scales;

  Vector<bool> active_states;
  Vector<float> point_mix_factors;
  Vector<float> lambdas_normal;
  Vector<float> lambdas_friction;

  void init_or_preserve_state(const ExternalEdgeContacts &prev_contacts,
                              const std::optional<int> &prev_i)
  {
    if (prev_i) {
      this->active_states.append(prev_contacts.active_states[*prev_i]);
      this->point_mix_factors.append(prev_contacts.point_mix_factors[*prev_i]);
      this->lambdas_normal.append(prev_contacts.lambdas_normal[*prev_i]);
      this->lambdas_friction.append(prev_contacts.lambdas_friction[*prev_i]);
    }
    else {
      this->active_states.append(false);
      this->point_mix_factors.append(0.0f);
      this->lambdas_normal.append(0.0f);
      this->lambdas_friction.append(0.0f);
    }
  }
};

struct ChunkData {
  Vector<xpbd::ConstraintSet *> static_constraints;
  Vector<xpbd::VelocityConstraintSet *> static_velocity_constraints;

  Vector<PinPositionConstraintChunkUsage> pin_position_constraints;
  Vector<PinRotationConstraintChunkUsage> pin_rotation_constraints;

  ExternalFaceContacts external_face_contacts;
  ExternalEdgeContacts external_edge_contacts;
};

struct ConstraintsInfo {
  Vector<MeshCollider> mesh_colliders;
  Vector<CollisionContacts> collision_contacts;
  Vector<DampingConstraint> damping_constraints;
  Vector<RodStretchShearConstraint> rod_stretch_shear_constraints;
  Vector<RodBendTwistConstraint> rod_bend_twist_constraints;
  Vector<PinPositionConstraint> pin_position_constraints;
  Vector<PinRotationConstraint> pin_rotation_constraints;
  Vector<EdgeLengthConstraint> edge_length_constraints;
  Vector<CrossEdgeLengthConstraint> cross_edge_length_constraints;
};

class XpbdSolverStep {
 public:
  struct Result {
    /* Average relative residual error of all constraints. The average absolute error of a specific
     * constraint type is scaled by a threshold value, anything below the threshold is considered
     * "solved". This allows combining constraint residuals of different types into a single value.
     */
    float total_residual_error;
  };

 private:
  struct TLS {
    ResourceScope scope;
    IndexMaskMemory &mask_memory;
    LinearAllocator<> &allocator;

    TLS() : mask_memory(scope.construct<IndexMaskMemory>()), allocator(mask_memory) {}
  };

  threading::EnumerableThreadSpecific<TLS> tls_;

  /** The simulation world that is being modified. */
  Bundle &world_;

  const int substeps_;
  const float sub_delta_time_;
  float substep_compliance_factor_;

  const float interpolation_begin_;
  const float interpolation_end_;

  const float4x4 world_to_simulation_;

  std::string geometry_tag_filter_;

  int constraint_iterations_;

  Geometries geometries_;
  ConstraintsInfo constraints_;
  Array<ChunkData> chunks_data_;

  Mutex field_evaluators_mutex_;

  Result result_;
  Mutex warnings_mutex_;
  VectorSet<std::pair<NodeWarningType, std::string>> warnings_;

  MultiValueMap<std::string, std::string> nested_bundle_paths_;

 public:
  XpbdSolverStep(Bundle &world,
                 const float total_delta_time,
                 const int substeps,
                 const int constraint_iterations,
                 const float interpolation_begin,
                 const float interpolation_end,
                 const StringRef geometry_tag_filter,
                 const float4x4 &simulation_to_world)
      : world_(world),
        substeps_(substeps),
        sub_delta_time_(total_delta_time / substeps_),
        interpolation_begin_(interpolation_begin),
        interpolation_end_(interpolation_end),
        world_to_simulation_(math::invert(simulation_to_world)),
        geometry_tag_filter_(geometry_tag_filter),
        constraint_iterations_(constraint_iterations)
  {
    substep_compliance_factor_ = math::safe_rcp(pow2f(sub_delta_time_));
  }

  void do_step()
  {
    this->gather_nested_bundle_paths();
    this->gather_from_world__geometries();
    this->prepare_geometry_chunks();
    this->prepare_inverse_masses();
    this->prepare_inverse_moments_of_inertia();

    this->gather_from_world__mesh_colliders();
    this->gather_from_world__stretch_shear_constraints();
    this->gather_from_world__bend_twist_constraints();
    this->gather_from_world__edge_length_constraints();
    this->gather_from_world__cross_edge_length_constraints();
    this->gather_from_world__damping();
    this->gather_from_world__pin_positions();
    this->gather_from_world__pin_rotations();
    this->gather_from_world__collision_contacts();

    this->parallel_for_each_chunk(32, [&](const int chunk_i) {
      TLS &tls = tls_.local();
      this->create_chunk_constraints__rod_stretch_shear(tls, chunk_i);
      this->create_chunk_constraints__rod_bend_twist(tls, chunk_i);
      this->create_chunk_constraints__damping(tls, chunk_i);
      this->create_chunk_constraints__pin_positions(tls, chunk_i);
      this->create_chunk_constraints__pin_rotations(tls, chunk_i);
    });

    result_ = this->do_simulation();

    this->write_back__pin_positions();
    this->write_back__rod_stretch_shear();

    this->finish_common_attribute_writers();
    this->write_back_geometries_to_world();

    this->write_back__contacts();
  }

  const Result &result() const
  {
    return result_;
  }

  Span<std::pair<NodeWarningType, std::string>> warnings() const
  {
    return warnings_;
  }

 private:
  static float error_scale_from_threshold(const float error_threshold)
  {
    return 1.0f / std::max(math::square(error_threshold), 1e-12f);
  }

  void gather_nested_bundle_paths()
  {
    foreach_nested_bundle_item(world_,
                               [&](const Span<BundleKey> path, const BundleItemValue &value) {
                                 const BundlePtr *bundle_ptr = value.as_pointer<BundlePtr>();
                                 if (!bundle_ptr || !*bundle_ptr) {
                                   return;
                                 }
                                 const Bundle &bundle = **bundle_ptr;
                                 const std::optional<StringRef> type = bundle.type();
                                 if (!type.has_value()) {
                                   return;
                                 }
                                 nested_bundle_paths_.add_as(*type, Bundle::combine_path(path));
                               });
    /* Ensure the order is deterministic. */
    for (MutableSpan<std::string> paths : nested_bundle_paths_.values()) {
      std::ranges::sort(paths);
    }
  }

  void gather_from_world__geometries()
  {
    TLS &tls = tls_.local();
    /* Gather geometry sets to process from the world. */
    Vector<std::string> paths = gather_bundle_paths_by_data_type(world_, SOCK_GEOMETRY);
    /* Ensure the order is deterministic. */
    std::ranges::sort(paths);
    for (const StringRef path : paths) {
      GeometrySetData geo_set_data;
      geo_set_data.path = path;
      geo_set_data.geometry = std::move(*world_.lookup_path_for_write_ptr<GeometrySet>(path));
      if (geo_set_data.geometry.has_bundle()) {
        const Bundle &bundle_in_geo = *geo_set_data.geometry.bundle();
        if (const std::optional<GListPtr> tags_list_ptr = bundle_in_geo.lookup_path<GListPtr>(
                "tags"))
        {
          if (*tags_list_ptr) {
            const GList &tags_list = **tags_list_ptr;
            if (tags_list.cpp_type().is<std::string>()) {
              tags_list.typed<std::string>().foreach(
                  [&](const std::string &tag) { geo_set_data.tags.add(tag); });
            }
          }
        }
      }
      if (!tag_filter_matches(geometry_tag_filter_, geo_set_data.tags)) {
        continue;
      }
      geometries_.geometry_sets.append(std::move(geo_set_data));
    }

    /* Gather individual components that should be simulated. There may be more than geometry sets
     * because each geometry set could contain e.g. a mesh and curves. */
    for (const int geo_bundle_i : geometries_.geometry_sets.index_range()) {
      GeometrySetData &geo_set_data = geometries_.geometry_sets[geo_bundle_i];
      GeometrySet &geometry = geo_set_data.geometry;
      for (bke::GeometryComponent::Type type : {bke::GeometryComponent::Type::Mesh,
                                                bke::GeometryComponent::Type::PointCloud,
                                                bke::GeometryComponent::Type::Curve,
                                                bke::GeometryComponent::Type::Instance})
      {
        if (!geometry.has(type)) {
          continue;
        }
        bke::GeometryComponent &component = geometry.get_component_for_write(type);
        geometries_.data_keys.add_new({geo_bundle_i, type, std::nullopt});
        GeometryData &geo_data = tls.scope.construct<GeometryData>(
            *component.attributes_for_write());
        geo_data.curves = type == bke::GeometryComponent::Type::Curve ?
                              &geometry.get_curves_for_write()->geometry.wrap() :
                              nullptr;
        geo_data.domain = type == bke::GeometryComponent::Type::Instance ? AttrDomain::Instance :
                                                                           AttrDomain::Point;
        geometries_.data.append(&geo_data);
      }
      if (geometry.has_grease_pencil()) {
        using namespace blender::bke::greasepencil;
        GreasePencil &grease_pencil = *geometry.get_grease_pencil_for_write();
        for (const int layer_i : grease_pencil.layers().index_range()) {
          Layer &layer = grease_pencil.layer(layer_i);
          Drawing *drawing = grease_pencil.get_eval_drawing(layer);
          if (!drawing) {
            continue;
          }
          bke::CurvesGeometry &curves = drawing->strokes_for_write();
          geometries_.data_keys.add_new(
              {geo_bundle_i, bke::GeometryComponent::Type::Curve, layer_i});
          GeometryData &geo_data = tls.scope.construct<GeometryData>(
              curves.attributes_for_write());
          geo_data.domain = AttrDomain::Point;
          geo_data.curves = &curves;
          geometries_.data.append(&geo_data);
        }
      }
    }
    geometries_.total_points_num = 0;
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      const AttrDomain domain = geo_data.domain;
      geo_data.size = geo_data.attributes.domain_size(domain);
      geometries_.total_points_num += geo_data.size;
      geo_data.temp_positions.reinitialize(geo_data.size);
      geo_data.temp_rotations.reinitialize(geo_data.size);
      geo_data.position_attr = this->ensure_attribute<float3>(
          geo_data.attributes, attribute_names::position, domain);
      geo_data.velocity_attr = this->ensure_attribute<float3>(
          geo_data.attributes, attribute_names::velocity, domain);
      geo_data.external_force_attr = this->lookup_attribute_default<float3>(
          data_key_i, attribute_names::external_force, domain, float3(0, 0, 0));
      geo_data.masses = this->lookup_attribute_default<float>(
          data_key_i, attribute_names::mass, domain, 1.0f);

      geo_data.is_hard_pinned.reinitialize(geo_data.size);
      geo_data.is_hard_pinned.fill(false);
      if (geo_data.attributes.contains(attribute_names::rotation)) {
        geo_data.uses_rotation = true;
        geo_data.rotation_attr = this->ensure_attribute<math::Quaternion>(
            geo_data.attributes, attribute_names::rotation, domain);
        geo_data.angular_velocity_attr = this->ensure_attribute<float3>(
            geo_data.attributes, attribute_names::angular_velocity, domain);
        geo_data.external_torque_attr = this->lookup_attribute_default<float3>(
            data_key_i, attribute_names::external_torque, domain, float3(0, 0, 0));
        geo_data.moments_of_inertia = this->lookup_attribute_default<float3>(
            data_key_i, attribute_names::moment_of_inertia, domain, float3(1.0f));
      }
    }
  }

  void ensure_friction_loaded(const int data_key_i)
  {
    GeometryData &geo_data = *geometries_.data[data_key_i];
    geo_data.load_friction_mutex.ensure([&]() {
      geo_data.static_frictions = this->lookup_attribute_default<float>(
          data_key_i, attribute_names::static_friction, geo_data.domain, 0.0f);
      geo_data.dynamic_frictions = this->lookup_attribute_default<float>(
          data_key_i, attribute_names::dynamic_friction, geo_data.domain, 0.0f);
    });
  }

  void ensure_radius_loaded(const int data_key_i)
  {
    GeometryData &geo_data = *geometries_.data[data_key_i];
    geo_data.load_radius_mutex.ensure([&]() {
      geo_data.radii = this->lookup_attribute_default<float>(
          data_key_i, attribute_names::radius, geo_data.domain, 0.0f);
      geo_data.prev_radii = geo_data.radii;
    });
  }

  void gather_from_world__mesh_colliders()
  {
    const Span<std::string> paths = nested_bundle_paths_.lookup_as(MeshColliderBundle::name);
    for (const StringRef path : paths) {
      const BundlePtr *bundle_ptr = world_.lookup_path_ptr<BundlePtr>(path);
      if (!bundle_ptr || !*bundle_ptr) {
        continue;
      }
      const Bundle &bundle = **bundle_ptr;
      const Bundle *previous_bundle = this->get_previous_bundle(bundle);
      const bke::GeometrySet *geometry = bundle.lookup_ptr<bke::GeometrySet>(
          *BundleKey::from_str("geometry"));
      const float margin = bundle.lookup<float>(*BundleKey::from_str("margin")).value_or(0.0f);
      const float friction = bundle.lookup<float>(*BundleKey::from_str("friction")).value_or(0.0f);
      const float compliance =
          bundle.lookup<float>(*BundleKey::from_str("compliance")).value_or(0.0f);
      const bool deforming =
          bundle.lookup<bool>(*BundleKey::from_str("deforming")).value_or(false);
      const bool use_edge_contacts =
          bundle.lookup<bool>(*BundleKey::from_str("use_edge_contacts")).value_or(false);
      const bool is_boundary =
          bundle.lookup<bool>(*BundleKey::from_str("is_boundary")).value_or(false);
      const float error_threshold =
          bundle.lookup<float>(*BundleKey::from_str("error_threshold")).value_or(1e-3f);
      const bke::GeometrySet *prev_geometry = previous_bundle ?
                                                  previous_bundle->lookup_ptr<bke::GeometrySet>(
                                                      *BundleKey::from_str("geometry")) :
                                                  nullptr;
      if (!geometry) {
        continue;
      }
      Vector<int> affected_data;
      for (const int data_key_i : geometries_.data_keys.index_range()) {
        if (this->effector_applies_to_geometry(path, bundle, data_key_i)) {
          affected_data.append(data_key_i);
          this->ensure_friction_loaded(data_key_i);
          this->ensure_radius_loaded(data_key_i);
        }
      }
      Vector<int> instance_id_stack;
      this->gather_colliders_in_geometry(path,
                                         world_to_simulation_,
                                         world_to_simulation_,
                                         *geometry,
                                         prev_geometry,
                                         margin,
                                         friction,
                                         compliance,
                                         deforming,
                                         use_edge_contacts,
                                         is_boundary,
                                         error_threshold,
                                         affected_data,
                                         instance_id_stack);
    }
  }

  void gather_contacts__mesh_colliders(const int chunk_i,
                                       const float max_distance,
                                       const int solver_refs_i,
                                       const SubstepInterval &substep,
                                       const ExternalFaceContacts &prev_face_contacts,
                                       const ExternalEdgeContacts &prev_edge_contacts,
                                       ExternalFaceContacts &r_face_contacts,
                                       ExternalEdgeContacts &r_edge_contacts)
  {
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    const GeometryData &geo_data = *geometries_.data[chunk.data_key_i];
    for (const MeshColliderUsage &collider_usage : geo_data.mesh_colliders) {
      const MeshCollider &collider = constraints_.mesh_colliders[collider_usage.constraint_i];
      const float4x4 &mesh_to_local = math::interpolate(
          collider.begin_transform, collider.end_transform, substep.interpolate_end);
      const float4x4 &prev_mesh_to_local = math::interpolate(
          collider.begin_transform, collider.end_transform, substep.interpolate_begin);
      const float4x4 local_to_mesh = math::invert(mesh_to_local);

      if (std::holds_alternative<StaticMeshInfo>(collider.mesh)) {
        this->gather_contacts__mesh_collider(chunk_i,
                                             max_distance,
                                             solver_refs_i,
                                             false,
                                             substep,
                                             collider,
                                             collider_usage,
                                             mesh_to_local,
                                             prev_mesh_to_local,
                                             local_to_mesh,
                                             prev_face_contacts,
                                             prev_edge_contacts,
                                             r_face_contacts,
                                             r_edge_contacts);
      }
      else if (std::holds_alternative<DeformingMeshInfo>(collider.mesh)) {
        this->gather_contacts__mesh_collider(chunk_i,
                                             max_distance,
                                             solver_refs_i,
                                             true,
                                             substep,
                                             collider,
                                             collider_usage,
                                             mesh_to_local,
                                             prev_mesh_to_local,
                                             local_to_mesh,
                                             prev_face_contacts,
                                             prev_edge_contacts,
                                             r_face_contacts,
                                             r_edge_contacts);
      }
    }
  }

  void gather_contacts__mesh_collider(const int chunk_i,
                                      const float max_distance,
                                      const int solver_refs_i,
                                      const bool is_deforming,
                                      const SubstepInterval &substep,
                                      const MeshCollider &collider,
                                      const MeshColliderUsage &collider_usage,
                                      const float4x4 &mesh_to_local,
                                      const float4x4 &prev_mesh_to_local,
                                      const float4x4 &local_to_mesh,
                                      const ExternalFaceContacts &prev_face_contacts,
                                      const ExternalEdgeContacts &prev_edge_contacts,
                                      ExternalFaceContacts &r_face_contacts,
                                      ExternalEdgeContacts &r_edge_contacts)
  {
    const Mesh *mesh;
    const bke::BVHTreeFromMesh *corner_tris_bvh;
    const bke::BVHTreeFromMesh *edges_bvh;
    Span<float3> prev_vert_positions;
    if (is_deforming) {
      BLI_assert(std::holds_alternative<DeformingMeshInfo>(collider.mesh));
      const auto &deforming_mesh = std::get<DeformingMeshInfo>(collider.mesh);
      mesh = deforming_mesh.substep_meshes[substep.current_i + 1];
      corner_tris_bvh = &deforming_mesh.substep_corner_tris_bvh_trees[substep.current_i];
      edges_bvh = &deforming_mesh.substep_edges_bvh_trees[substep.current_i];
      prev_vert_positions = deforming_mesh.substep_meshes[substep.current_i]->vert_positions();
    }
    else {
      BLI_assert(std::holds_alternative<StaticMeshInfo>(collider.mesh));
      const auto &static_mesh = std::get<StaticMeshInfo>(collider.mesh);
      mesh = static_mesh.mesh;
      corner_tris_bvh = &static_mesh.corner_tris_bvh;
      edges_bvh = &static_mesh.edges_bvh;
    }
    const Span<float3> vert_positions = mesh->vert_positions();
    const Span<int2> edge_verts = mesh->edges();
    const Span<float3> vert_normals = mesh->vert_normals();
    const Span<int> corner_verts = mesh->corner_verts();
    const Span<int3> corner_tris = mesh->corner_tris();

    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    const GeometryData &geo_data = *geometries_.data[chunk.data_key_i];
    const Span<float3> positions =
        geometries_.solver_refs[solver_refs_i][chunk.data_key_i].positions;

    /* Only uniform scaling is correctly handled here. For non-uniform scaling, just the max
     * scale along one axis is used. */
    const float3 mesh_to_local_radius_scale = math::abs(math::to_scale(local_to_mesh));
    const float local_to_mesh_radius_factor = std::max({mesh_to_local_radius_scale.x,
                                                        mesh_to_local_radius_scale.y,
                                                        mesh_to_local_radius_scale.z});
    const float mesh_to_local_radius_factor = math::safe_rcp(local_to_mesh_radius_factor);
    const float margin_local = mesh_to_local_radius_factor * collider.margin;

    for (const int point_i : chunk.points_range) {
      if (geo_data.is_hard_pinned[point_i]) {
        continue;
      }
      const float3 &pos_local = positions[point_i];
      const float3 pos_mesh = math::transform_point(local_to_mesh, pos_local);
      const float radius_local = math::interpolate(
          geo_data.prev_radii[point_i], geo_data.radii[point_i], substep.interpolate_end);
      const float radius_mesh = local_to_mesh_radius_factor * radius_local;
      const std::optional<ClosestMeshFaceContact> contact = this->get_closest_mesh_face_contact(
          pos_mesh,
          *corner_tris_bvh,
          corner_tris,
          corner_verts,
          vert_positions,
          max_distance + radius_mesh);
      if (!contact) {
        continue;
      }

      const float3 contact_pos_local = math::transform_point(mesh_to_local, contact->nearest_pos);
      float3 prev_contact_pos_mesh;
      if (is_deforming) {
        const int3 &tri = corner_tris[contact->tri_i];
        prev_contact_pos_mesh = bke::attribute_math::mix3(
            contact->bary_coords,
            prev_vert_positions[corner_verts[tri[0]]],
            prev_vert_positions[corner_verts[tri[1]]],
            prev_vert_positions[corner_verts[tri[2]]]);
      }
      else {
        prev_contact_pos_mesh = contact->nearest_pos;
      }
      const float3 prev_contact_pos_local = math::transform_point(prev_mesh_to_local,
                                                                  prev_contact_pos_mesh);

      /* Separating axis to move self out of penetration. */
      const float3 collision_axis = (pos_local - contact_pos_local) *
                                    (contact->is_inside ? -1.0f : 1.0f) *
                                    (collider.is_boundary ? -1.0f : 1.0f);
      const float3 valid_axis = math::normalize(math::is_zero(collision_axis, 1e-6f) ?
                                                    math::transpose(float3x3(local_to_mesh)) *
                                                        contact->face_nor *
                                                        (collider.is_boundary ? -1.0f : 1.0f) :
                                                    collision_axis);

      const float static_friction = this->compute_contact_friction(
          geo_data.static_frictions[point_i], collider.friction);
      const float dynamic_friction = this->compute_contact_friction(
          geo_data.dynamic_frictions[point_i], collider.friction);

      const int contact_i = r_face_contacts.points.append_and_get_index(point_i);
      r_face_contacts.point_radii.append(radius_local);
      r_face_contacts.positions_on_face.append(contact_pos_local);
      r_face_contacts.collider_motion.append(contact_pos_local - prev_contact_pos_local);
      r_face_contacts.face_normals.append(valid_axis);
      r_face_contacts.face_margins.append(margin_local);
      r_face_contacts.static_frictions.append(static_friction);
      r_face_contacts.dynamic_frictions.append(dynamic_friction);
      r_face_contacts.compliance_terms.append(
          std::max(0.0f, substep_compliance_factor_ * collider.compliance));
      r_face_contacts.error_scales.append(error_scale_from_threshold(collider.error_threshold));

      const MeshContactId contact_id{collider_usage.constraint_i, point_i};
      r_face_contacts.mesh_contact_indices.add(contact_id, contact_i);
      r_face_contacts.init_or_preserve_state(
          prev_face_contacts, prev_face_contacts.mesh_contact_indices.lookup_try(contact_id));
    }

    if (collider.use_edge_contacts) {
      auto handle_edge = [&](const int geo_contact_id, const int point0, const int point1) {
        if (geo_data.is_hard_pinned[point0] && geo_data.is_hard_pinned[point1]) {
          return;
        }
        const float3 &pos_local0 = positions[point0];
        const float3 &pos_local1 = positions[point1];
        const float3 pos_mesh0 = math::transform_point(local_to_mesh, pos_local0);
        const float3 pos_mesh1 = math::transform_point(local_to_mesh, pos_local1);
        const float radius_local0 = math::interpolate(
            geo_data.prev_radii[point0], geo_data.radii[point0], substep.interpolate_end);
        const float radius_local1 = math::interpolate(
            geo_data.prev_radii[point1], geo_data.radii[point1], substep.interpolate_end);
        /* Use max radius for collision detection. */
        const float max_radius_mesh = local_to_mesh_radius_factor *
                                      std::max(radius_local0, radius_local1);
        const std::optional<ClosestMeshEdgeContact> contact = this->get_closest_mesh_edge_contact(
            pos_mesh0,
            pos_mesh1,
            *edges_bvh,
            edge_verts,
            vert_positions,
            max_distance + max_radius_mesh);
        if (!contact) {
          return;
        }

        /* Add a contact for each face adjacent to the closest edge. */
        const int2 &edge = edge_verts[contact->edge_i];

        const float3 contact_pos_local = math::transform_point(mesh_to_local,
                                                               contact->nearest_pos);
        float3 prev_contact_pos_mesh;
        if (is_deforming) {
          prev_contact_pos_mesh = bke::attribute_math::mix2(
              contact->edge_factor, prev_vert_positions[edge[0]], prev_vert_positions[edge[1]]);
        }
        else {
          prev_contact_pos_mesh = contact->nearest_pos;
        }
        const float3 prev_contact_pos_local = math::transform_point(prev_mesh_to_local,
                                                                    prev_contact_pos_mesh);

        const float3 edge_direction_mesh = math::normalize(vert_positions[edge[1]] -
                                                           vert_positions[edge[0]]);
        const float3 edge_direction_local = math::normalize(
            math::transform_direction(mesh_to_local, edge_direction_mesh));
        /* Contact with the closest edge is active if the segment intersects with the
         * half-plane defined by the edge normal. A segment intersecting with any of the
         * adjacent faces would need to be moved along a tangent of the respective face to
         * resolve the collision, but since in combination with point-face collisions it is
         * sufficient to define a single edge normal, as long as the half-plane it defines is
         * inside both of the half-planes of the adjacent faces, i.e. between the two normal
         * directions. */
        const float3 averaged_vert_normal_mesh = math::midpoint(vert_normals[edge[0]],
                                                                vert_normals[edge[1]]);
        /* Transforming normal with transpose of inverse. */
        const float3 averaged_vert_normal_local = math::transform_direction(
            math::transpose(local_to_mesh), averaged_vert_normal_mesh);
        const float3 edge_normal_local = math::normalize(math::cross(
            math::cross(edge_direction_local, averaged_vert_normal_local), edge_direction_local));
        BLI_assert(math::is_unit(math::cross(edge_direction_local, edge_normal_local)));

        /* Use geometric average as edge friction coefficient. */
        const float static_friction = math::sqrt(
            math::square(this->compute_contact_friction(geo_data.static_frictions[point0],
                                                        collider.friction)) +
            math::square(this->compute_contact_friction(geo_data.static_frictions[point1],
                                                        collider.friction)));
        const float dynamic_friction = math::sqrt(
            math::square(this->compute_contact_friction(geo_data.dynamic_frictions[point0],
                                                        collider.friction)) +
            math::square(this->compute_contact_friction(geo_data.dynamic_frictions[point1],
                                                        collider.friction)));

        const int contact_i = r_edge_contacts.point_pairs.append_and_get_index({point0, point1});
        r_edge_contacts.point_radii.append({radius_local0, radius_local1});
        r_edge_contacts.positions_on_edge.append(contact_pos_local);
        r_edge_contacts.collider_motion.append(contact_pos_local - prev_contact_pos_local);
        r_edge_contacts.edge_directions.append(edge_direction_local);
        r_edge_contacts.edge_normals.append(edge_normal_local *
                                            (collider.is_boundary ? -1.0f : 1.0f));
        r_edge_contacts.edge_margins.append(margin_local);
        r_edge_contacts.static_frictions.append(static_friction);
        r_edge_contacts.dynamic_frictions.append(dynamic_friction);
        r_edge_contacts.compliance_terms.append(
            std::max(0.0f, substep_compliance_factor_ * collider.compliance));
        r_edge_contacts.error_scales.append(error_scale_from_threshold(collider.error_threshold));

        const MeshContactId contact_id{collider_usage.constraint_i, geo_contact_id};
        r_edge_contacts.mesh_contact_indices.add(contact_id, contact_i);
        r_edge_contacts.init_or_preserve_state(
            prev_edge_contacts, prev_edge_contacts.mesh_contact_indices.lookup_try(contact_id));
      };

      if (chunk.curves_range) {
        const OffsetIndices points_by_curve = geo_data.curves->points_by_curve();
        for (const int curve : *chunk.curves_range) {
          for (const int point0 : points_by_curve[curve].drop_back(1)) {
            const int geo_contact_id = point0;
            handle_edge(geo_contact_id, point0, point0 + 1);
          }
        }
      }
    }
  }

  void gather_from_world__collision_contacts()
  {
    const Span<std::string> paths = nested_bundle_paths_.lookup(CollisionContactsBundle::name);
    for (const StringRef path : paths) {
      const BundlePtr *bundle_ptr = world_.lookup_path_ptr<BundlePtr>(path);
      if (!bundle_ptr || !*bundle_ptr) {
        continue;
      }

      constraints_.collision_contacts.append({path});
    }
  }

  struct ClosestMeshFaceContact {
    /** The nearest position exactly on the mesh surface. */
    float3 nearest_pos;
    float3 face_nor;
    float3 bary_coords;
    /** This does not take the radius into account. */
    bool is_inside;
    int tri_i;
  };

  struct ClosestMeshEdgeContact {
    /** The nearest position exactly on the mesh edge. */
    float3 nearest_pos;
    float edge_factor;
    int edge_i;
  };

  std::optional<ClosestMeshFaceContact> get_closest_mesh_face_contact(
      const float3 &sample_pos,
      const bke::BVHTreeFromMesh &corner_tris_bvh,
      const Span<int3> corner_tris,
      const Span<int> corner_verts,
      const Span<float3> vert_positions,
      const float max_distance) const
  {
    BVHTreeNearest nearest{};
    nearest.index = -1;
    nearest.dist_sq = pow2f(max_distance);
    BLI_bvhtree_find_nearest(corner_tris_bvh.tree,
                             sample_pos,
                             &nearest,
                             corner_tris_bvh.nearest_callback,
                             (void *)&corner_tris_bvh);
    if (nearest.index == -1) {
      return std::nullopt;
    }
    const int tri_i = nearest.index;
    const int3 &tri = corner_tris[tri_i];
    const float3 &contact_pos = float3(nearest.co);
    const float3 &contact_nor = float3(nearest.no);
    const float3 bary_coords = bke::mesh_surface_sample::compute_bary_coord_in_triangle(
        vert_positions, corner_verts, tri, contact_pos);
    const float3 direction_to_mesh = contact_pos - sample_pos;
    bool is_inside;
    if (this->is_bary_coord_close_to_edge(bary_coords)) {
      /* The nearest point is on an edge, so its normal is unreliable, use a more robust test. */
      is_inside = this->is_inside_ray_using_rays(sample_pos, corner_tris_bvh, direction_to_mesh);
    }
    else {
      is_inside = math::dot(direction_to_mesh, contact_nor) > 0.0f;
    }
    return ClosestMeshFaceContact{contact_pos, contact_nor, bary_coords, is_inside, tri_i};
  }

  std::optional<ClosestMeshEdgeContact> get_closest_mesh_edge_contact(
      const float3 &sample_pos0,
      const float3 &sample_pos1,
      const bke::BVHTreeFromMesh &edges_bvh,
      const Span<int2> edges,
      const Span<float3> vert_positions,
      const float max_distance) const
  {
    struct UserData {
      Span<int2> edges;
      Span<float3> vert_positions;
      float3 sample_pos0;
      float3 sample_pos1;
    };

    const auto closest_edge_cb =
        [](void *user_data, int index, const BVHTreeRay * /*ray*/, BVHTreeRayHit *hit) {
          const auto &data = *static_cast<const UserData *>(user_data);

          const int2 &edge = data.edges[index];
          const float3 &v0 = data.vert_positions[edge[0]];
          const float3 &v1 = data.vert_positions[edge[1]];

          float3 closest_on_ray, closest_on_edge;
          isect_seg_seg_v3(
              data.sample_pos0, data.sample_pos1, v0, v1, closest_on_ray, closest_on_edge);
          const float dist_squared = math::distance_squared(closest_on_ray, closest_on_edge);
          if (dist_squared <= math::square(hit->dist)) {
            hit->dist = math::sqrt(dist_squared);
            hit->index = index;
            copy_v3_v3(hit->co, closest_on_edge);
          }
        };

    float ray_len;
    const float3 ray_dir = math::normalize_and_get_length(sample_pos1 - sample_pos0, ray_len);

    UserData user_data = {edges, vert_positions, sample_pos0, sample_pos1};
    /* Needs to be initialized. */
    BVHTreeRayHit hit;
    hit.index = -1;
    hit.dist = ray_len;
    BLI_bvhtree_ray_cast_ex(
        edges_bvh.tree, sample_pos0, ray_dir, max_distance, &hit, closest_edge_cb, &user_data, 0);
    if (hit.index == -1) {
      return std::nullopt;
    }

    const int edge_i = hit.index;
    const int2 &edge = edges[edge_i];
    const float3 &contact_pos = float3(hit.co);
    const float3 v0 = vert_positions[edge[0]];
    const float3 v1 = vert_positions[edge[1]];
    const float edge_factor = math::sqrt(math::safe_divide(math::distance_squared(contact_pos, v0),
                                                           math::distance_squared(v1, v0)));
    return ClosestMeshEdgeContact{contact_pos, edge_factor, edge_i};
  }

  bool is_bary_coord_close_to_edge(const float3 &bary_coords) const
  {
    constexpr float epsilon = 1e-3f;
    return math::abs(bary_coords[0]) < epsilon || math::abs(bary_coords[1]) < epsilon ||
           math::abs(bary_coords[2]) < epsilon;
  }

  bool is_inside_ray_using_rays(const float3 &pos,
                                const bke::BVHTreeFromMesh &bvh,
                                const float3 &approx_ray_direction) const
  {
    /* Shoot rays in the approximate direction of where the nearest point is. This is a heuristic
     * for better performance to make the rays shorter. */
    const float3 approx_ray_direction_normalized = math::normalize(approx_ray_direction);
    const std::array<float3, 3> dirs = {
        {math::normalize(approx_ray_direction_normalized + float3{0.125f, -0.164f, 0.172f}),
         math::normalize(approx_ray_direction_normalized + float3{0.176f, 0.197f, -0.176f}),
         math::normalize(approx_ray_direction_normalized + float3{-0.140f, 0.151f, -0.126f})}};
    int inside_count = 0;
    for (const float3 &dir : dirs) {
      BVHTreeRayHit hit{};
      hit.index = -1;
      hit.dist = FLT_MAX;
      BLI_bvhtree_ray_cast(bvh.tree, pos, dir, 0.0f, &hit, bvh.raycast_callback, (void *)&bvh);
      if (hit.index != -1) {
        const float3 dir = float3(hit.co) - pos;
        const bool is_inside = math::dot(dir, float3(hit.no)) > 0.0f;
        inside_count += is_inside ? 1 : -1;
        if (std::abs(inside_count) >= 2) {
          break;
        }
      }
      else {
        return false;
      }
    }
    return inside_count > 0;
  }

  void gather_colliders_in_geometry(const StringRef path,
                                    const float4x4 &transform,
                                    const float4x4 &prev_transform,
                                    const GeometrySet &collider_geo,
                                    const GeometrySet *prev_collider_geo,
                                    const float margin,
                                    const float friction,
                                    const float compliance,
                                    const bool deforming,
                                    const bool use_edge_contacts,
                                    const bool is_boundary,
                                    const float error_threshold,
                                    const Span<int> affected_data,
                                    Vector<int> &instance_id_stack)
  {
    if (const Mesh *mesh = collider_geo.get_mesh()) {
      if (mesh->faces_num > 0) {
        MeshCollider mesh_collider;
        mesh_collider.path = path;
        mesh_collider.instance_ids = instance_id_stack;
        mesh_collider.margin = margin;
        mesh_collider.friction = friction;
        mesh_collider.compliance = compliance;
        mesh_collider.use_edge_contacts = use_edge_contacts;
        mesh_collider.is_boundary = is_boundary;
        mesh_collider.error_threshold = error_threshold;
        mesh_collider.begin_transform = prev_transform;
        mesh_collider.end_transform = transform;
        const Mesh *prev_mesh = prev_collider_geo ? prev_collider_geo->get_mesh() : nullptr;
        mesh_collider.mesh = this->make_collider_mesh_info(*mesh, prev_mesh, deforming);

        const int collider_i = constraints_.mesh_colliders.append_and_get_index(
            std::move(mesh_collider));
        for (const int data_key_i : affected_data) {
          geometries_.data[data_key_i]->mesh_colliders.append({collider_i});
        }
      }
    }
    if (const bke::Instances *instances = collider_geo.get_instances()) {
      struct PrevInstanceItem {
        const float4x4 *transform;
        const bke::InstanceReference *reference;
      };
      Map<int, PrevInstanceItem> prev_instance_by_id;
      if (prev_collider_geo) {
        const bke::Instances *prev_instances = prev_collider_geo->get_instances();
        if (prev_instances) {
          const Span<float4x4> prev_instance_transforms = prev_instances->transforms();
          const Span<int> prev_instance_ids = prev_instances->unique_ids();
          const Span<bke::InstanceReference> prev_references = prev_instances->references();
          for (const int i : prev_instance_transforms.index_range()) {
            const int prev_instance_id = prev_instance_ids[i];
            const float4x4 &prev_instance_transform = prev_instance_transforms[i];
            const bke::InstanceReference &prev_reference = prev_references[i];
            prev_instance_by_id.add(prev_instance_id, {&prev_instance_transform, &prev_reference});
          }
        }
      }

      const Span<float4x4> instance_transforms = instances->transforms();
      const Span<bke::InstanceReference> references = instances->references();
      const Span<int> handles = instances->reference_handles();
      const Span<int> instance_ids = instances->unique_ids();
      for (const int instance_i : instance_transforms.index_range()) {
        const int handle = handles[instance_i];
        if (!references.index_range().contains(handle)) {
          continue;
        }
        const int instance_id = instance_ids[instance_i];
        const float4x4 instance_transform = instance_transforms[instance_i];
        const PrevInstanceItem *prev_instance_item = prev_instance_by_id.lookup_ptr(instance_id);
        const bke::InstanceReference &reference = references[handle];
        GeometrySet reference_geo;
        reference.to_geometry_set(reference_geo);
        GeometrySet prev_reference_geo;
        float4x4 prev_instance_transform = instance_transform;
        if (prev_instance_item) {
          prev_instance_item->reference->to_geometry_set(prev_reference_geo);
          prev_instance_transform = *prev_instance_item->transform;
        }

        instance_id_stack.append(instance_id);
        BLI_SCOPED_DEFER([&]() { instance_id_stack.pop_last(); });
        this->gather_colliders_in_geometry(path,
                                           transform * instance_transform,
                                           prev_transform * prev_instance_transform,
                                           reference_geo,
                                           prev_instance_item ? &prev_reference_geo : nullptr,
                                           margin,
                                           friction,
                                           compliance,
                                           deforming,
                                           use_edge_contacts,
                                           is_boundary,
                                           error_threshold,
                                           affected_data,
                                           instance_id_stack);
      }
    }
  }

  std::variant<StaticMeshInfo, DeformingMeshInfo> make_collider_mesh_info(const Mesh &mesh,
                                                                          const Mesh *prev_mesh,
                                                                          const bool deforming)
  {
    if (!deforming || !prev_mesh) {
      return StaticMeshInfo{&mesh, mesh.bvh_corner_tris(), mesh.bvh_edges()};
    }
    if (mesh.verts_num != prev_mesh->verts_num) {
      return StaticMeshInfo{&mesh, mesh.bvh_corner_tris(), mesh.bvh_edges()};
    }
    const int verts_num = mesh.verts_num;
    DeformingMeshInfo result;
    result.substep_meshes.resize(substeps_ + 1);
    result.substep_corner_tris_bvh_trees.resize(substeps_);
    result.substep_edges_bvh_trees.resize(substeps_);

    const Span<float3> begin_positions = prev_mesh->vert_positions();
    const Span<float3> end_positions = mesh.vert_positions();

    threading::parallel_for(
        IndexRange(substeps_ + 1), std::max(1024 / verts_num, 1), [&](const IndexRange range) {
          TLS &tls = tls_.local();
          for (const int mesh_i : range) {
            const float mix_factor = (mesh_i - 1) / float(substeps_) *
                                         (interpolation_end_ - interpolation_begin_) +
                                     interpolation_begin_;
            const Mesh *substep_mesh = nullptr;
            if (mix_factor == 0.0f && mesh_i == 0) {
              substep_mesh = prev_mesh;
            }
            else if (mix_factor == 1.0f) {
              substep_mesh = &mesh;
            }
            else {
              Mesh *interpolated_mesh = BKE_mesh_copy_for_eval(mesh);
              tls.scope.add_destruct_call(
                  [interpolated_mesh]() { BKE_id_free(nullptr, interpolated_mesh); });
              MutableSpan<float3> substep_positions =
                  interpolated_mesh->vert_positions_for_write();
              for (const int i : IndexRange(verts_num)) {
                substep_positions[i] = math::interpolate(
                    begin_positions[i], end_positions[i], mix_factor);
              }
              interpolated_mesh->tag_positions_changed();
              substep_mesh = interpolated_mesh;
            }
            result.substep_meshes[mesh_i] = substep_mesh;
            if (mesh_i > 0) {
              /* The bvh tree is not needed for the first substep. */
              result.substep_corner_tris_bvh_trees[mesh_i - 1] = substep_mesh->bvh_corner_tris();
              result.substep_edges_bvh_trees[mesh_i - 1] = substep_mesh->bvh_edges();
            }
          }
        });
    return result;
  }

  void prepare_geometry_chunks()
  {
    constexpr int approx_points_per_chunk = 256;
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      if (geo_data.size == 0) {
        continue;
      }
      const int old_chunks_num = geometries_.chunks.size();

      if (geo_data.curves) {
        /* For curve data, align the ranges with curve boundaries. */
        const bke::CurvesGeometry &curves = *geo_data.curves;
        const OffsetIndices<int> points_by_curve = curves.points_by_curve();
        Stack<IndexRange> curve_ranges_to_check;
        curve_ranges_to_check.push(curves.curves_range());
        while (!curve_ranges_to_check.is_empty()) {
          const IndexRange curves_range = curve_ranges_to_check.pop();
          const IndexRange points_range = points_by_curve[curves_range];
          if (points_range.is_empty()) {
            continue;
          }
          if (curves_range.size() == 1 || points_range.size() <= approx_points_per_chunk) {
            geometries_.chunks.append({data_key_i, points_range, curves_range});
            continue;
          }
          const int split_pos = curves_range.size() / 2;
          const IndexRange curves_range_left = curves_range.take_front(split_pos);
          const IndexRange curves_range_right = curves_range.drop_front(split_pos);
          /* Pushing in this order ensures that the chunks are sorted in the end. */
          curve_ranges_to_check.push(curves_range_right);
          curve_ranges_to_check.push(curves_range_left);
        }
      }
      else {
        const IndexRange points_range = IndexRange(geo_data.size);
        Stack<IndexRange> ranges_to_check;
        ranges_to_check.push(points_range);
        while (!ranges_to_check.is_empty()) {
          const IndexRange points_range = ranges_to_check.pop();
          if (points_range.is_empty()) {
            continue;
          }
          if (points_range.size() <= approx_points_per_chunk) {
            geometries_.chunks.append({data_key_i, points_range});
            continue;
          }
          const int split_pos = points_range.size() / 2;
          const IndexRange points_range_left = points_range.take_front(split_pos);
          const IndexRange points_range_right = points_range.drop_front(split_pos);
          /* Pushing in this order ensures that the chunks are sorted in the end. */
          ranges_to_check.push(points_range_right);
          ranges_to_check.push(points_range_left);
        }
      }
      geo_data.chunks = IndexRange::from_begin_end(old_chunks_num, geometries_.chunks.size());

      /* Create mapping from points to chunks. */
      geo_data.point_to_chunk.reinitialize(geo_data.size);
      for (const int chunk_i : geo_data.chunks) {
        const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
        geo_data.point_to_chunk.as_mutable_span().slice(chunk.points_range).fill(chunk_i);
      }
    }
    chunks_data_.reinitialize(geometries_.chunks.size());
    for (const int chunk_i : geometries_.chunks.index_range()) {
      const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
      geometries_.max_chunk_size = std::max<int>(geometries_.max_chunk_size,
                                                 chunk.points_range.size());
    }
  }

  void prepare_inverse_masses()
  {
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      geo_data.inv_masses.reinitialize(geo_data.size);
      MutableSpan<float> inv_masses = geo_data.inv_masses;

      if (const std::optional<float> mass = geo_data.masses.get_if_single()) {
        inv_masses.fill(math::safe_rcp(*mass));
      }
      else {
        const VArraySpan<float> masses = geo_data.masses;
        threading::parallel_for(IndexRange(geo_data.size), 2048, [&](const IndexRange range) {
          for (const int i : range) {
            const float mass = masses[i];
            inv_masses[i] = math::safe_rcp(mass);
          }
        });
      }
    }
  }

  void prepare_inverse_moments_of_inertia()
  {
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      if (!geo_data.uses_rotation) {
        continue;
      }
      geo_data.inv_moments_of_inertia.reinitialize(geo_data.size);
      MutableSpan<float3> inv_moments_of_inertia = geo_data.inv_moments_of_inertia;

      threading::parallel_for(IndexRange(geo_data.size), 2048, [&](const IndexRange range) {
        for (const int i : range) {
          const float3 &moment_of_inertia = geo_data.moments_of_inertia[i];
          if (math::is_zero(moment_of_inertia)) {
            inv_moments_of_inertia[i] = float3(0.0f);
          }
          else {
            inv_moments_of_inertia[i] = math::safe_rcp(moment_of_inertia);
          }
        }
      });
    }
  }

  void gather_from_world__stretch_shear_constraints()
  {
    TLS &tls = tls_.local();
    const Span<std::string> paths = nested_bundle_paths_.lookup(RodStretchShearBundle::name);
    for (const StringRef path : paths) {
      const Bundle &bundle = **world_.lookup_path_ptr<BundlePtr>(path);
      RodStretchShearConstraint constraint;
      constraint.path = path;
      constraint.error_threshold =
          bundle.lookup<float>(*BundleKey::from_str("error_threshold")).value_or(1e-3f);
      constraint.lambda_pos_attr = bundle
                                       .lookup<std::string>(
                                           *BundleKey::from_str("lambda_position_attribute"))
                                       .value_or("");
      constraint.lambda_rot_attr = bundle
                                       .lookup<std::string>(
                                           *BundleKey::from_str("lambda_rotation_attribute"))
                                       .value_or("");

      const int constraint_i = constraints_.rod_stretch_shear_constraints.append_and_get_index(
          std::move(constraint));

      for (const int data_key_i : geometries_.data_keys.index_range()) {
        GeometryData &geo_data = *geometries_.data[data_key_i];
        if (!geo_data.curves) {
          continue;
        }
        if (!geo_data.uses_rotation) {
          this->report(NodeWarningType::Error,
                       TIP_("Rod stretch/shear constraint requires \"rotation\" attribute"));
          continue;
        }
        if (!this->effector_applies_to_geometry(path, bundle, data_key_i)) {
          continue;
        }
        geo_data.rod_stretch_shear_constraints.append({constraint_i});
      }
    }
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      for (RodStretchShearConstraintUsage &constraint_usage :
           geo_data.rod_stretch_shear_constraints)
      {
        const RodStretchShearConstraint &constraint =
            constraints_.rod_stretch_shear_constraints[constraint_usage.constraint_i];
        VArray<float> rest_lengths = this->lookup_attribute_required<float>(
            data_key_i, this->prop_attr_name(constraint.path, "rest_length"), geo_data.domain);
        if (!rest_lengths) {
          continue;
        }

        constraint_usage.compliances = this->make_range_spans(
            tls,
            this->lookup_attribute_default<float>(
                data_key_i,
                this->prop_attr_name(constraint.path, "compliance"),
                geo_data.domain,
                0.0f));
        constraint_usage.rest_lengths = std::move(rest_lengths);
        constraint_usage.is_valid = true;

        constraint_usage.lambdas_pos = tls.allocator.allocate_array<float3>(geo_data.size);
        constraint_usage.lambdas_rot = tls.allocator.allocate_array<float3>(geo_data.size);
      }
    }
  }

  void create_chunk_constraints__rod_stretch_shear(TLS &tls, const int chunk_i)
  {
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    ChunkData &chunk_data = chunks_data_[chunk_i];
    const GeometryData &geo_data = *geometries_.data[chunk.data_key_i];
    if (geo_data.rod_stretch_shear_constraints.is_empty()) {
      return;
    }
    const bke::CurvesGeometry &curves = *geo_data.curves;
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    for (const RodStretchShearConstraintUsage &constraint_usage :
         geo_data.rod_stretch_shear_constraints)
    {
      if (!constraint_usage.is_valid) {
        continue;
      }
      const RodStretchShearConstraint &constraint =
          constraints_.rod_stretch_shear_constraints[constraint_usage.constraint_i];
      chunk_data.static_constraints.append(
          &tls.scope.construct<xpbd::RodStretchAndShearConstraintSet>(
              chunk.data_key_i,
              *chunk.curves_range,
              points_by_curve,
              constraint_usage.rest_lengths,
              constraint_usage.compliances.get_span_for_range(chunk.points_range),
              error_scale_from_threshold(constraint.error_threshold),
              constraint_usage.lambdas_pos,
              constraint_usage.lambdas_rot));
    }
  }

  void write_back__rod_stretch_shear()
  {
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      for (const RodStretchShearConstraintUsage &constraint_usage :
           geo_data.rod_stretch_shear_constraints)
      {
        if (!constraint_usage.is_valid) {
          continue;
        }
        const RodStretchShearConstraint &constraint =
            constraints_.rod_stretch_shear_constraints[constraint_usage.constraint_i];
        geo_data.attributes.remove(constraint.lambda_pos_attr);
        geo_data.attributes.remove(constraint.lambda_rot_attr);
        if (bke::SpanAttributeWriter<float3> lambda_pos_attr =
                this->get_output_attribute_writer<float3>(
                    data_key_i, constraint.lambda_pos_attr, geo_data.domain))
        {
          lambda_pos_attr.span.copy_from(constraint_usage.lambdas_pos);
          lambda_pos_attr.finish();
        }
        if (bke::SpanAttributeWriter<float3> lambda_rot_attr =
                this->get_output_attribute_writer<float3>(
                    data_key_i, constraint.lambda_rot_attr, geo_data.domain))
        {
          lambda_rot_attr.span.copy_from(constraint_usage.lambdas_rot);
          lambda_rot_attr.finish();
        }
      }
    }
  }

  void gather_from_world__bend_twist_constraints()
  {
    TLS &tls = tls_.local();
    const Span<std::string> paths = nested_bundle_paths_.lookup(RodBendTwistBundle::name);
    for (const StringRef path : paths) {
      const Bundle &bundle = **world_.lookup_path_ptr<BundlePtr>(path);

      RodBendTwistConstraint constraint;
      constraint.path = path;
      constraint.error_threshold =
          bundle.lookup<float>(*BundleKey::from_str("error_threshold")).value_or(1e-2f);

      const int constraint_i = constraints_.rod_bend_twist_constraints.append_and_get_index(
          std::move(constraint));

      for (const int data_key_i : geometries_.data.index_range()) {
        GeometryData &geo_data = *geometries_.data[data_key_i];
        if (!geo_data.curves) {
          continue;
        }
        if (!geo_data.uses_rotation) {
          this->report(NodeWarningType::Error,
                       TIP_("Rod stretch/shear constraint requires \"rotation\" attribute"));
          continue;
        }
        if (!this->effector_applies_to_geometry(path, bundle, data_key_i)) {
          continue;
        }
        geo_data.rod_bend_twist_constraints.append({constraint_i});
      }
    }
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      for (RodBendTwistConstraintUsage &constraint_usage : geo_data.rod_bend_twist_constraints) {
        const RodBendTwistConstraint &constraint =
            constraints_.rod_bend_twist_constraints[constraint_usage.constraint_i];
        constraint_usage.lambdas = tls.allocator.allocate_array<float4>(geo_data.size);
        constraint_usage.compliances = this->make_range_spans(
            tls,
            this->lookup_attribute_default<float>(
                data_key_i,
                this->prop_attr_name(constraint.path, "compliance"),
                geo_data.domain,
                0.0f));
        constraint_usage.rest_bend_rotations = this->lookup_attribute_default<math::Quaternion>(
            data_key_i,
            this->prop_attr_name(constraint.path, "rest_bend_rotation"),
            geo_data.domain,
            math::Quaternion::identity());
      }
    }
  }

  void create_chunk_constraints__rod_bend_twist(TLS &tls, const int chunk_i)
  {
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    ChunkData &chunk_data = chunks_data_[chunk_i];
    const GeometryData &geo_data = *geometries_.data[chunk.data_key_i];
    if (geo_data.rod_bend_twist_constraints.is_empty()) {
      return;
    }
    const bke::CurvesGeometry &curves = *geo_data.curves;
    const OffsetIndices<int> points_by_curve = curves.points_by_curve();
    for (const RodBendTwistConstraintUsage &constraint_usage : geo_data.rod_bend_twist_constraints)
    {
      const RodBendTwistConstraint &constraint =
          constraints_.rod_bend_twist_constraints[constraint_usage.constraint_i];
      chunk_data.static_constraints.append(
          &tls.scope.construct<xpbd::RodBendAndTwistConstraintSet>(
              chunk.data_key_i,
              *chunk.curves_range,
              points_by_curve,
              constraint_usage.rest_bend_rotations,
              constraint_usage.compliances.get_span_for_range(chunk.points_range),
              error_scale_from_threshold(constraint.error_threshold),
              constraint_usage.lambdas));
    }
  }

  void gather_from_world__edge_length_constraints()
  {
    TLS &tls = tls_.local();
    const Span<std::string> paths = nested_bundle_paths_.lookup(EdgeLengthConstraintBundle::name);
    for (const StringRef path : paths) {
      const Bundle &bundle = **world_.lookup_path_ptr<BundlePtr>(path);

      const float error_threshold =
          bundle.lookup<float>(*BundleKey::from_str("error_threshold")).value_or(1e-3f);
      const int constraint_i = constraints_.edge_length_constraints.append_and_get_index(
          {path, error_threshold});

      for (const int data_key_i : geometries_.data.index_range()) {
        const DataKey &data_key = geometries_.data_keys[data_key_i];
        GeometryData &geo_data = *geometries_.data[data_key_i];
        if (data_key.type != GeometryComponent::Type::Mesh) {
          continue;
        }
        if (this->effector_applies_to_geometry(path, bundle, data_key_i)) {
          EdgeLengthConstraintUsage constraint_usage;
          constraint_usage.constraint_i = constraint_i;
          geo_data.edge_length_constraints.append(std::move(constraint_usage));
        }
      }
    }

    for (const int data_key_i : geometries_.data_keys.index_range()) {
      const DataKey &data_key = geometries_.data_keys[data_key_i];
      GeometryData &geo_data = *geometries_.data[data_key_i];
      for (EdgeLengthConstraintUsage &constraint_usage : geo_data.edge_length_constraints) {
        const EdgeLengthConstraint &constraint =
            constraints_.edge_length_constraints[constraint_usage.constraint_i];
        const Mesh &mesh = *geometries_.geometry_sets[data_key.geo_bundle_i].geometry.get_mesh();
        const Span<int2> edges = mesh.edges();
        const int edge_num = edges.size();

        VArray<float> rest_lengths = this->lookup_attribute_required<float>(
            data_key_i, this->prop_attr_name(constraint.path, "rest_length"), AttrDomain::Edge);
        if (!rest_lengths) {
          continue;
        }

        constraint_usage.lambdas = tls.allocator.allocate_array<float>(edge_num);
        constraint_usage.rest_lengths = rest_lengths;
        constraint_usage.compliances = this->lookup_attribute_default<float>(
            data_key_i,
            this->prop_attr_name(constraint.path, "compliance"),
            AttrDomain::Edge,
            0.0f);
        constraint_usage.is_valid = true;

        auto &constraint_set = tls.scope.construct<xpbd::DistanceConstraintSet>(
            data_key_i,
            edges,
            constraint_usage.rest_lengths,
            constraint_usage.compliances,
            error_scale_from_threshold(constraint.error_threshold),
            constraint_usage.lambdas);
        xpbd::ConstraintColoring coloring = constraint_set.color_constraints(tls.mask_memory);
        geo_data.static_constraints.append({&constraint_set, std::move(coloring)});
      }
    }
  }

  void gather_from_world__cross_edge_length_constraints()
  {
    TLS &tls = tls_.local();
    const Span<std::string> paths = nested_bundle_paths_.lookup_as(
        CrossEdgeLengthConstraintBundle::name);
    for (const StringRef path : paths) {
      const Bundle &bundle = **world_.lookup_path_ptr<BundlePtr>(path);
      const float error_threshold =
          bundle.lookup<float>(*BundleKey::from_str("error_threshold")).value_or(1e-3f);
      const int constraint_i = constraints_.cross_edge_length_constraints.append_and_get_index(
          {path, error_threshold});
      for (const int data_key_i : geometries_.data.index_range()) {
        const DataKey &data_key = geometries_.data_keys[data_key_i];
        GeometryData &geo_data = *geometries_.data[data_key_i];
        if (data_key.type != GeometryComponent::Type::Mesh) {
          continue;
        }
        if (this->effector_applies_to_geometry(path, bundle, data_key_i)) {
          CrossEdgeLengthConstraintUsage constraint_usage;
          constraint_usage.constraint_i = constraint_i;
          geo_data.cross_edge_length_constraints.append(std::move(constraint_usage));
        }
      }
    }

    for (const int data_key_i : geometries_.data_keys.index_range()) {
      const DataKey &data_key = geometries_.data_keys[data_key_i];
      GeometryData &geo_data = *geometries_.data[data_key_i];
      for (CrossEdgeLengthConstraintUsage &constraint_usage :
           geo_data.cross_edge_length_constraints)
      {
        const CrossEdgeLengthConstraint &constraint =
            constraints_.cross_edge_length_constraints[constraint_usage.constraint_i];
        const Mesh &mesh = *geometries_.geometry_sets[data_key.geo_bundle_i].geometry.get_mesh();
        const VArraySpan<float3> rest_positions = this->lookup_attribute_required<float3>(
            data_key_i, this->prop_attr_name(constraint.path, "rest_position"), AttrDomain::Point);
        if (rest_positions.is_empty()) {
          continue;
        }
        const VArray<float> edge_compliances = this->lookup_attribute_default<float>(
            data_key_i,
            this->prop_attr_name(constraint.path, "compliance"),
            AttrDomain::Edge,
            0.0f);

        Vector<int2> &cross_edges = constraint_usage.cross_edge_points;
        Vector<float> &cross_edge_rest_lengths = constraint_usage.distances;
        Vector<float> &cross_edge_compliances = constraint_usage.compliances;

        this->foreach_cross_edge(mesh, [&](const int edge_i, const int v0, const int v1) {
          const float3 &rest_position0 = rest_positions[v0];
          const float3 &rest_position1 = rest_positions[v1];
          const float rest_length = math::distance(rest_position0, rest_position1);
          const float compliance = edge_compliances[edge_i];
          cross_edges.append({v0, v1});
          cross_edge_rest_lengths.append(rest_length);
          cross_edge_compliances.append(compliance);
        });

        constraint_usage.lambdas = tls.allocator.allocate_array<float>(cross_edges.size());

        auto &constraint_set = tls.scope.construct<xpbd::DistanceConstraintSet>(
            data_key_i,
            cross_edges,
            cross_edge_rest_lengths,
            cross_edge_compliances,
            error_scale_from_threshold(constraint.error_threshold),
            constraint_usage.lambdas);
        xpbd::ConstraintColoring coloring = constraint_set.color_constraints(tls.mask_memory);
        geo_data.static_constraints.append({&constraint_set, std::move(coloring)});
      }
    }
  }

  void foreach_cross_edge(const Mesh &mesh,
                          std::invocable<int /*edge_i*/, int /*v0*/, int /*v1*/> auto &&fn)
  {
    /* Don't use triangulated mesh because the triangulation depends on vertex positions which
     * change during the simulation which introduces instability. */
    const Span<int2> edges = mesh.edges();
    const Span<int> corner_verts = mesh.corner_verts();
    const OffsetIndices<int> faces = mesh.faces();

    Array<int> offsets;
    Array<int> indices;
    const GroupedSpan<int> edge_to_faces = bke::mesh::build_edge_to_face_map(
        faces, mesh.corner_edges(), mesh.edges_num, offsets, indices);

    for (const int edge_i : edges.index_range()) {
      const int2 &edge = edges[edge_i];
      const int edge_v0 = edge[0];
      const int edge_v1 = edge[1];
      const Span<int> incident_faces = edge_to_faces[edge_i];
      if (incident_faces.size() != 2) {
        continue;
      }
      int face0 = incident_faces[0];
      int face1 = incident_faces[1];
      if (faces[face0].size() > faces[face1].size()) {
        std::swap(face0, face1);
      }
      const IndexRange face0_corners = faces[face0];
      const IndexRange face1_corners = faces[face1];
      const int face0_size = face0_corners.size();
      const int face1_size = face1_corners.size();
      if (face0_size == 3 && face1_size == 3) {
        const int face0_v0 = corner_verts[face0_corners[0]];
        const int face0_v1 = corner_verts[face0_corners[1]];
        const int face0_v2 = corner_verts[face0_corners[2]];

        const int face1_v0 = corner_verts[face1_corners[0]];
        const int face1_v1 = corner_verts[face1_corners[1]];
        const int face1_v2 = corner_verts[face1_corners[2]];

        /* Create a cross edge between opposite corners of the two faces. */
        const int cross_edge_v0 = edge_v0 ^ edge_v1 ^ face0_v0 ^ face0_v1 ^ face0_v2;
        const int cross_edge_v1 = edge_v0 ^ edge_v1 ^ face1_v0 ^ face1_v1 ^ face1_v2;
        fn(edge_i, cross_edge_v0, cross_edge_v1);
      }
      else if (face0_size == 3 && face1_size == 4) {
        const int face0_v0 = corner_verts[face0_corners[0]];
        const int face0_v1 = corner_verts[face0_corners[1]];
        const int face0_v2 = corner_verts[face0_corners[2]];

        /* Create two cross edges between the triangle and quad. */
        const int cross_edge_v0 = edge_v0 ^ edge_v1 ^ face0_v0 ^ face0_v1 ^ face0_v2;
        for (const int face1_corner : face1_corners) {
          const int face1_v = corner_verts[face1_corner];
          if (!ELEM(face1_v, edge_v0, edge_v1)) {
            fn(edge_i, cross_edge_v0, face1_v);
          }
        }
      }
      else {
        /* Create cross edges between "diagonal" corners. */
        const Span<int> face0_verts = corner_verts.slice(face0_corners);
        const Span<int> face1_verts = corner_verts.slice(face1_corners);
        const int face0_i0 = face0_verts.first_index(edge_v0);
        const int face0_dir = face0_verts[(face0_i0 + 1) % face0_size] == edge_v1 ? 1 : -1;
        const int face1_i1 = face1_verts.first_index(edge_v1);
        const int face1_dir = face1_verts[(face1_i1 + 1) % face1_size] == edge_v0 ? 1 : -1;
        /* Some corners may be ignored if they if both faces have a different number of
         * corners. */
        const int cross_edge_num = std::max(face0_size, face1_size) - 2;
        for (const int i : IndexRange(cross_edge_num)) {
          const int cross_edge_v0 = face0_verts[mod_i(face0_i0 - face0_dir * (i + 1), face0_size)];
          const int cross_edge_v1 = face1_verts[mod_i(face1_i1 - face1_dir * (i + 1), face1_size)];
          fn(edge_i, cross_edge_v0, cross_edge_v1);
        }
      }
    }
  }

  void gather_from_world__damping()
  {
    TLS &tls = tls_.local();
    const Span<std::string> paths = nested_bundle_paths_.lookup(DampingBundle::name);
    for (const StringRef path : paths) {
      const BundlePtr *bundle_ptr = world_.lookup_path_ptr<BundlePtr>(path);
      if (!bundle_ptr || !*bundle_ptr) {
        continue;
      }
      const Bundle &bundle = **bundle_ptr;

      const int constraint_i = constraints_.damping_constraints.append_and_get_index({path});

      for (const int data_key_i : geometries_.data_keys.index_range()) {
        GeometryData &geo_data = *geometries_.data[data_key_i];
        if (this->effector_applies_to_geometry(path, bundle, data_key_i)) {
          geo_data.damping_constraints.append({constraint_i});
        }
      }
    }

    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      for (DampingConstraintUsage &constraint_usage : geo_data.damping_constraints) {
        const DampingConstraint &constraint =
            constraints_.damping_constraints[constraint_usage.constraint_i];
        constraint_usage.linear_dampings = this->make_range_spans(
            tls,
            this->lookup_attribute_default<float>(data_key_i,
                                                  this->prop_attr_name(constraint.path, "linear"),
                                                  geo_data.domain,
                                                  0.0f));
        constraint_usage.angular_dampings = this->make_range_spans(
            tls,
            this->lookup_attribute_default<float>(data_key_i,
                                                  this->prop_attr_name(constraint.path, "angular"),
                                                  geo_data.domain,
                                                  0.0f));

        constraint_usage.linear_damping_lambdas = tls.allocator.allocate_array<float>(
            geo_data.size);
        constraint_usage.angular_damping_lambdas = tls.allocator.allocate_array<float>(
            geo_data.size);
      }
    }
  }

  void create_chunk_constraints__damping(TLS &tls, const int chunk_i)
  {
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    ChunkData &chunk_data = chunks_data_[chunk_i];
    const GeometryData &geo_data = *geometries_.data[chunk.data_key_i];

    for (const DampingConstraintUsage &constraint_usage : geo_data.damping_constraints) {
      chunk_data.static_velocity_constraints.append(
          &tls.scope.construct<xpbd::LinearDampingConstraintSet>(
              chunk.data_key_i,
              chunk.points_range,
              constraint_usage.linear_dampings.get_span_for_range(chunk.points_range),
              constraint_usage.linear_damping_lambdas.slice(chunk.points_range)));
      if (geo_data.uses_rotation) {
        chunk_data.static_velocity_constraints.append(
            &tls.scope.construct<xpbd::AngularDampingConstraintSet>(
                chunk.data_key_i,
                chunk.points_range,
                constraint_usage.angular_dampings.get_span_for_range(chunk.points_range),
                constraint_usage.angular_damping_lambdas.slice(chunk.points_range)));
      }
    }
  }

  void gather_from_world__pin_positions()
  {
    TLS &tls = tls_.local();
    const Span<std::string> paths = nested_bundle_paths_.lookup(PinPositionBundle::name);
    for (const StringRef path : paths) {
      const Bundle &bundle = **world_.lookup_path_ptr<BundlePtr>(path);
      PinPositionConstraint constraint;
      constraint.path = path;
      constraint.error_threshold =
          bundle.lookup<float>(*BundleKey::from_str("error_threshold")).value_or(1e-3f);
      constraint.lambda_attr =
          bundle.lookup<std::string>(*BundleKey::from_str("lambda_attribute")).value_or("");
      const int constraint_i = constraints_.pin_position_constraints.append_and_get_index(
          std::move(constraint));

      for (const int data_key_i : geometries_.data_keys.index_range()) {
        GeometryData &geo_data = *geometries_.data[data_key_i];
        if (this->effector_applies_to_geometry(path, bundle, data_key_i)) {
          geo_data.pin_position_constraints.append({constraint_i});
        }
      }
    }
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      for (PinPositionConstraintUsage &constraint_usage : geo_data.pin_position_constraints) {
        const PinPositionConstraint &constraint =
            constraints_.pin_position_constraints[constraint_usage.constraint_i];

        const VArray<bool> selection_attr = this->lookup_attribute_required<bool>(
            data_key_i, this->prop_attr_name(constraint.path, "selection"), geo_data.domain);
        const VArray<float3> positions_attr = this->lookup_attribute_required<float3>(
            data_key_i, this->prop_attr_name(constraint.path, "position"), geo_data.domain);
        if (!positions_attr || !selection_attr) {
          continue;
        }
        const IndexMask pin_selection = IndexMask::from_bools(selection_attr, tls.allocator);
        if (pin_selection.is_empty()) {
          continue;
        }
        const VArray<float> compliances_attr = this->lookup_attribute_default<float>(
            data_key_i,
            this->prop_attr_name(constraint.path, "compliance"),
            geo_data.domain,
            0.0f);
        VArray<bool> prev_selection_attr;
        VArray<float3> prev_positions_attr;
        if (sub_delta_time_ > 0.0f) {
          prev_selection_attr = this->lookup_attribute_optional<bool>(
              data_key_i,
              this->prev_prop_attr_name(constraint.path, "selection"),
              geo_data.domain);
          prev_positions_attr = this->lookup_attribute_optional<float3>(
              data_key_i, this->prev_prop_attr_name(constraint.path, "position"), geo_data.domain);
        }

        const int pin_num = pin_selection.size();
        MutableSpan<int> points = tls.allocator.allocate_array<int>(pin_num);
        pin_selection.to_indices(points);

        MutableSpan<float3> end_positions = tls.allocator.allocate_array<float3>(pin_num);
        MutableSpan<float> compliances = tls.allocator.allocate_array<float>(pin_num);

        positions_attr.materialize_compressed_to_uninitialized(pin_selection, end_positions);
        compliances_attr.materialize_compressed_to_uninitialized(pin_selection, compliances);

        constraint_usage.points = points;
        constraint_usage.end_positions = end_positions;
        constraint_usage.compliances = compliances;
        /* These will be initialized later. */
        constraint_usage.lambdas = tls.allocator.allocate_array<float>(pin_num);
        constraint_usage.current_positions = tls.allocator.allocate_array<float3>(pin_num);

        /* Initialize the previous pin position. */
        if (prev_selection_attr && prev_positions_attr) {
          MutableSpan<float3> begin_positions = tls.allocator.allocate_array<float3>(pin_num);
          threading::parallel_for(pin_selection.index_range(), 1024, [&](const IndexRange range) {
            for (const int pin_i : range) {
              const int point_i = points[pin_i];
              const bool was_pinned = prev_selection_attr[point_i];
              if (was_pinned) {
                begin_positions[pin_i] = prev_positions_attr[point_i];
              }
              else {
                begin_positions[pin_i] = geo_data.position_attr.span[point_i];
              }
            }
          });
          constraint_usage.begin_positions = begin_positions;
        }
        else {
          constraint_usage.begin_positions = end_positions;
        }
      }
    }
  }

  void create_chunk_constraints__pin_positions(TLS &tls, const int chunk_i)
  {
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    ChunkData &chunk_data = chunks_data_[chunk_i];
    GeometryData &geo_data = *geometries_.data[chunk.data_key_i];
    for (const int constraint_usage_i : geo_data.pin_position_constraints.index_range()) {
      const PinPositionConstraintUsage &constraint_usage =
          geo_data.pin_position_constraints[constraint_usage_i];
      const PinPositionConstraint &constraint =
          constraints_.pin_position_constraints[constraint_usage.constraint_i];

      /* Detect hard pinned points. */
      for (const int pin_i : constraint_usage.points.index_range()) {
        const int point_i = constraint_usage.points[pin_i];
        const float compliance = constraint_usage.compliances[pin_i];
        if (compliance <= 0.0f) {
          geo_data.is_hard_pinned[point_i] = true;
        }
      }

      const IndexRange pin_range = unique_sorted_indices::find_content_range<int>(
          constraint_usage.points, chunk.points_range);
      if (pin_range.is_empty()) {
        continue;
      }
      chunk_data.pin_position_constraints.append({constraint_usage_i, pin_range});
      chunk_data.static_constraints.append(&tls.scope.construct<xpbd::PinPositionConstraintSet>(
          chunk.data_key_i,
          constraint_usage.points.slice(pin_range),
          constraint_usage.current_positions.slice(pin_range),
          constraint_usage.compliances.slice(pin_range),
          error_scale_from_threshold(constraint.error_threshold),
          constraint_usage.lambdas.slice(pin_range)));
    }
  }

  void write_back__pin_positions()
  {
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      for (const PinPositionConstraintUsage &constraint_usage : geo_data.pin_position_constraints)
      {
        const PinPositionConstraint &constraint =
            constraints_.pin_position_constraints[constraint_usage.constraint_i];
        geo_data.attributes.remove(constraint.lambda_attr);
      }
      for (const PinPositionConstraintUsage &constraint_usage : geo_data.pin_position_constraints)
      {
        if (constraint_usage.points.is_empty()) {
          /* If there is nothing pinned, these attributes don't need to exist. */
          continue;
        }
        const PinPositionConstraint &constraint =
            constraints_.pin_position_constraints[constraint_usage.constraint_i];
        if (bke::SpanAttributeWriter<float> lambda_attr = this->get_output_attribute_writer<float>(
                data_key_i, constraint.lambda_attr, geo_data.domain))
        {
          for (const int pin_i : constraint_usage.points.index_range()) {
            const int point_i = constraint_usage.points[pin_i];
            lambda_attr.span[point_i] = constraint_usage.lambdas[pin_i];
          }
          lambda_attr.finish();
        }
      }
    }
  }

  void gather_from_world__pin_rotations()
  {
    TLS &tls = tls_.local();
    const Span<std::string> paths = nested_bundle_paths_.lookup(PinRotationBundle::name);
    for (const StringRef path : paths) {
      const Bundle &bundle = **world_.lookup_path_ptr<BundlePtr>(path);

      PinRotationConstraint constraint;
      constraint.path = path;
      constraint.error_threshold =
          bundle.lookup<float>(*BundleKey::from_str("error_threshold")).value_or(1e-2f);
      const int constraint_i = constraints_.pin_rotation_constraints.append_and_get_index(
          std::move(constraint));

      for (const int data_key_i : geometries_.data_keys.index_range()) {
        GeometryData &geo_data = *geometries_.data[data_key_i];
        if (!geo_data.uses_rotation) {
          continue;
        }
        if (this->effector_applies_to_geometry(path, bundle, data_key_i)) {
          geo_data.pin_rotation_constraints.append({constraint_i});
        }
      }
    }
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      for (PinRotationConstraintUsage &constraint_usage : geo_data.pin_rotation_constraints) {
        const PinRotationConstraint &constraint =
            constraints_.pin_rotation_constraints[constraint_usage.constraint_i];

        const VArray<bool> selection_attr = this->lookup_attribute_required<bool>(
            data_key_i, this->prop_attr_name(constraint.path, "selection"), geo_data.domain);
        const VArray<math::Quaternion> rotation_attr =
            this->lookup_attribute_required<math::Quaternion>(
                data_key_i, this->prop_attr_name(constraint.path, "rotation"), geo_data.domain);
        if (!selection_attr || !rotation_attr) {
          continue;
        }
        const IndexMask pin_selection = IndexMask::from_bools(selection_attr, tls.allocator);
        if (pin_selection.is_empty()) {
          continue;
        }
        const VArray<float> compliances_attr = this->lookup_attribute_default<float>(
            data_key_i,
            this->prop_attr_name(constraint.path, "compliance"),
            geo_data.domain,
            0.0f);
        VArray<bool> prev_selection_attr;
        VArray<math::Quaternion> prev_rotations_attr;
        if (sub_delta_time_ > 0.0f) {
          prev_selection_attr = this->lookup_attribute_optional<bool>(
              data_key_i,
              this->prev_prop_attr_name(constraint.path, "selection"),
              geo_data.domain);
          prev_rotations_attr = this->lookup_attribute_optional<math::Quaternion>(
              data_key_i, this->prev_prop_attr_name(constraint.path, "rotation"), geo_data.domain);
        }

        const int pin_num = pin_selection.size();
        MutableSpan<int> points = tls.allocator.allocate_array<int>(pin_num);
        pin_selection.to_indices(points);

        MutableSpan<math::Quaternion> end_rotations =
            tls.allocator.allocate_array<math::Quaternion>(pin_num);
        MutableSpan<float> compliances = tls.allocator.allocate_array<float>(pin_num);

        rotation_attr.materialize_compressed_to_uninitialized(pin_selection, end_rotations);
        compliances_attr.materialize_compressed_to_uninitialized(pin_selection, compliances);

        constraint_usage.points = points;
        constraint_usage.end_rotations = end_rotations;
        constraint_usage.compliances = compliances;
        /* These will be initialized later. */
        constraint_usage.lambdas = tls.allocator.allocate_array<float4>(pin_num);
        constraint_usage.current_rotations = tls.allocator.allocate_array<math::Quaternion>(
            pin_num);

        /* Initialize the previous pin rotation. */
        if (prev_selection_attr && prev_rotations_attr) {
          MutableSpan<math::Quaternion> begin_rotations =
              tls.allocator.allocate_array<math::Quaternion>(pin_num);
          threading::parallel_for(pin_selection.index_range(), 1024, [&](const IndexRange range) {
            for (const int pin_i : range) {
              const int point_i = points[pin_i];
              const bool was_pinned = prev_selection_attr[point_i];
              if (was_pinned) {
                begin_rotations[pin_i] = prev_rotations_attr[point_i];
                continue;
              }
              begin_rotations[pin_i] = geo_data.rotation_attr.span[point_i];
            }
          });
          constraint_usage.begin_rotations = begin_rotations;
        }
        else {
          constraint_usage.begin_rotations = end_rotations;
        }
      }
    }
  }

  void create_chunk_constraints__pin_rotations(TLS &tls, const int chunk_i)
  {
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    ChunkData &chunk_data = chunks_data_[chunk_i];
    const GeometryData &geo_data = *geometries_.data[chunk.data_key_i];
    for (const int constraint_usage_i : geo_data.pin_rotation_constraints.index_range()) {
      const PinRotationConstraintUsage &constraint_usage =
          geo_data.pin_rotation_constraints[constraint_usage_i];
      const PinRotationConstraint &constraint =
          constraints_.pin_rotation_constraints[constraint_usage.constraint_i];

      const IndexRange pin_range = unique_sorted_indices::find_content_range<int>(
          constraint_usage.points, chunk.points_range);
      if (pin_range.is_empty()) {
        continue;
      }
      chunk_data.pin_rotation_constraints.append({constraint_usage_i, pin_range});
      chunk_data.static_constraints.append(&tls.scope.construct<xpbd::PinRotationConstraintSet>(
          chunk.data_key_i,
          constraint_usage.points.slice(pin_range),
          constraint_usage.current_rotations.slice(pin_range),
          constraint_usage.compliances.slice(pin_range),
          error_scale_from_threshold(constraint.error_threshold),
          constraint_usage.lambdas.slice(pin_range)));
    }
  }

  struct ResidualErrorTLS {
    float total_error_squared = 0.0f;
    int total_error_count = 0;
  };

  Result do_simulation()
  {
    this->prepare_solver_geometry_refs();
    float average_error_squared = 0.0f;

    if (this->support_chunk_simulation()) {
      threading::EnumerableThreadSpecific<ResidualErrorTLS> error_tls;

      this->parallel_for_each_chunk(1, [&](const int chunk_i) {
        float chunk_error_squared;
        int chunk_error_count;
        int solver_refs_i = 0;
        for (const int substep_i : IndexRange(substeps_)) {
          const SubstepInterval substep(
              substeps_, substep_i, interpolation_begin_, interpolation_end_);
          this->simulate__update_pins__chunk(chunk_i, substep);
          this->simulate__inertial_update__chunk(chunk_i, solver_refs_i);
          this->simulate__gather_dynamic_constraints__chunk(substep, chunk_i, solver_refs_i);
          this->simulate__reset_forces__chunk(chunk_i);

          const Span<xpbd::GeometryRef> solver_refs = geometries_.solver_refs[solver_refs_i];
          xpbd::ConstraintSetParams solve_params{solver_refs, sub_delta_time_};
          for ([[maybe_unused]] const int iter_i : IndexRange(constraint_iterations_)) {
            xpbd::GaussSeidelUpdater updater{solver_refs};
            this->simulate__position_solve__single_iteration__chunk(
                chunk_i, solve_params, updater);
            chunk_error_squared = updater.total_error_squared();
            chunk_error_count = updater.total_error_count();
          }

          this->simulate__update_velocities__chunk(chunk_i, solver_refs_i);
          this->simulate__velocity_solve__chunk(chunk_i, solver_refs_i);
          solver_refs_i = 1 - solver_refs_i;
        }
        this->simulate__ensure_final_data_in_outputs__chunk(chunk_i);

        if (chunk_error_count) {
          error_tls.local().total_error_squared += chunk_error_squared;
          error_tls.local().total_error_count += chunk_error_count;
        }
      });

      float total_error_squared = 0.0f;
      int total_error_count = 0;
      for (const ResidualErrorTLS &error : error_tls) {
        total_error_squared += error.total_error_squared;
        total_error_count += error.total_error_count;
      }
      if (total_error_count) {
        average_error_squared = total_error_squared / total_error_count;
      }
    }
    else {
      int solver_refs_i = 0;
      for (const int substep_i : IndexRange(substeps_)) {
        const SubstepInterval substep(
            substeps_, substep_i, interpolation_begin_, interpolation_end_);
        this->parallel_for_each_chunk(16, [&](const int chunk_i) {
          this->simulate__update_pins__chunk(chunk_i, substep);
          this->simulate__inertial_update__chunk(chunk_i, solver_refs_i);
        });
        this->simulate__gather_dynamic_constraints(substep, solver_refs_i);
        this->simulate__reset_forces();
        for ([[maybe_unused]] const int iter_i : IndexRange(constraint_iterations_)) {
          this->simulate__position_solve__single_iteration(solver_refs_i, average_error_squared);
        }
        this->parallel_for_each_chunk(16, [&](const int chunk_i) {
          this->simulate__update_velocities__chunk(chunk_i, solver_refs_i);
        });
        this->simulate__velocity_solve(solver_refs_i);
        this->parallel_for_each_chunk(16, [&](const int chunk_i) {
          this->simulate__ensure_final_data_in_outputs__chunk(chunk_i);
        });
        solver_refs_i = 1 - solver_refs_i;
      }
    }

    return Result{math::sqrt(average_error_squared)};
  }

  bool support_chunk_simulation() const
  {
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      const GeometryData &geo_data = *geometries_.data[data_key_i];
      if (!geo_data.static_constraints.is_empty()) {
        return false;
      }
    }
    return true;
  }

  void prepare_solver_geometry_refs()
  {
    for (const int direction : IndexRange(2)) {
      Array<xpbd::GeometryRef> &refs = geometries_.solver_refs[direction];
      refs.reinitialize(geometries_.data_keys.size());
      for (const int data_key_i : geometries_.data_keys.index_range()) {
        GeometryData &geo_data = *geometries_.data[data_key_i];
        xpbd::GeometryRef &ref = refs[data_key_i];

        if (direction == 0) {
          ref.positions = geo_data.temp_positions;
          ref.prev_positions = geo_data.position_attr.span;
          if (geo_data.uses_rotation) {
            ref.rotations = geo_data.temp_rotations;
            ref.prev_rotations = geo_data.rotation_attr.span;
          }
        }
        else {
          ref.positions = geo_data.position_attr.span;
          ref.prev_positions = geo_data.temp_positions;
          if (geo_data.uses_rotation) {
            ref.rotations = geo_data.rotation_attr.span;
            ref.prev_rotations = geo_data.temp_rotations;
          }
        }
        ref.velocities = geo_data.velocity_attr.span;
        ref.inv_masses = geo_data.inv_masses;
        if (geo_data.uses_rotation) {
          ref.angular_velocities = geo_data.angular_velocity_attr.span;
          ref.moments_of_inertia = geo_data.moments_of_inertia;
          ref.inv_moments_of_inertia = geo_data.inv_moments_of_inertia;
        }
      }
    }
  }

  void simulate__update_pins__chunk(const int chunk_i, const SubstepInterval &substep)
  {
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    const int data_key_i = chunk.data_key_i;
    GeometryData &geo_data = *geometries_.data[data_key_i];
    ChunkData &chunk_data = chunks_data_[chunk_i];

    /* Update animated pin positions. */
    for (const PinPositionConstraintChunkUsage &constraint_chunk_usage :
         chunk_data.pin_position_constraints)
    {
      const PinPositionConstraintUsage &constraint_usage =
          geo_data.pin_position_constraints[constraint_chunk_usage.constraint_usage_i];
      for (const int pin_i : constraint_chunk_usage.pin_range) {
        const float3 &begin_pos = constraint_usage.begin_positions[pin_i];
        const float3 &end_pos = constraint_usage.end_positions[pin_i];
        const float3 pin_pos = math::interpolate(begin_pos, end_pos, substep.interpolate_end);
        constraint_usage.current_positions[pin_i] = pin_pos;
      }
    }
    if (geo_data.uses_rotation) {
      for (const PinRotationConstraintChunkUsage &constraint_chunk_usage :
           chunk_data.pin_rotation_constraints)
      {
        const PinRotationConstraintUsage &constraint_usage =
            geo_data.pin_rotation_constraints[constraint_chunk_usage.constraint_usage_i];
        for (const int pin_i : constraint_chunk_usage.pin_range) {
          const math::Quaternion &begin_rot = constraint_usage.begin_rotations[pin_i];
          const math::Quaternion &end_rot = constraint_usage.end_rotations[pin_i];
          const math::Quaternion pin_rot = math::interpolate(
              begin_rot, end_rot, substep.interpolate_end);
          constraint_usage.current_rotations[pin_i] = pin_rot;
        }
      }
    }
  }

  void simulate__inertial_update__chunk(const int chunk_i, const int solver_refs_i)
  {
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    const IndexRange points_range = chunk.points_range;
    const int data_key_i = chunk.data_key_i;
    GeometryData &geo_data = *geometries_.data[data_key_i];
    xpbd::GeometryRef &ref = geometries_.solver_refs[solver_refs_i][data_key_i];

    this->integrate_linear_velocities(sub_delta_time_,
                                      ref.prev_positions.slice(points_range),
                                      ref.positions.slice(points_range),
                                      geo_data.velocity_attr.span.slice(points_range),
                                      geo_data.inv_masses.as_span().slice(points_range),
                                      geo_data.external_force_attr.slice(points_range));
    if (geo_data.uses_rotation) {
      this->integrate_angular_velocities(
          sub_delta_time_,
          ref.prev_rotations.slice(points_range),
          ref.rotations.slice(points_range),
          geo_data.angular_velocity_attr.span.slice(points_range),
          geo_data.inv_moments_of_inertia.as_span().slice(points_range),
          geo_data.external_torque_attr.slice(points_range));
    }
  }

  void simulate__gather_dynamic_constraints(const SubstepInterval &substep,
                                            const int solver_refs_i)
  {
    this->parallel_for_each_chunk(1, [&](const int chunk_i) {
      this->simulate__gather_dynamic_constraints__chunk(substep, chunk_i, solver_refs_i);
    });
  }

  void simulate__gather_dynamic_constraints__chunk(const SubstepInterval &substep,
                                                   const int chunk_i,
                                                   const int solver_refs_i)
  {
    const float max_distance = this->get_max_search_distance(sub_delta_time_);
    ChunkData &chunk_data = chunks_data_[chunk_i];
    const ExternalFaceContacts &prev_face_contacts = chunk_data.external_face_contacts;
    const ExternalEdgeContacts &prev_edge_contacts = chunk_data.external_edge_contacts;
    ExternalFaceContacts new_face_contacts;
    ExternalEdgeContacts new_edge_contacts;
    this->gather_contacts__mesh_colliders(chunk_i,
                                          max_distance,
                                          solver_refs_i,
                                          substep,
                                          prev_face_contacts,
                                          prev_edge_contacts,
                                          new_face_contacts,
                                          new_edge_contacts);

    for (const int i : IndexRange(new_face_contacts.points.size())) {
      new_face_contacts.collider_velocities.append(
          math::safe_divide(new_face_contacts.collider_motion[i], sub_delta_time_));
    }
    for (const int i : IndexRange(new_edge_contacts.point_pairs.size())) {
      new_edge_contacts.collider_velocities.append(
          math::safe_divide(new_edge_contacts.collider_motion[i], sub_delta_time_));
    }
    chunk_data.external_face_contacts = std::move(new_face_contacts);
    chunk_data.external_edge_contacts = std::move(new_edge_contacts);
  }

  void simulate__reset_forces()
  {
    this->parallel_for_each_chunk(
        16, [&](const int chunk_i) { this->simulate__reset_forces__chunk(chunk_i); });
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];
      for (ConstraintWithColoring &constraint : geo_data.static_constraints) {
        constraint.constraint->reset_forces();
      }
    }
  }

  void simulate__reset_forces__chunk(const int chunk_i)
  {
    ChunkData &chunk_data = chunks_data_[chunk_i];
    chunk_data.external_face_contacts.lambdas_normal.fill(0.0f);
    chunk_data.external_face_contacts.lambdas_friction.fill(0.0f);
    for (xpbd::ConstraintSet *constraint : chunk_data.static_constraints) {
      constraint->reset_forces();
    }
    for (xpbd::VelocityConstraintSet *constraint : chunk_data.static_velocity_constraints) {
      constraint->reset_forces();
    }
  }

  void simulate__position_solve__single_iteration(const int solver_refs_i,
                                                  float &r_average_error_squared)
  {
    const Span<xpbd::GeometryRef> solver_refs = geometries_.solver_refs[solver_refs_i];
    xpbd::ConstraintSetParams solve_params{solver_refs, sub_delta_time_};
    threading::EnumerableThreadSpecific<ResidualErrorTLS> error_tls;

    this->parallel_for_each_chunk(1, [&](const int chunk_i) {
      xpbd::GaussSeidelUpdater updater{solver_refs};
      this->simulate__position_solve__single_iteration__chunk(chunk_i, solve_params, updater);
      error_tls.local().total_error_squared += updater.total_error_squared();
      error_tls.local().total_error_count += updater.total_error_count();
    });

    for (const int data_key_i : geometries_.data_keys.index_range()) {
      const GeometryData &geo_data = *geometries_.data[data_key_i];
      for (const ConstraintWithColoring &constraint : geo_data.static_constraints) {
        for (const int color_i : constraint.coloring.colors.index_range()) {
          const IndexMask &mask = constraint.coloring.colors[color_i];
          threading::parallel_for(mask.index_range(), 512, [&](const IndexRange range) {
            const IndexMask sliced_mask = mask.slice(range);
            xpbd::GaussSeidelUpdater updater{solver_refs};
            constraint.constraint->solve_sequential(solve_params, updater, sliced_mask);
            error_tls.local().total_error_squared += updater.total_error_squared();
            error_tls.local().total_error_count += updater.total_error_count();
          });
        }
      }
    }

    float total_error_squared = 0.0f;
    int total_error_count = 0;
    for (const ResidualErrorTLS &error : error_tls) {
      total_error_squared += error.total_error_squared;
      total_error_count += error.total_error_count;
    }
    if (total_error_count) {
      r_average_error_squared = total_error_squared / total_error_count;
    }
  }

  void simulate__position_solve__single_iteration__chunk(const int chunk_i,
                                                         xpbd::ConstraintSetParams solve_params,
                                                         xpbd::GaussSeidelUpdater &updater)
  {
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    ChunkData &chunk_data = chunks_data_[chunk_i];

    for (xpbd::ConstraintSet *constraint : chunk_data.static_constraints) {
      constraint->solve_sequential_all(solve_params, updater);
    }

    if (!chunk_data.external_face_contacts.points.is_empty()) {
      ExternalFaceContacts &contacts = chunk_data.external_face_contacts;
      xpbd::CollisionFaceConstraintSet constraint(chunk.data_key_i,
                                                  contacts.points,
                                                  contacts.point_radii,
                                                  contacts.positions_on_face,
                                                  contacts.collider_motion,
                                                  contacts.face_normals,
                                                  contacts.face_margins,
                                                  contacts.compliance_terms,
                                                  contacts.static_frictions,
                                                  contacts.dynamic_frictions,
                                                  contacts.error_scales,
                                                  contacts.active_states,
                                                  contacts.lambdas_normal);
      constraint.solve_sequential_all(solve_params, updater);
    }
    if (!chunk_data.external_edge_contacts.point_pairs.is_empty()) {
      ExternalEdgeContacts &contacts = chunk_data.external_edge_contacts;
      xpbd::CollisionEdgeConstraintSet constraint(chunk.data_key_i,
                                                  contacts.point_pairs,
                                                  contacts.point_radii,
                                                  contacts.positions_on_edge,
                                                  contacts.collider_motion,
                                                  contacts.edge_directions,
                                                  contacts.edge_normals,
                                                  contacts.edge_margins,
                                                  contacts.compliance_terms,
                                                  contacts.static_frictions,
                                                  contacts.dynamic_frictions,
                                                  contacts.error_scales,
                                                  contacts.active_states,
                                                  contacts.point_mix_factors,
                                                  contacts.lambdas_normal);
      constraint.solve_sequential_all(solve_params, updater);
    }
  }

  void simulate__update_velocities__chunk(const int chunk_i, const int solver_refs_i)
  {
    if (sub_delta_time_ <= 0.0f) {
      /* This may happen for example on the first frame when no time has passed yet. */
      return;
    }

    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    const IndexRange points_range = chunk.points_range;
    const int data_key_i = chunk.data_key_i;
    GeometryData &geo_data = *geometries_.data[data_key_i];
    xpbd::GeometryRef &ref = geometries_.solver_refs[solver_refs_i][data_key_i];
    this->update_linear_velocities(sub_delta_time_,
                                   ref.prev_positions.slice(points_range),
                                   ref.positions.slice(points_range),
                                   geo_data.velocity_attr.span.slice(points_range));
    if (geo_data.uses_rotation) {
      this->update_angular_velocities(sub_delta_time_,
                                      ref.prev_rotations.slice(points_range),
                                      ref.rotations.slice(points_range),
                                      geo_data.angular_velocity_attr.span.slice(points_range));
    }
  }

  void simulate__velocity_solve(const int solver_refs_i)
  {
    this->parallel_for_each_chunk(8, [&](const int chunk_i) {
      this->simulate__velocity_solve__chunk(chunk_i, solver_refs_i);
    });
  }

  void simulate__velocity_solve__chunk(const int chunk_i, const int solver_refs_i)
  {
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    ChunkData &chunk_data = chunks_data_[chunk_i];

    const Span<xpbd::GeometryRef> solver_refs = geometries_.solver_refs[solver_refs_i];
    xpbd::ConstraintSetParams params{solver_refs, sub_delta_time_};
    xpbd::VelocityUpdater velocity_updater{solver_refs};

    for (xpbd::VelocityConstraintSet *constraint : chunk_data.static_velocity_constraints) {
      constraint->solve_sequential(params, velocity_updater);
    }

    if (!chunk_data.external_face_contacts.points.is_empty()) {
      ExternalFaceContacts &contacts = chunk_data.external_face_contacts;
      xpbd::FrictionFaceConstraintSet constraint(chunk.data_key_i,
                                                 contacts.points,
                                                 contacts.face_normals,
                                                 contacts.collider_velocities,
                                                 contacts.dynamic_frictions,
                                                 contacts.lambdas_normal,
                                                 contacts.lambdas_friction);
      constraint.solve_sequential(params, velocity_updater);
    }
    if (!chunk_data.external_edge_contacts.point_pairs.is_empty()) {
      ExternalEdgeContacts &contacts = chunk_data.external_edge_contacts;
      xpbd::FrictionEdgeConstraintSet constraint(chunk.data_key_i,
                                                 contacts.point_pairs,
                                                 contacts.edge_normals,
                                                 contacts.collider_velocities,
                                                 contacts.dynamic_frictions,
                                                 contacts.lambdas_normal,
                                                 contacts.point_mix_factors,
                                                 contacts.lambdas_friction);
      constraint.solve_sequential(params, velocity_updater);
    }
  }

  void simulate__ensure_final_data_in_outputs__chunk(const int chunk_i)
  {
    if (substeps_ % 2 == 0) {
      /* Nothing to do. */
      return;
    }
    const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
    GeometryData &geo_data = *geometries_.data[chunk.data_key_i];
    const IndexRange points_range = chunk.points_range;
    geo_data.position_attr.span.slice(points_range)
        .copy_from(geo_data.temp_positions.as_span().slice(points_range));
    if (geo_data.uses_rotation) {
      geo_data.rotation_attr.span.slice(points_range)
          .copy_from(geo_data.temp_rotations.as_span().slice(points_range));
    }
  }

  void finish_common_attribute_writers()
  {
    for (const int data_key_i : geometries_.data_keys.index_range()) {
      GeometryData &geo_data = *geometries_.data[data_key_i];

      geo_data.position_attr.finish();
      geo_data.velocity_attr.finish();
      geo_data.rotation_attr.finish();
      geo_data.angular_velocity_attr.finish();
    }
  }

  void integrate_linear_velocities(const float delta_time,
                                   const Span<float3> old_positions,
                                   MutableSpan<float3> new_positions,
                                   MutableSpan<float3> velocities,
                                   const Span<float> inv_masses,
                                   const Span<float3> forces)
  {
    for (const int i : old_positions.index_range()) {
      const float3 external_force = forces[i];
      const float inv_mass = inv_masses[i];
      const float3 &old_pos = old_positions[i];
      const float3 acceleration = external_force * inv_mass;
      const float3 new_velocity = velocities[i] + acceleration * delta_time;
      velocities[i] = new_velocity;
      new_positions[i] = old_pos + new_velocity * delta_time;
    }
  }

  void integrate_angular_velocities(const float delta_time,
                                    const Span<math::Quaternion> old_rotations,
                                    MutableSpan<math::Quaternion> new_rotations,
                                    MutableSpan<float3> angular_velocities,
                                    const Span<float3> inv_inertias,
                                    const Span<float3> torques)
  {
    for (const int i : old_rotations.index_range()) {
      const math::Quaternion &old_rotation = old_rotations[i];
      const float3 &external_torque = torques[i];
      const float3 &inv_inertia = inv_inertias[i];
      if (math::is_zero(inv_inertia)) {
        new_rotations[i] = old_rotation;
        continue;
      }
      float3 &angular_velocity = angular_velocities[i];
      const float3 precession = math::cross(angular_velocity,
                                            math::safe_divide(angular_velocity, inv_inertia));
      angular_velocity += delta_time * (external_torque - precession) * inv_inertia;
      const math::Quaternion direction = old_rotation * math::Quaternion(0, angular_velocity);
      new_rotations[i] = math::normalize(
          math::Quaternion(float4(old_rotation) + delta_time * 0.5f * float4(direction)));
    }
  }

  void update_linear_velocities(const float delta_time,
                                const Span<float3> prev_positions,
                                const Span<float3> new_positions,
                                MutableSpan<float3> r_velocities)
  {
    const float inv_delta_time = math::safe_rcp(delta_time);
    for (const int i : r_velocities.index_range()) {
      const float3 &old_pos = prev_positions[i];
      const float3 &new_pos = new_positions[i];
      const float3 diff = new_pos - old_pos;
      const float3 velocity = diff * inv_delta_time;
      r_velocities[i] = velocity;
    }
  }

  void update_angular_velocities(const float delta_time,
                                 const Span<math::Quaternion> prev_rotations,
                                 const Span<math::Quaternion> new_rotations,
                                 MutableSpan<float3> r_angular_velocities)
  {
    const float inv_delta_time = math::safe_rcp(delta_time);
    for (const int i : r_angular_velocities.index_range()) {
      float3 diff =
          (math::invert_normalized(prev_rotations[i]) * new_rotations[i]).imaginary_part();
      for (const int j : IndexRange(3)) {
        if (math::abs(diff[j]) < 1e-5f) {
          diff[j] = 0.0f;
        }
      }
      const float3 new_angular_velocity = 2.0f * diff * inv_delta_time;
      r_angular_velocities[i] = new_angular_velocity;
    }
  }

  float compute_contact_friction(const float point_friction, const float collider_friction) const
  {
    return math::sqrt(point_friction * collider_friction);
  }

  float get_max_search_distance(const float delta_time)
  {
    /* This is 60 m/s or 216 km/h. */
    constexpr float max_velocity = 60.0f;
    const float max_search_distance = 2.0f * max_velocity * delta_time;
    return max_search_distance;
  }

  template<typename T>
  bke::SpanAttributeWriter<T> ensure_attribute(bke::MutableAttributeAccessor attributes,
                                               const StringRef name,
                                               const bke::AttrDomain domain)
  {
    const std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(name);
    if (meta_data) {
      if (meta_data->domain != domain ||
          meta_data->data_type != bke::cpp_type_to_attribute_type(CPPType::get<T>()))
      {
        attributes.remove(name);
      }
    }
    return attributes.lookup_or_add_for_write_span<T>(name, domain);
  }

  template<typename T>
  bke::SpanAttributeWriter<T> get_output_attribute_writer(const int geo_data_i,
                                                          const StringRef name,
                                                          const AttrDomain domain)
  {
    if (name.is_empty()) {
      return {};
    }
    GeometryData &geo_data = *geometries_.data[geo_data_i];
    return geo_data.attributes.lookup_or_add_for_write_span<T>(name, domain);
  }

  const Bundle *get_previous_bundle(const Bundle &bundle) const
  {
    const BundlePtr *previous_bundle_ptr = bundle.lookup_ptr<BundlePtr>(
        *BundleKey::from_str("previous"));
    if (!previous_bundle_ptr || !*previous_bundle_ptr) {
      return nullptr;
    }
    return &**previous_bundle_ptr;
  }

  bool effector_applies_to_geometry([[maybe_unused]] const StringRef effector_path,
                                    const Bundle &effector,
                                    const int data_key_i) const
  {
    const DataKey &data_key = geometries_.data_keys[data_key_i];
    const GeometrySetData &geo_set_data = geometries_.geometry_sets[data_key.geo_bundle_i];
    const StringRef geo_bundle_path = geo_set_data.path;

    const std::string filter =
        effector.lookup<std::string>(*BundleKey::from_str("filter")).value_or("");
    const bool match = tag_filter_matches(filter, geo_set_data.tags);
    return match;
  }

  void parallel_for_each_chunk(const int grain_size, const FunctionRef<void(int chunk_i)> fn)
  {
    threading::parallel_for(
        geometries_.chunks.index_range(), grain_size, [&](const IndexRange chunk_range) {
          for (const int chunk_i : chunk_range) {
            fn(chunk_i);
          }
        });
  }

  void write_back_geometries_to_world()
  {
    for (const int geo_bundle_i : geometries_.geometry_sets.index_range()) {
      GeometrySetData &geo_set_data = geometries_.geometry_sets[geo_bundle_i];
      world_.add_path_override(geo_set_data.path, std::move(geo_set_data.geometry));
    }
  }

  PointCloud *write_back__plane_contacts(const IndexRange mesh_colliders_range,
                                         const OffsetIndices<int> points_by_chunk)
  {
    PointCloud *pointcloud = BKE_pointcloud_new_nomain(points_by_chunk.total_size());
    MutableAttributeAccessor attributes = pointcloud->attributes_for_write();
    bke::SpanAttributeWriter<int> geometries_writer =
        attributes.lookup_or_add_for_write_only_span<int>("geometry", AttrDomain::Point);
    bke::SpanAttributeWriter<int> colliders_writer =
        attributes.lookup_or_add_for_write_only_span<int>("collider", AttrDomain::Point);
    bke::SpanAttributeWriter<int> geometry_points0_writer =
        attributes.lookup_or_add_for_write_only_span<int>("geometry_point0", AttrDomain::Point);
    MutableSpan<float3> positions = pointcloud->positions_for_write();
    bke::SpanAttributeWriter<float3> collider_velocities_writer =
        attributes.lookup_or_add_for_write_only_span<float3>("collider_velocity",
                                                             AttrDomain::Point);
    bke::SpanAttributeWriter<float3> normals_writer =
        attributes.lookup_or_add_for_write_only_span<float3>("normal", AttrDomain::Point);
    bke::SpanAttributeWriter<float> static_frictions_writer =
        attributes.lookup_or_add_for_write_only_span<float>("static_friction", AttrDomain::Point);
    bke::SpanAttributeWriter<float> dynamic_frictions_writer =
        attributes.lookup_or_add_for_write_only_span<float>("dynamic_friction", AttrDomain::Point);
    bke::SpanAttributeWriter<bool> active_states_writer =
        attributes.lookup_or_add_for_write_only_span<bool>("active", AttrDomain::Point);
    bke::SpanAttributeWriter<float> lambdas_normal_writer =
        attributes.lookup_or_add_for_write_only_span<float>("lambda_normal", AttrDomain::Point);
    bke::SpanAttributeWriter<float> lambdas_friction_writer =
        attributes.lookup_or_add_for_write_only_span<float>("lambda_friction", AttrDomain::Point);

    this->parallel_for_each_chunk(16, [&](const int chunk_i) {
      const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
      const ChunkData &chunk_data = chunks_data_[chunk_i];
      const ExternalFaceContacts &contacts = chunk_data.external_face_contacts;

      const IndexRange points = points_by_chunk[chunk_i];
      if (points.is_empty()) {
        /* Avoids checking the filter a second time. */
        return;
      }

      /* Store collider indices and affected geometry point indices that map a contact to the
       * simulated geometry. */
      MutableSpan<int> geometries = geometries_writer.span.slice(points);
      MutableSpan<int> colliders = colliders_writer.span.slice(points);
      MutableSpan<int> geometry_points0 = geometry_points0_writer.span.slice(points);

      geometries.fill(chunk.data_key_i);
      for (const auto &item : contacts.mesh_contact_indices.items()) {
        colliders[item.value] = mesh_colliders_range[item.key.mesh_collider_i];
        geometry_points0[item.value] = contacts.points[item.value];
      }

      array_utils::copy(contacts.positions_on_face.as_span(), positions.slice(points));
      array_utils::copy(contacts.collider_velocities.as_span(),
                        collider_velocities_writer.span.slice(points));
      array_utils::copy(contacts.face_normals.as_span(), normals_writer.span.slice(points));
      array_utils::copy(contacts.static_frictions.as_span(),
                        static_frictions_writer.span.slice(points));
      array_utils::copy(contacts.dynamic_frictions.as_span(),
                        dynamic_frictions_writer.span.slice(points));
      array_utils::copy(contacts.active_states.as_span(), active_states_writer.span.slice(points));
      array_utils::copy(contacts.lambdas_normal.as_span(),
                        lambdas_normal_writer.span.slice(points));
      array_utils::copy(contacts.lambdas_friction.as_span(),
                        lambdas_friction_writer.span.slice(points));
    });
    pointcloud->tag_positions_changed();
    geometries_writer.finish();
    colliders_writer.finish();
    geometry_points0_writer.finish();
    collider_velocities_writer.finish();
    normals_writer.finish();
    static_frictions_writer.finish();
    dynamic_frictions_writer.finish();
    active_states_writer.finish();
    lambdas_normal_writer.finish();
    lambdas_friction_writer.finish();

    return pointcloud;
  }

  PointCloud *write_back__edge_contacts(const IndexRange mesh_colliders_range,
                                        const OffsetIndices<int> points_by_chunk)
  {
    PointCloud *pointcloud = BKE_pointcloud_new_nomain(points_by_chunk.total_size());
    MutableAttributeAccessor attributes = pointcloud->attributes_for_write();
    bke::SpanAttributeWriter<int> geometries_writer =
        attributes.lookup_or_add_for_write_only_span<int>("geometry", AttrDomain::Point);
    bke::SpanAttributeWriter<int> colliders_writer =
        attributes.lookup_or_add_for_write_only_span<int>("collider", AttrDomain::Point);
    bke::SpanAttributeWriter<int> geometry_points0_writer =
        attributes.lookup_or_add_for_write_only_span<int>("geometry_point0", AttrDomain::Point);
    bke::SpanAttributeWriter<int> geometry_points1_writer =
        attributes.lookup_or_add_for_write_only_span<int>("geometry_point1", AttrDomain::Point);
    MutableSpan<float3> positions = pointcloud->positions_for_write();
    bke::SpanAttributeWriter<float3> collider_velocities_writer =
        attributes.lookup_or_add_for_write_only_span<float3>("collider_velocity",
                                                             AttrDomain::Point);
    bke::SpanAttributeWriter<float3> normals_writer =
        attributes.lookup_or_add_for_write_only_span<float3>("normal", AttrDomain::Point);
    bke::SpanAttributeWriter<float> static_frictions_writer =
        attributes.lookup_or_add_for_write_only_span<float>("static_friction", AttrDomain::Point);
    bke::SpanAttributeWriter<float> dynamic_frictions_writer =
        attributes.lookup_or_add_for_write_only_span<float>("dynamic_friction", AttrDomain::Point);
    bke::SpanAttributeWriter<bool> active_states_writer =
        attributes.lookup_or_add_for_write_only_span<bool>("active", AttrDomain::Point);
    bke::SpanAttributeWriter<float> lambdas_normal_writer =
        attributes.lookup_or_add_for_write_only_span<float>("lambda_normal", AttrDomain::Point);
    bke::SpanAttributeWriter<float> lambdas_friction_writer =
        attributes.lookup_or_add_for_write_only_span<float>("lambda_friction", AttrDomain::Point);

    this->parallel_for_each_chunk(16, [&](const int chunk_i) {
      const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
      const ChunkData &chunk_data = chunks_data_[chunk_i];
      const ExternalEdgeContacts &contacts = chunk_data.external_edge_contacts;

      const IndexRange points = points_by_chunk[chunk_i];
      if (points.is_empty()) {
        /* Avoids checking the filter a second time. */
        return;
      }

      /* Store collider indices and affected geometry point indices that map a contact to the
       * simulated geometry. */
      MutableSpan<int> geometries = geometries_writer.span.slice(points);
      MutableSpan<int> colliders = colliders_writer.span.slice(points);
      MutableSpan<int> geometry_points0 = geometry_points0_writer.span.slice(points);
      MutableSpan<int> geometry_points1 = geometry_points1_writer.span.slice(points);
      geometries.fill(chunk.data_key_i);
      for (const auto &item : contacts.mesh_contact_indices.items()) {
        colliders[item.value] = mesh_colliders_range[item.key.mesh_collider_i];
        geometry_points0[item.value] = contacts.point_pairs[item.value][0];
        geometry_points1[item.value] = contacts.point_pairs[item.value][1];
      }

      array_utils::copy(contacts.positions_on_edge.as_span(), positions.slice(points));
      array_utils::copy(contacts.collider_velocities.as_span(),
                        collider_velocities_writer.span.slice(points));
      array_utils::copy(contacts.edge_normals.as_span(), normals_writer.span.slice(points));
      array_utils::copy(contacts.static_frictions.as_span(),
                        static_frictions_writer.span.slice(points));
      array_utils::copy(contacts.dynamic_frictions.as_span(),
                        dynamic_frictions_writer.span.slice(points));
      array_utils::copy(contacts.active_states.as_span(), active_states_writer.span.slice(points));
      array_utils::copy(contacts.lambdas_normal.as_span(),
                        lambdas_normal_writer.span.slice(points));
      array_utils::copy(contacts.lambdas_friction.as_span(),
                        lambdas_friction_writer.span.slice(points));
    });
    pointcloud->tag_positions_changed();
    geometries_writer.finish();
    colliders_writer.finish();
    geometry_points0_writer.finish();
    geometry_points1_writer.finish();
    collider_velocities_writer.finish();
    normals_writer.finish();
    static_frictions_writer.finish();
    dynamic_frictions_writer.finish();
    active_states_writer.finish();
    lambdas_normal_writer.finish();
    lambdas_friction_writer.finish();

    return pointcloud;
  }

  void write_back__contacts()
  {
    for (CollisionContacts &contacts : this->constraints_.collision_contacts) {
      const Bundle &bundle = **world_.lookup_path_ptr<BundlePtr>(contacts.path);
      const std::string plane_points_path = Bundle::combine_path(
          {contacts.path, "plane_contacts"});
      const std::string edge_points_path = Bundle::combine_path({contacts.path, "edge_contacts"});
      const std::string collider_map_path = Bundle::combine_path({contacts.path, "collider_map"});

      /* Collider paths are combined in a single array, these are offsets for collider indices. */
      const IndexRange geometries_range = geometries_.geometry_sets.index_range();
      const IndexRange mesh_colliders_range = geometries_range.after(
          constraints_.mesh_colliders.size());
      Vector<std::string> collider_paths;
      collider_paths.reserve(geometries_range.size() + mesh_colliders_range.size());
      for (const GeometrySetData &geometry_set_data : geometries_.geometry_sets) {
        collider_paths.append_unchecked(geometry_set_data.path);
      }
      for (const MeshCollider &collider : constraints_.mesh_colliders) {
        collider_paths.append_unchecked(collider.path);
      }

      Array<int> plane_contacts_offsets(geometries_.chunks.size() + 1);
      Array<int> edge_contacts_offsets(geometries_.chunks.size() + 1);
      this->parallel_for_each_chunk(16, [&](const int chunk_i) {
        const GeometryDataChunk &chunk = geometries_.chunks[chunk_i];
        const ChunkData &chunk_data = chunks_data_[chunk_i];

        if (!effector_applies_to_geometry(contacts.path, bundle, chunk.data_key_i)) {
          plane_contacts_offsets[chunk_i] = 0;
          edge_contacts_offsets[chunk_i] = 0;
          return;
        }

        plane_contacts_offsets[chunk_i] = chunk_data.external_face_contacts.points.size();
        edge_contacts_offsets[chunk_i] = chunk_data.external_edge_contacts.point_pairs.size();
      });
      const OffsetIndices plane_contacts_by_chunk = offset_indices::accumulate_counts_to_offsets(
          plane_contacts_offsets);
      const OffsetIndices edge_contacts_by_chunk = offset_indices::accumulate_counts_to_offsets(
          edge_contacts_offsets);

      GeometrySet plane_contacts_geometry;
      GeometrySet edge_contacts_geometry;
      if (plane_contacts_by_chunk.total_size() > 0) {
        plane_contacts_geometry.replace_pointcloud(
            write_back__plane_contacts(mesh_colliders_range, plane_contacts_by_chunk));
      }
      if (edge_contacts_by_chunk.total_size() > 0) {
        edge_contacts_geometry.replace_pointcloud(
            write_back__edge_contacts(mesh_colliders_range, edge_contacts_by_chunk));
      }

      world_.add_path_override(plane_points_path, std::move(plane_contacts_geometry));
      world_.add_path_override(edge_points_path, std::move(edge_contacts_geometry));
      /* TODO Currently have to create an explicit BundleItemSocketValue to store a list with
       * add_path_override. It relies on socket_type_info_by_static_type, which only supports
       * fields and single values currently, but not lists. */
      world_.add_path_override(
          collider_map_path,
          BundleItemSocketValue{
              bke::node_socket_type_find_static(SOCK_STRING),
              bke::SocketValueVariant::From(GList::from_container(std::move(collider_paths)))});
    }
  }

  template<typename T> VArrayRangeSpans<T> make_range_spans(TLS &tls, const VArray<T> &varray)
  {
    return VArrayRangeSpans<T>(tls.scope, varray, geometries_.max_chunk_size);
  }

  template<typename T>
  VArray<T> lookup_attribute_default(const int data_key_i,
                                     const StringRef attribute_name,
                                     const AttrDomain domain,
                                     const T &default_value)
  {
    GeometryData &geo_data = *geometries_.data[data_key_i];
    VArray<T> varray = *geo_data.attributes.lookup<T>(attribute_name, domain);
    if (!varray) {
      this->report(
          NodeWarningType::Info,
          fmt::format(
              "Attribute \"{}\" not found in \"{}\", using fallback value",
              attribute_name,
              geometries_.geometry_sets[geometries_.data_keys[data_key_i].geo_bundle_i].path));
      return VArray<T>::from_single(default_value, geo_data.attributes.domain_size(domain));
    }
    return varray;
  }

  template<typename T>
  VArray<T> lookup_attribute_required(const int data_key_i,
                                      const StringRef attribute_name,
                                      const AttrDomain domain)
  {
    GeometryData &geo_data = *geometries_.data[data_key_i];
    VArray<T> varray = *geo_data.attributes.lookup<T>(attribute_name, domain);
    if (!varray) {
      this->report(
          NodeWarningType::Error,
          fmt::format(
              "Attribute \"{}\" not found on \"{}\", some functionality is disabled",
              attribute_name,
              geometries_.geometry_sets[geometries_.data_keys[data_key_i].geo_bundle_i].path));
    }
    return varray;
  }

  template<typename T>
  VArray<T> lookup_attribute_optional(const int data_key_i,
                                      const StringRef attribute_name,
                                      const AttrDomain domain)
  {
    GeometryData &geo_data = *geometries_.data[data_key_i];
    VArray<T> varray = *geo_data.attributes.lookup<T>(attribute_name, domain);
    if (!varray) {
      this->report(
          NodeWarningType::Info,
          fmt::format(
              "Optional attribute \"{}\" not found on \"{}\"",
              attribute_name,
              geometries_.geometry_sets[geometries_.data_keys[data_key_i].geo_bundle_i].path));
    }
    return varray;
  }

  std::string prop_attr_name(const StringRef effector_path, const StringRef prop_name) const
  {
    return fmt::format("sim:prop:{}:{}", effector_path, prop_name);
  }

  std::string prev_prop_attr_name(const StringRef effector_path, const StringRef prop_name) const
  {
    return fmt::format("sim:prop_prev:{}:{}", effector_path, prop_name);
  }

  void report(NodeWarningType type, std::string warning)
  {
    std::lock_guard<Mutex> lock(warnings_mutex_);
    warnings_.add({type, std::move(warning)});
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  BundlePtr world_ptr = params.get_input<BundlePtr>("World"_ustr);
  if (!world_ptr) {
    params.set_default_remaining_outputs();
    return;
  }
  const int substeps = params.get_input<int>("Substeps"_ustr);
  if (substeps <= 0) {
    params.set_output("World"_ustr, std::move(world_ptr));
    return;
  }
  const int constraint_iterations = params.get_input<int>("Constraint Iterations"_ustr);
  const float delta_time = std::max(0.0f, params.get_input<float>("Delta Time"_ustr));
  Bundle &world = world_ptr.ensure_mutable_inplace();
  const std::string geometry_tag_filter = params.extract_input<std::string>("Filter"_ustr);
  const float4x4 simulation_to_world = params.extract_input<float4x4>("Simulation to World"_ustr);
  const float interpolation_begin = params.extract_input<float>("Begin"_ustr);
  const float interpolation_end = params.extract_input<float>("End"_ustr);
  const std::string solver_path = params.extract_input<std::string>("Solver Path"_ustr);

  XpbdSolverStep step(world,
                      delta_time,
                      substeps,
                      constraint_iterations,
                      interpolation_begin,
                      interpolation_end,
                      geometry_tag_filter,
                      simulation_to_world);
  step.do_step();

  if (!solver_path.empty()) {
    BundlePtr solver_data_ptr = Bundle::create();
    Bundle &solver_data = solver_data_ptr.ensure_mutable_inplace();
    solver_data.add(Bundle::type_item_name, std::string(XPBDSolverDataBundle::name));
    solver_data.add_path("residual_error", step.result().total_residual_error);
    world.add_path_override(solver_path, std::move(solver_data_ptr));
  }
  for (const std::pair<NodeWarningType, std::string> &warning : step.warnings()) {
    params.error_message_add(warning.first, warning.second);
  }

  params.set_output("World"_ustr, std::move(world_ptr));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeXPBDSolver"_ustr);
  ntype.ui_name = "XPBD Solver";
  ntype.ui_description = "Simulate physics using the XPBD framework";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.default_width = bke::NodeWidth::_160;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_xpbd_solver_cc
