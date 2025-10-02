/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_lib_id.hh"
#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_closure.hh"
#include "NOD_geometry_nodes_log.hh"

#include "BLI_listbase.h"
#include "BLI_stack.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_compute_context_cache.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_nodes_gizmos_transforms.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_query.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_report.hh"
#include "BKE_type_conversions.hh"
#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_openvdb.hh"

#include "DNA_grease_pencil_types.h"
#include "DNA_modifier_types.h"
#include "DNA_space_types.h"

#include "ED_geometry.hh"
#include "ED_node.hh"
#include "ED_viewer_path.hh"

#include "MOD_nodes.hh"

#include "UI_resources.hh"

#include "DEG_depsgraph_query.hh"

namespace blender::nodes::geo_eval_log {

using bke::bNodeTreeZone;
using bke::bNodeTreeZones;
using fn::FieldInput;
using fn::FieldInputs;

GenericValueLog::~GenericValueLog()
{
  this->value.destruct();
}

StringLog::StringLog(StringRef string, LinearAllocator<> &allocator)
{
  /* Avoid logging the entirety of long strings, to avoid unnecessary memory usage. */
  if (string.size() <= 100) {
    this->truncated = false;
    this->value = allocator.copy_string(string);
    return;
  }
  this->truncated = true;
  const char *end = BLI_str_find_prev_char_utf8(string.data() + 100, string.data());
  this->value = allocator.copy_string(StringRef(string.data(), end));
}

FieldInfoLog::FieldInfoLog(const GField &field) : type(field.cpp_type())
{
  const std::shared_ptr<const fn::FieldInputs> &field_input_nodes = field.node().field_inputs();

  /* Put the deduplicated field inputs into a vector so that they can be sorted below. */
  Vector<std::reference_wrapper<const FieldInput>> field_inputs;
  if (field_input_nodes) {
    field_inputs.extend(field_input_nodes->deduplicated_nodes.begin(),
                        field_input_nodes->deduplicated_nodes.end());
  }

  std::sort(
      field_inputs.begin(), field_inputs.end(), [](const FieldInput &a, const FieldInput &b) {
        const int index_a = int(a.category());
        const int index_b = int(b.category());
        if (index_a == index_b) {
          return a.socket_inspection_name().size() < b.socket_inspection_name().size();
        }
        return index_a < index_b;
      });

  for (const FieldInput &field_input : field_inputs) {
    this->input_tooltips.append(field_input.socket_inspection_name());
  }
}

GeometryInfoLog::GeometryInfoLog(const bke::GeometrySet &geometry_set)
{
  this->name = geometry_set.name;

  static std::array all_component_types = {bke::GeometryComponent::Type::Curve,
                                           bke::GeometryComponent::Type::Instance,
                                           bke::GeometryComponent::Type::Mesh,
                                           bke::GeometryComponent::Type::PointCloud,
                                           bke::GeometryComponent::Type::GreasePencil,
                                           bke::GeometryComponent::Type::Volume};

  /* Keep track handled attribute names to make sure that we do not return the same name twice.
   * Currently #GeometrySet::attribute_foreach does not do that. Note that this will merge
   * attributes with the same name but different domains or data types on separate components. */
  Set<StringRef> names;

  geometry_set.attribute_foreach(
      all_component_types,
      true,
      [&](const StringRef attribute_id,
          const bke::AttributeMetaData &meta_data,
          const bke::GeometryComponent & /*component*/) {
        if (!bke::attribute_name_is_anonymous(attribute_id) && names.add(attribute_id)) {
          this->attributes.append({attribute_id, meta_data.domain, meta_data.data_type});
        }
      });

  for (const bke::GeometryComponent *component : geometry_set.get_components()) {
    this->component_types.append(component->type());
    switch (component->type()) {
      case bke::GeometryComponent::Type::Mesh: {
        const auto &mesh_component = *static_cast<const bke::MeshComponent *>(component);
        MeshInfo &info = this->mesh_info.emplace();
        info.verts_num = mesh_component.attribute_domain_size(bke::AttrDomain::Point);
        info.edges_num = mesh_component.attribute_domain_size(bke::AttrDomain::Edge);
        info.faces_num = mesh_component.attribute_domain_size(bke::AttrDomain::Face);
        break;
      }
      case bke::GeometryComponent::Type::Curve: {
        const auto &curve_component = *static_cast<const bke::CurveComponent *>(component);
        CurveInfo &info = this->curve_info.emplace();
        info.points_num = curve_component.attribute_domain_size(bke::AttrDomain::Point);
        info.splines_num = curve_component.attribute_domain_size(bke::AttrDomain::Curve);
        break;
      }
      case bke::GeometryComponent::Type::PointCloud: {
        const auto &pointcloud_component = *static_cast<const bke::PointCloudComponent *>(
            component);
        PointCloudInfo &info = this->pointcloud_info.emplace();
        info.points_num = pointcloud_component.attribute_domain_size(bke::AttrDomain::Point);
        break;
      }
      case bke::GeometryComponent::Type::Instance: {
        const auto &instances_component = *static_cast<const bke::InstancesComponent *>(component);
        InstancesInfo &info = this->instances_info.emplace();
        info.instances_num = instances_component.attribute_domain_size(bke::AttrDomain::Instance);
        break;
      }
      case bke::GeometryComponent::Type::Edit: {
        const auto &edit_component = *static_cast<const bke::GeometryComponentEditData *>(
            component);
        if (!this->edit_data_info) {
          this->edit_data_info.emplace(EditDataInfo());
        }
        EditDataInfo &info = *this->edit_data_info;
        if (const bke::CurvesEditHints *curve_edit_hints = edit_component.curves_edit_hints_.get())
        {
          info.has_deform_matrices = curve_edit_hints->deform_mats.has_value();
          info.has_deformed_positions = curve_edit_hints->positions().has_value();
        }
        if (const bke::GizmoEditHints *gizmo_edit_hints = edit_component.gizmo_edit_hints_.get()) {
          info.gizmo_transforms_num = gizmo_edit_hints->gizmo_transforms.size();
        }
        break;
      }
      case bke::GeometryComponent::Type::Volume: {
#ifdef WITH_OPENVDB
        const auto &volume_component = *static_cast<const bke::VolumeComponent *>(component);
        if (const Volume *volume = volume_component.get()) {
          VolumeInfo &info = this->volume_info.emplace();
          info.grids.resize(BKE_volume_num_grids(volume));
          for (const int i : IndexRange(BKE_volume_num_grids(volume))) {
            const bke::VolumeGridData *grid = BKE_volume_grid_get(volume, i);
            info.grids[i] = {grid->name(), bke::volume_grid::get_type(*grid)};
          }
        }
#endif /* WITH_OPENVDB */
        break;
      }
      case bke::GeometryComponent::Type::GreasePencil: {
        const auto &grease_pencil_component = *static_cast<const bke::GreasePencilComponent *>(
            component);
        if (const GreasePencil *grease_pencil = grease_pencil_component.get()) {
          GreasePencilInfo &info = this->grease_pencil_info.emplace(GreasePencilInfo());
          info.layers_num = grease_pencil->layers().size();
          Set<StringRef> unique_layer_names;
          for (const bke::greasepencil::Layer *layer : grease_pencil->layers()) {
            const StringRefNull layer_name = layer->name();
            if (unique_layer_names.add(layer_name)) {
              info.layer_names.append(layer_name);
            }
          }
        }
        break;
      }
    }
  }
}

#ifdef WITH_OPENVDB
struct GridIsEmptyOp {
  const openvdb::GridBase &base_grid;
  bool result = false;

  template<typename GridType> bool operator()()
  {
    result = static_cast<const GridType &>(base_grid).empty();
    return true;
  }
};
#endif /* WITH_OPENVDB */

GridInfoLog::GridInfoLog(const bke::GVolumeGrid &grid)
{
#ifdef WITH_OPENVDB
  bke::VolumeTreeAccessToken token;
  const openvdb::GridBase &vdb_grid = grid->grid(token);
  const VolumeGridType grid_type = bke::volume_grid::get_type(vdb_grid);

  GridIsEmptyOp is_empty_op{vdb_grid};
  if (BKE_volume_grid_type_operation(grid_type, is_empty_op)) {
    this->is_empty = is_empty_op.result;
  }
  else {
    this->is_empty = true;
  }
#else
  UNUSED_VARS(grid);
  this->is_empty = true;
#endif
}

BundleValueLog::BundleValueLog(Vector<Item> items) : items(std::move(items)) {}

ClosureValueLog::ClosureValueLog(Vector<Item> inputs,
                                 Vector<Item> outputs,
                                 const std::optional<ClosureSourceLocation> &source_location,
                                 std::shared_ptr<ClosureEvalLog> eval_log)
    : inputs(std::move(inputs)), outputs(std::move(outputs)), eval_log(std::move(eval_log))
{
  if (source_location) {
    const bNodeTree *tree_eval = source_location->tree;
    const bNodeTree *tree_orig = reinterpret_cast<const bNodeTree *>(
        DEG_get_original_id(&tree_eval->id));
    this->source = Source{tree_orig->id.session_uid,
                          source_location->closure_output_node_id,
                          source_location->compute_context_hash};
  }
}

ListInfoLog::ListInfoLog(const List *list)
{
  if (!list) {
    this->size = 0;
    return;
  }
  this->size = list->size();
}

NodeWarning::NodeWarning(const Report &report)
{
  switch (report.type) {
    case RPT_ERROR:
      this->type = NodeWarningType::Error;
      break;
    default:
      this->type = NodeWarningType::Info;
      break;
  }
  this->message = report.message;
}

/* Avoid generating these in every translation unit. */
GeoNodesLog::GeoNodesLog() = default;
GeoNodesLog::~GeoNodesLog() = default;

GeoTreeLogger::GeoTreeLogger() = default;
GeoTreeLogger::~GeoTreeLogger() = default;

GeoNodeLog::GeoNodeLog() = default;
GeoNodeLog::~GeoNodeLog() = default;

GeoTreeLog::GeoTreeLog(GeoNodesLog *root_log, Vector<GeoTreeLogger *> tree_loggers)
    : root_log_(root_log), tree_loggers_(std::move(tree_loggers))
{
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const ComputeContextHash &hash : tree_logger->children_hashes) {
      children_hashes_.add(hash);
    }
  }
}

GeoTreeLog::~GeoTreeLog() = default;

void GeoTreeLogger::log_value(const bNode &node, const bNodeSocket &socket, const GPointer value)
{
  const CPPType &type = *value.type();

  auto store_logged_value = [&](destruct_ptr<ValueLog> value_log) {
    auto &socket_values = socket.in_out == SOCK_IN ? this->input_socket_values :
                                                     this->output_socket_values;
    socket_values.append(*this->allocator,
                         {node.identifier, socket.index(), std::move(value_log)});
  };

  auto log_generic_value = [&](const CPPType &type, const void *value) {
    void *buffer = this->allocator->allocate(type);
    type.copy_construct(value, buffer);
    store_logged_value(this->allocator->construct<GenericValueLog>(GMutablePointer{type, buffer}));
  };

  if (type.is<bke::SocketValueVariant>()) {
    bke::SocketValueVariant value_variant = *value.get<bke::SocketValueVariant>();
    if (value_variant.valid_for_socket(SOCK_GEOMETRY)) {
      const bke::GeometrySet &geometry = value_variant.get<bke::GeometrySet>();
      store_logged_value(this->allocator->construct<GeometryInfoLog>(geometry));
    }
    else if (value_variant.is_context_dependent_field()) {
      const GField field = value_variant.extract<GField>();
      store_logged_value(this->allocator->construct<FieldInfoLog>(field));
    }
#ifdef WITH_OPENVDB
    else if (value_variant.is_volume_grid()) {
      const bke::GVolumeGrid grid = value_variant.extract<bke::GVolumeGrid>();
      store_logged_value(this->allocator->construct<GridInfoLog>(grid));
    }
#endif
    else if (value_variant.is_list()) {
      const ListPtr list = value_variant.extract<ListPtr>();
      store_logged_value(this->allocator->construct<ListInfoLog>(list.get()));
    }
    else if (value_variant.valid_for_socket(SOCK_BUNDLE)) {
      Vector<BundleValueLog::Item> items;
      if (const BundlePtr bundle = value_variant.extract<BundlePtr>()) {
        for (const Bundle::StoredItem &item : bundle->items()) {
          if (const BundleItemSocketValue *socket_value = std::get_if<BundleItemSocketValue>(
                  &item.value.value))
          {
            items.append({item.key, {socket_value->type}});
          }
          if (const BundleItemInternalValue *internal_value = std::get_if<BundleItemInternalValue>(
                  &item.value.value))
          {
            items.append({item.key, {internal_value->value->type_name()}});
          }
        }
      }
      store_logged_value(this->allocator->construct<BundleValueLog>(std::move(items)));
    }
    else if (value_variant.valid_for_socket(SOCK_CLOSURE)) {
      Vector<ClosureValueLog::Item> inputs;
      Vector<ClosureValueLog::Item> outputs;
      std::optional<ClosureSourceLocation> source_location;
      std::shared_ptr<ClosureEvalLog> eval_log;
      if (const ClosurePtr closure = value_variant.extract<ClosurePtr>()) {
        const ClosureSignature &signature = closure->signature();
        for (const ClosureSignature::Item &item : signature.inputs) {
          inputs.append({item.key, item.type});
        }
        for (const ClosureSignature::Item &item : signature.outputs) {
          outputs.append({item.key, item.type});
        }
        source_location = closure->source_location();
        eval_log = closure->eval_log_ptr();
      }
      store_logged_value(this->allocator->construct<ClosureValueLog>(
          std::move(inputs), std::move(outputs), source_location, eval_log));
    }
    else {
      value_variant.convert_to_single();
      const GPointer value = value_variant.get_single_ptr();
      if (value.type()->is<std::string>()) {
        const std::string &string = *value.get<std::string>();
        store_logged_value(this->allocator->construct<StringLog>(string, *this->allocator));
      }
      else {
        log_generic_value(*value.type(), value.get());
      }
    }
  }
  else {
    log_generic_value(type, value.get());
  }
}

const bke::GeometrySet *ViewerNodeLog::main_geometry() const
{
  main_geometry_cache_mutex_.ensure([&]() {
    for (const Item &item : this->items) {
#ifdef WITH_OPENVDB
      if (item.value.is_volume_grid()) {
        const bke::GVolumeGrid grid = item.value.get<bke::GVolumeGrid>();
        Volume *volume = BKE_id_new_nomain<Volume>(nullptr);
        grid->add_user();
        BKE_volume_grid_add(volume, grid.get());
        main_geometry_cache_ = bke::GeometrySet::from_volume(volume);
        return;
      }
#endif
      if (item.value.is_single() && item.value.get_single_ptr().is_type<bke::GeometrySet>()) {
        main_geometry_cache_ = *item.value.get_single_ptr().get<bke::GeometrySet>();
        return;
      }
    }
  });
  return main_geometry_cache_ ? &*main_geometry_cache_ : nullptr;
}

static bool warning_is_propagated(const NodeWarningPropagation propagation,
                                  const NodeWarningType warning_type)
{
  switch (propagation) {
    case NODE_WARNING_PROPAGATION_ALL:
      return true;
    case NODE_WARNING_PROPAGATION_NONE:
      return false;
    case NODE_WARNING_PROPAGATION_ONLY_ERRORS:
      return warning_type == NodeWarningType::Error;
    case NODE_WARNING_PROPAGATION_ONLY_ERRORS_AND_WARNINGS:
      return ELEM(warning_type, NodeWarningType::Error, NodeWarningType::Warning);
  }
  BLI_assert_unreachable();
  return true;
}

void GeoTreeLog::ensure_node_warnings(const NodesModifierData &nmd)
{
  if (reduced_node_warnings_) {
    return;
  }
  if (!nmd.node_group) {
    reduced_node_warnings_ = true;
    return;
  }
  Map<uint32_t, const bNodeTree *> map;
  BKE_library_foreach_ID_link(
      nullptr,
      &nmd.node_group->id,
      [&](LibraryIDLinkCallbackData *cb_data) {
        if (ID *id = *cb_data->id_pointer) {
          if (GS(id->name) == ID_NT) {
            const bNodeTree *tree = reinterpret_cast<const bNodeTree *>(id);
            map.add(id->session_uid, tree);
          }
        }
        return IDWALK_RET_NOP;
      },
      nullptr,
      IDWALK_READONLY | IDWALK_RECURSE);
  this->ensure_node_warnings(map);
}

void GeoTreeLog::ensure_node_warnings(const Main &bmain)
{
  if (reduced_node_warnings_) {
    return;
  }
  Map<uint32_t, const bNodeTree *> map;
  FOREACH_NODETREE_BEGIN (const_cast<Main *>(&bmain), tree, id) {
    map.add_new(tree->id.session_uid, tree);
  }
  FOREACH_NODETREE_END;
  this->ensure_node_warnings(map);
}

void GeoTreeLog::ensure_node_warnings(
    const Map<uint32_t, const bNodeTree *> &orig_tree_by_session_uid)
{
  if (reduced_node_warnings_) {
    return;
  }
  if (tree_loggers_.is_empty()) {
    return;
  }
  const std::optional<uint32_t> tree_uid = tree_loggers_[0]->tree_orig_session_uid;
  const bNodeTree *tree = tree_uid ? orig_tree_by_session_uid.lookup_default(*tree_uid, nullptr) :
                                     nullptr;

  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const GeoTreeLogger::WarningWithNode &warning : tree_logger->node_warnings) {
      NodeWarningPropagation propagation = NODE_WARNING_PROPAGATION_ALL;
      if (tree) {
        if (const bNode *node = tree->node_by_id(warning.node_id)) {
          propagation = NodeWarningPropagation(node->warning_propagation);
        }
      }
      this->nodes.lookup_or_add_default(warning.node_id).warnings.add(warning.warning);
      if (warning_is_propagated(propagation, warning.warning.type)) {
        this->all_warnings.add(warning.warning);
      }
    }
  }
  for (const ComputeContextHash &child_hash : children_hashes_) {
    GeoTreeLog &child_log = root_log_->get_tree_log(child_hash);
    if (child_log.tree_loggers_.is_empty()) {
      continue;
    }
    const GeoTreeLogger &first_child_logger = *child_log.tree_loggers_[0];
    NodeWarningPropagation propagation = NODE_WARNING_PROPAGATION_ALL;
    const std::optional<int32_t> &caller_node_id = first_child_logger.parent_node_id;
    if (tree && caller_node_id) {
      if (const bNode *caller_node = tree->node_by_id(*caller_node_id)) {
        propagation = NodeWarningPropagation(caller_node->warning_propagation);
      }
    }
    child_log.ensure_node_warnings(orig_tree_by_session_uid);
    if (caller_node_id.has_value()) {
      this->nodes.lookup_or_add_default(*caller_node_id)
          .warnings.add_multiple(child_log.all_warnings);
    }
    for (const NodeWarning &warning : child_log.all_warnings) {
      if (warning_is_propagated(propagation, warning.type)) {
        this->all_warnings.add(warning);
        continue;
      }
    }
  }
  reduced_node_warnings_ = true;
}

void GeoTreeLog::ensure_execution_times()
{
  if (reduced_execution_times_) {
    return;
  }
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const GeoTreeLogger::NodeExecutionTime &timings : tree_logger->node_execution_times) {
      const std::chrono::nanoseconds duration = timings.end - timings.start;
      this->nodes.lookup_or_add_default_as(timings.node_id).execution_time += duration;
    }
    this->execution_time += tree_logger->execution_time;
  }
  reduced_execution_times_ = true;
}

void GeoTreeLog::ensure_socket_values()
{
  if (reduced_socket_values_) {
    return;
  }
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const GeoTreeLogger::SocketValueLog &value_log_data : tree_logger->input_socket_values) {
      this->nodes.lookup_or_add_as(value_log_data.node_id)
          .input_values_.add(value_log_data.socket_index, value_log_data.value.get());
    }
    for (const GeoTreeLogger::SocketValueLog &value_log_data : tree_logger->output_socket_values) {
      this->nodes.lookup_or_add_as(value_log_data.node_id)
          .output_values_.add(value_log_data.socket_index, value_log_data.value.get());
    }
  }
  reduced_socket_values_ = true;
}

void GeoTreeLog::ensure_viewer_node_logs()
{
  if (reduced_viewer_node_logs_) {
    return;
  }
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const GeoTreeLogger::ViewerNodeLogWithNode &viewer_log : tree_logger->viewer_node_logs) {
      this->viewer_node_logs.add(viewer_log.node_id, viewer_log.viewer_log.get());
    }
  }
  reduced_viewer_node_logs_ = true;
}

void GeoTreeLog::ensure_existing_attributes()
{
  if (reduced_existing_attributes_) {
    return;
  }
  this->ensure_socket_values();

  auto handle_value_log = [&](const ValueLog &value_log) {
    const GeometryInfoLog *geo_log = dynamic_cast<const GeometryInfoLog *>(&value_log);
    if (geo_log == nullptr) {
      return;
    }
    for (const GeometryAttributeInfo &attribute : geo_log->attributes) {
      this->existing_attributes.append(&attribute);
    }
  };

  for (const GeoNodeLog &node_log : this->nodes.values()) {
    for (const ValueLog *value_log : node_log.input_values_.values()) {
      handle_value_log(*value_log);
    }
    for (const ValueLog *value_log : node_log.output_values_.values()) {
      handle_value_log(*value_log);
    }
  }
  reduced_existing_attributes_ = true;
}

void GeoTreeLog::ensure_used_named_attributes()
{
  if (reduced_used_named_attributes_) {
    return;
  }

  auto add_attribute = [&](const int32_t node_id,
                           const StringRefNull attribute_name,
                           const NamedAttributeUsage &usage) {
    this->nodes.lookup_or_add_default(node_id).used_named_attributes.lookup_or_add(attribute_name,
                                                                                   usage) |= usage;
    this->used_named_attributes.lookup_or_add_as(attribute_name, usage) |= usage;
  };

  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const GeoTreeLogger::AttributeUsageWithNode &item : tree_logger->used_named_attributes) {
      add_attribute(item.node_id, item.attribute_name, item.usage);
    }
  }
  for (const ComputeContextHash &child_hash : children_hashes_) {
    GeoTreeLog &child_log = root_log_->get_tree_log(child_hash);
    if (child_log.tree_loggers_.is_empty()) {
      continue;
    }
    child_log.ensure_used_named_attributes();
    if (const std::optional<int32_t> &parent_node_id = child_log.tree_loggers_[0]->parent_node_id)
    {
      for (const auto item : child_log.used_named_attributes.items()) {
        add_attribute(*parent_node_id, item.key, item.value);
      }
    }
  }
  reduced_used_named_attributes_ = true;
}

void GeoTreeLog::ensure_debug_messages()
{
  if (reduced_debug_messages_) {
    return;
  }
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const GeoTreeLogger::DebugMessage &debug_message : tree_logger->debug_messages) {
      this->nodes.lookup_or_add_as(debug_message.node_id)
          .debug_messages.append(debug_message.message);
    }
  }
  reduced_debug_messages_ = true;
}

void GeoTreeLog::ensure_evaluated_gizmo_nodes()
{
  if (reduced_evaluated_gizmo_nodes_) {
    return;
  }
  for (const GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const GeoTreeLogger::EvaluatedGizmoNode &evaluated_gizmo :
         tree_logger->evaluated_gizmo_nodes)
    {
      this->evaluated_gizmo_nodes.add(evaluated_gizmo.node_id);
    }
  }

  reduced_evaluated_gizmo_nodes_ = true;
}

void GeoTreeLog::ensure_layer_names()
{
  if (reduced_layer_names_) {
    return;
  }

  this->ensure_socket_values();

  auto handle_value_log = [&](const ValueLog &value_log) {
    const GeometryInfoLog *geo_log = dynamic_cast<const GeometryInfoLog *>(&value_log);
    if (geo_log == nullptr || !geo_log->grease_pencil_info.has_value()) {
      return;
    }
    for (const std::string &name : geo_log->grease_pencil_info->layer_names) {
      this->all_layer_names.append(name);
    }
  };

  for (const GeoNodeLog &node_log : this->nodes.values()) {
    for (const ValueLog *value_log : node_log.input_values_.values()) {
      handle_value_log(*value_log);
    }
    for (const ValueLog *value_log : node_log.output_values_.values()) {
      handle_value_log(*value_log);
    }
  }

  reduced_layer_names_ = true;
}

ValueLog *GeoTreeLog::find_socket_value_log(const bNodeSocket &query_socket)
{
  /**
   * Geometry nodes does not log values for every socket. That would produce a lot of redundant
   * data, because often many linked sockets have the same value. To find the logged value for a
   * socket one might have to look at linked sockets as well.
   */

  BLI_assert(reduced_socket_values_);
  if (query_socket.is_multi_input()) {
    /* Not supported currently. */
    return nullptr;
  }

  Set<const bNodeSocket *> added_sockets;
  Stack<const bNodeSocket *> sockets_to_check;
  sockets_to_check.push(&query_socket);
  added_sockets.add(&query_socket);
  const bNodeTree &tree = query_socket.owner_tree();

  while (!sockets_to_check.is_empty()) {
    const bNodeSocket &socket = *sockets_to_check.pop();
    const bNode &node = socket.owner_node();
    if (GeoNodeLog *node_log = this->nodes.lookup_ptr(node.identifier)) {
      ValueLog *value_log = socket.is_input() ?
                                node_log->input_values_.lookup_default(socket.index(), nullptr) :
                                node_log->output_values_.lookup_default(socket.index(), nullptr);
      if (value_log != nullptr) {
        return value_log;
      }
    }

    if (socket.is_input()) {
      const Span<const bNodeLink *> links = socket.directly_linked_links();
      for (const bNodeLink *link : links) {
        const bNodeSocket &from_socket = *link->fromsock;
        if (added_sockets.add(&from_socket)) {
          sockets_to_check.push(&from_socket);
        }
      }
    }
    else {
      if (node.is_reroute()) {
        const bNodeSocket &input_socket = node.input_socket(0);
        if (added_sockets.add(&input_socket)) {
          sockets_to_check.push(&input_socket);
        }
        const Span<const bNodeLink *> links = input_socket.directly_linked_links();
        for (const bNodeLink *link : links) {
          const bNodeSocket &from_socket = *link->fromsock;
          if (added_sockets.add(&from_socket)) {
            sockets_to_check.push(&from_socket);
          }
        }
      }
      else if (node.is_muted()) {
        if (const bNodeSocket *input_socket = socket.internal_link_input()) {
          if (added_sockets.add(input_socket)) {
            sockets_to_check.push(input_socket);
          }
          const Span<const bNodeLink *> links = input_socket->directly_linked_links();
          for (const bNodeLink *link : links) {
            const bNodeSocket &from_socket = *link->fromsock;
            if (added_sockets.add(&from_socket)) {
              sockets_to_check.push(&from_socket);
            }
          }
        }
      }
      else if (node.is_group_input()) {
        const int index = socket.index();
        /* Check if the value is stored for any other group input node. */
        for (const bNode *other_group_input : tree.group_input_nodes()) {
          const bNodeSocket &other_socket = other_group_input->output_socket(index);
          if (added_sockets.add(&other_socket)) {
            sockets_to_check.push(&other_socket);
          }
        }
      }
    }
  }

  return nullptr;
}

bool GeoTreeLog::try_convert_primitive_socket_value(const GenericValueLog &value_log,
                                                    const CPPType &dst_type,
                                                    void *dst)
{
  const void *src_value = value_log.value.get();
  if (!src_value) {
    return false;
  }
  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  const CPPType &src_type = *value_log.value.type();
  if (!conversions.is_convertible(src_type, dst_type) && src_type != dst_type) {
    return false;
  }
  dst_type.destruct(dst);
  conversions.convert_to_uninitialized(src_type, dst_type, src_value, dst);
  return true;
}

static std::optional<uint32_t> get_original_session_uid(const ID *id)
{
  if (!id) {
    return {};
  }
  if (DEG_is_original(id)) {
    return id->session_uid;
  }
  if (const ID *id_orig = DEG_get_original(id)) {
    return id_orig->session_uid;
  }
  return {};
}

GeoTreeLogger &GeoNodesLog::get_local_tree_logger(const ComputeContext &compute_context)
{
  LocalData &local_data = data_per_thread_.local();
  Map<ComputeContextHash, destruct_ptr<GeoTreeLogger>> &local_tree_loggers =
      local_data.tree_logger_by_context;
  destruct_ptr<GeoTreeLogger> &tree_logger_ptr = local_tree_loggers.lookup_or_add_default(
      compute_context.hash());
  if (tree_logger_ptr) {
    return *tree_logger_ptr;
  }
  tree_logger_ptr = local_data.allocator.construct<GeoTreeLogger>();
  GeoTreeLogger &tree_logger = *tree_logger_ptr;
  tree_logger.allocator = &local_data.allocator;
  const ComputeContext *parent_compute_context = compute_context.parent();
  std::optional<uint32_t> parent_tree_session_uid;
  if (parent_compute_context != nullptr) {
    tree_logger.parent_hash = parent_compute_context->hash();
    GeoTreeLogger &parent_logger = this->get_local_tree_logger(*parent_compute_context);
    parent_logger.children_hashes.append(compute_context.hash());
    parent_tree_session_uid = parent_logger.tree_orig_session_uid;
  }
  if (const auto *context = dynamic_cast<const bke::GroupNodeComputeContext *>(&compute_context)) {
    tree_logger.parent_node_id.emplace(context->node_id());
    if (const bNode *caller_node = context->node()) {
      tree_logger.tree_orig_session_uid = get_original_session_uid(caller_node->id);
    }
  }
  else if (const auto *context = dynamic_cast<const bke::RepeatZoneComputeContext *>(
               &compute_context))
  {
    tree_logger.parent_node_id.emplace(context->output_node_id());
    tree_logger.tree_orig_session_uid = parent_tree_session_uid;
  }
  else if (const auto *context =
               dynamic_cast<const bke::ForeachGeometryElementZoneComputeContext *>(
                   &compute_context))
  {
    tree_logger.parent_node_id.emplace(context->output_node_id());
    tree_logger.tree_orig_session_uid = parent_tree_session_uid;
  }
  else if (const auto *context = dynamic_cast<const bke::SimulationZoneComputeContext *>(
               &compute_context))
  {
    tree_logger.parent_node_id.emplace(context->output_node_id());
    tree_logger.tree_orig_session_uid = parent_tree_session_uid;
  }
  else if (const auto *context = dynamic_cast<const bke::EvaluateClosureComputeContext *>(
               &compute_context))
  {
    tree_logger.parent_node_id.emplace(context->node_id());
    const std::optional<nodes::ClosureSourceLocation> &location =
        context->closure_source_location();
    if (location.has_value()) {
      BLI_assert(DEG_is_evaluated(location->tree));
      tree_logger.tree_orig_session_uid = DEG_get_original_id(&location->tree->id)->session_uid;
    }
  }
  else if (const auto *context = dynamic_cast<const bke::ModifierComputeContext *>(
               &compute_context))
  {
    if (const NodesModifierData *nmd = context->nmd()) {
      tree_logger.tree_orig_session_uid = get_original_session_uid(
          reinterpret_cast<const ID *>(nmd->node_group));
    }
  }
  else if (const auto *context = dynamic_cast<const bke::OperatorComputeContext *>(
               &compute_context))
  {
    if (const bNodeTree *tree = context->tree()) {
      tree_logger.tree_orig_session_uid = tree->id.session_uid;
    }
  }
  return tree_logger;
}

GeoTreeLog &GeoNodesLog::get_tree_log(const ComputeContextHash &compute_context_hash)
{
  GeoTreeLog &reduced_tree_log = *tree_logs_.lookup_or_add_cb(compute_context_hash, [&]() {
    Vector<GeoTreeLogger *> tree_logs;
    for (LocalData &local_data : data_per_thread_) {
      destruct_ptr<GeoTreeLogger> *tree_log = local_data.tree_logger_by_context.lookup_ptr(
          compute_context_hash);
      if (tree_log != nullptr) {
        tree_logs.append(tree_log->get());
      }
    }
    return std::make_unique<GeoTreeLog>(this, std::move(tree_logs));
  });
  return reduced_tree_log;
}

static void find_tree_zone_hash_recursive(
    const bNodeTreeZone &zone,
    bke::ComputeContextCache &compute_context_cache,
    const ComputeContext *current,
    Map<const bNodeTreeZone *, ComputeContextHash> &r_hash_by_zone)
{
  current = ed::space_node::compute_context_for_zone(zone, compute_context_cache, current);
  if (!current) {
    return;
  }
  r_hash_by_zone.add_new(&zone, current->hash());
  for (const bNodeTreeZone *child_zone : zone.child_zones) {
    find_tree_zone_hash_recursive(*child_zone, compute_context_cache, current, r_hash_by_zone);
  }
}

Map<const bNodeTreeZone *, ComputeContextHash> GeoNodesLog::
    get_context_hash_by_zone_for_node_editor(const SpaceNode &snode,
                                             bke::ComputeContextCache &compute_context_cache)
{
  const ComputeContext *current = ed::space_node::compute_context_for_edittree(
      snode, compute_context_cache);
  if (!current) {
    return {};
  }

  const bNodeTreeZones *tree_zones = snode.edittree->zones();
  if (tree_zones == nullptr) {
    return {};
  }
  Map<const bNodeTreeZone *, ComputeContextHash> hash_by_zone;
  hash_by_zone.add_new(nullptr, current->hash());
  for (const bNodeTreeZone *zone : tree_zones->root_zones) {
    find_tree_zone_hash_recursive(*zone, compute_context_cache, current, hash_by_zone);
  }
  return hash_by_zone;
}

static GeoNodesLog *get_root_log(const SpaceNode &snode)
{
  if (!ED_node_is_geometry(&snode)) {
    return nullptr;
  }

  switch (SpaceNodeGeometryNodesType(snode.node_tree_sub_type)) {
    case SNODE_GEOMETRY_MODIFIER: {
      std::optional<ed::space_node::ObjectAndModifier> object_and_modifier =
          ed::space_node::get_modifier_for_node_editor(snode);
      if (!object_and_modifier) {
        return {};
      }
      return object_and_modifier->nmd->runtime->eval_log.get();
    }
    case SNODE_GEOMETRY_TOOL: {
      const ed::geometry::GeoOperatorLog &log =
          ed::geometry::node_group_operator_static_eval_log();
      if (snode.selected_node_group->id.name + 2 != log.node_group_name) {
        return {};
      }
      return log.log.get();
    }
  }
  return nullptr;
}

ContextualGeoTreeLogs GeoNodesLog::get_contextual_tree_logs(const SpaceNode &snode)
{
  GeoNodesLog *log = get_root_log(snode);
  if (!log) {
    return {};
  }
  bke::ComputeContextCache compute_context_cache;
  const Map<const bNodeTreeZone *, ComputeContextHash> hash_by_zone =
      GeoNodesLog::get_context_hash_by_zone_for_node_editor(snode, compute_context_cache);
  Map<const bke::bNodeTreeZone *, GeoTreeLog *> tree_logs_by_zone;
  for (const auto item : hash_by_zone.items()) {
    GeoTreeLog &tree_log = log->get_tree_log(item.value);
    tree_logs_by_zone.add(item.key, &tree_log);
  }
  return {tree_logs_by_zone};
}

const ViewerNodeLog *GeoNodesLog::find_viewer_node_log_for_path(const ViewerPath &viewer_path)
{
  const std::optional<ed::viewer_path::ViewerPathForGeometryNodesViewer> parsed_path =
      ed::viewer_path::parse_geometry_nodes_viewer(viewer_path);
  if (!parsed_path.has_value()) {
    return nullptr;
  }
  const Object *object = parsed_path->object;
  NodesModifierData *nmd = nullptr;
  LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
    if (md->persistent_uid == parsed_path->modifier_uid) {
      if (md->type == eModifierType_Nodes) {
        nmd = reinterpret_cast<NodesModifierData *>(md);
      }
    }
  }
  if (nmd == nullptr) {
    return nullptr;
  }
  if (!nmd->runtime->eval_log) {
    return nullptr;
  }
  nodes::geo_eval_log::GeoNodesLog *root_log = nmd->runtime->eval_log.get();

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *compute_context = &compute_context_cache.for_modifier(nullptr, *nmd);
  for (const ViewerPathElem *elem : parsed_path->node_path) {
    compute_context = ed::viewer_path::compute_context_for_viewer_path_elem(
        *elem, compute_context_cache, compute_context);
    if (!compute_context) {
      return nullptr;
    }
  }
  const ComputeContextHash context_hash = compute_context->hash();
  nodes::geo_eval_log::GeoTreeLog &tree_log = root_log->get_tree_log(context_hash);
  tree_log.ensure_viewer_node_logs();

  const ViewerNodeLog *viewer_log = tree_log.viewer_node_logs.lookup_default(
      parsed_path->viewer_node_id, nullptr);
  return viewer_log;
}

ContextualGeoTreeLogs::ContextualGeoTreeLogs(
    Map<const bke::bNodeTreeZone *, GeoTreeLog *> tree_logs_by_zone)
    : tree_logs_by_zone_(std::move(tree_logs_by_zone))
{
}

GeoTreeLog *ContextualGeoTreeLogs::get_main_tree_log(const bke::bNodeTreeZone *zone) const
{
  return tree_logs_by_zone_.lookup_default(zone, nullptr);
}

GeoTreeLog *ContextualGeoTreeLogs::get_main_tree_log(const bNode &node) const
{
  const bNodeTree &tree = node.owner_tree();
  const bke::bNodeTreeZones *zones = tree.zones();
  if (!zones) {
    return nullptr;
  }
  const bke::bNodeTreeZone *zone = zones->get_zone_by_node(node.identifier);
  return this->get_main_tree_log(zone);
}

GeoTreeLog *ContextualGeoTreeLogs::get_main_tree_log(const bNodeSocket &socket) const
{
  const bNodeTree &tree = socket.owner_tree();
  const bke::bNodeTreeZones *zones = tree.zones();
  if (!zones) {
    return nullptr;
  }
  const bke::bNodeTreeZone *zone = zones->get_zone_by_socket(socket);
  return this->get_main_tree_log(zone);
}

void ContextualGeoTreeLogs::foreach_tree_log(FunctionRef<void(GeoTreeLog &)> callback) const
{
  for (GeoTreeLog *tree_log : tree_logs_by_zone_.values()) {
    if (tree_log) {
      callback(*tree_log);
    }
  }
}

}  // namespace blender::nodes::geo_eval_log
