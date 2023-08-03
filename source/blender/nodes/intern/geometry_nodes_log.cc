/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_geometry_nodes_log.hh"

#include "BKE_compute_contexts.hh"
#include "BKE_curves.hh"
#include "BKE_node_runtime.hh"
#include "BKE_viewer_path.h"

#include "FN_field_cpp_type.hh"

#include "DNA_modifier_types.h"
#include "DNA_space_types.h"

#include "ED_viewer_path.hh"

#include "MOD_nodes.hh"

namespace blender::nodes::geo_eval_log {

using bke::bNodeTreeZone;
using bke::bNodeTreeZones;
using fn::FieldInput;
using fn::FieldInputs;

GenericValueLog::~GenericValueLog()
{
  this->value.destruct();
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
  static std::array all_component_types = {bke::GeometryComponent::Type::Curve,
                                           bke::GeometryComponent::Type::Instance,
                                           bke::GeometryComponent::Type::Mesh,
                                           bke::GeometryComponent::Type::PointCloud,
                                           bke::GeometryComponent::Type::Volume};

  /* Keep track handled attribute names to make sure that we do not return the same name twice.
   * Currently #GeometrySet::attribute_foreach does not do that. Note that this will merge
   * attributes with the same name but different domains or data types on separate components. */
  Set<StringRef> names;

  geometry_set.attribute_foreach(
      all_component_types,
      true,
      [&](const bke::AttributeIDRef &attribute_id,
          const bke::AttributeMetaData &meta_data,
          const bke::GeometryComponent & /*component*/) {
        if (!attribute_id.is_anonymous() && names.add(attribute_id.name())) {
          this->attributes.append({attribute_id.name(), meta_data.domain, meta_data.data_type});
        }
      });

  for (const bke::GeometryComponent *component : geometry_set.get_components()) {
    this->component_types.append(component->type());
    switch (component->type()) {
      case bke::GeometryComponent::Type::Mesh: {
        const auto &mesh_component = *static_cast<const bke::MeshComponent *>(component);
        MeshInfo &info = this->mesh_info.emplace();
        info.verts_num = mesh_component.attribute_domain_size(ATTR_DOMAIN_POINT);
        info.edges_num = mesh_component.attribute_domain_size(ATTR_DOMAIN_EDGE);
        info.faces_num = mesh_component.attribute_domain_size(ATTR_DOMAIN_FACE);
        break;
      }
      case bke::GeometryComponent::Type::Curve: {
        const auto &curve_component = *static_cast<const bke::CurveComponent *>(component);
        CurveInfo &info = this->curve_info.emplace();
        info.points_num = curve_component.attribute_domain_size(ATTR_DOMAIN_POINT);
        info.splines_num = curve_component.attribute_domain_size(ATTR_DOMAIN_CURVE);
        break;
      }
      case bke::GeometryComponent::Type::PointCloud: {
        const auto &pointcloud_component = *static_cast<const bke::PointCloudComponent *>(
            component);
        PointCloudInfo &info = this->pointcloud_info.emplace();
        info.points_num = pointcloud_component.attribute_domain_size(ATTR_DOMAIN_POINT);
        break;
      }
      case bke::GeometryComponent::Type::Instance: {
        const auto &instances_component = *static_cast<const bke::InstancesComponent *>(component);
        InstancesInfo &info = this->instances_info.emplace();
        info.instances_num = instances_component.attribute_domain_size(ATTR_DOMAIN_INSTANCE);
        break;
      }
      case bke::GeometryComponent::Type::Edit: {
        const auto &edit_component = *static_cast<const bke::GeometryComponentEditData *>(
            component);
        if (const bke::CurvesEditHints *curve_edit_hints = edit_component.curves_edit_hints_.get())
        {
          EditDataInfo &info = this->edit_data_info.emplace();
          info.has_deform_matrices = curve_edit_hints->deform_mats.has_value();
          info.has_deformed_positions = curve_edit_hints->positions.has_value();
        }
        break;
      }
      case bke::GeometryComponent::Type::Volume: {
        break;
      }
      case bke::GeometryComponent::Type::GreasePencil: {
        /* TODO. Do nothing for now. */
        break;
      }
    }
  }
}

/* Avoid generating these in every translation unit. */
GeoModifierLog::GeoModifierLog() = default;
GeoModifierLog::~GeoModifierLog() = default;

GeoTreeLogger::GeoTreeLogger() = default;
GeoTreeLogger::~GeoTreeLogger() = default;

GeoNodeLog::GeoNodeLog() = default;
GeoNodeLog::~GeoNodeLog() = default;

GeoTreeLog::GeoTreeLog(GeoModifierLog *modifier_log, Vector<GeoTreeLogger *> tree_loggers)
    : modifier_log_(modifier_log), tree_loggers_(std::move(tree_loggers))
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
    socket_values.append({node.identifier, socket.index(), std::move(value_log)});
  };

  auto log_generic_value = [&](const CPPType &type, const void *value) {
    void *buffer = this->allocator->allocate(type.size(), type.alignment());
    type.copy_construct(value, buffer);
    store_logged_value(this->allocator->construct<GenericValueLog>(GMutablePointer{type, buffer}));
  };

  if (type.is<bke::GeometrySet>()) {
    const bke::GeometrySet &geometry = *value.get<bke::GeometrySet>();
    store_logged_value(this->allocator->construct<GeometryInfoLog>(geometry));
  }
  else if (const auto *value_or_field_type = fn::ValueOrFieldCPPType::get_from_self(type)) {
    const void *value_or_field = value.get();
    const CPPType &base_type = value_or_field_type->value;
    if (value_or_field_type->is_field(value_or_field)) {
      const GField *field = value_or_field_type->get_field_ptr(value_or_field);
      if (field->node().depends_on_input()) {
        store_logged_value(this->allocator->construct<FieldInfoLog>(*field));
      }
      else {
        BUFFER_FOR_CPP_TYPE_VALUE(base_type, value);
        fn::evaluate_constant_field(*field, value);
        log_generic_value(base_type, value);
      }
    }
    else {
      const void *value = value_or_field_type->get_value_ptr(value_or_field);
      log_generic_value(base_type, value);
    }
  }
  else {
    log_generic_value(type, value.get());
  }
}

void GeoTreeLogger::log_viewer_node(const bNode &viewer_node, bke::GeometrySet geometry)
{
  destruct_ptr<ViewerNodeLog> log = this->allocator->construct<ViewerNodeLog>();
  log->geometry = std::move(geometry);
  log->geometry.ensure_owns_direct_data();
  this->viewer_node_logs.append({viewer_node.identifier, std::move(log)});
}

void GeoTreeLog::ensure_node_warnings()
{
  if (reduced_node_warnings_) {
    return;
  }
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const GeoTreeLogger::WarningWithNode &warnings : tree_logger->node_warnings) {
      this->nodes.lookup_or_add_default(warnings.node_id).warnings.append(warnings.warning);
      this->all_warnings.append(warnings.warning);
    }
  }
  for (const ComputeContextHash &child_hash : children_hashes_) {
    GeoTreeLog &child_log = modifier_log_->get_tree_log(child_hash);
    child_log.ensure_node_warnings();
    const std::optional<int32_t> &group_node_id = child_log.tree_loggers_[0]->group_node_id;
    if (group_node_id.has_value()) {
      this->nodes.lookup_or_add_default(*group_node_id).warnings.extend(child_log.all_warnings);
    }
    this->all_warnings.extend(child_log.all_warnings);
  }
  reduced_node_warnings_ = true;
}

void GeoTreeLog::ensure_node_run_time()
{
  if (reduced_node_run_times_) {
    return;
  }
  for (GeoTreeLogger *tree_logger : tree_loggers_) {
    for (const GeoTreeLogger::NodeExecutionTime &timings : tree_logger->node_execution_times) {
      const std::chrono::nanoseconds duration = timings.end - timings.start;
      this->nodes.lookup_or_add_default_as(timings.node_id).run_time += duration;
      this->run_time_sum += duration;
    }
  }
  for (const ComputeContextHash &child_hash : children_hashes_) {
    GeoTreeLog &child_log = modifier_log_->get_tree_log(child_hash);
    child_log.ensure_node_run_time();
    const std::optional<int32_t> &group_node_id = child_log.tree_loggers_[0]->group_node_id;
    if (group_node_id.has_value()) {
      this->nodes.lookup_or_add_default(*group_node_id).run_time += child_log.run_time_sum;
    }
    this->run_time_sum += child_log.run_time_sum;
  }
  reduced_node_run_times_ = true;
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

  Set<StringRef> names;

  auto handle_value_log = [&](const ValueLog &value_log) {
    const GeometryInfoLog *geo_log = dynamic_cast<const GeometryInfoLog *>(&value_log);
    if (geo_log == nullptr) {
      return;
    }
    for (const GeometryAttributeInfo &attribute : geo_log->attributes) {
      if (names.add(attribute.name)) {
        this->existing_attributes.append(&attribute);
      }
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
    GeoTreeLog &child_log = modifier_log_->get_tree_log(child_hash);
    child_log.ensure_used_named_attributes();
    if (const std::optional<int32_t> &group_node_id = child_log.tree_loggers_[0]->group_node_id) {
      for (const auto item : child_log.used_named_attributes.items()) {
        add_attribute(*group_node_id, item.key, item.value);
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

ValueLog *GeoTreeLog::find_socket_value_log(const bNodeSocket &query_socket)
{
  /**
   * Geometry nodes does not log values for every socket. That would produce a lot of redundant
   * data,because often many linked sockets have the same value. To find the logged value for a
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
    }
  }

  return nullptr;
}

GeoTreeLogger &GeoModifierLog::get_local_tree_logger(const ComputeContext &compute_context)
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
  if (parent_compute_context != nullptr) {
    tree_logger.parent_hash = parent_compute_context->hash();
    GeoTreeLogger &parent_logger = this->get_local_tree_logger(*parent_compute_context);
    parent_logger.children_hashes.append(compute_context.hash());
  }
  if (const bke::NodeGroupComputeContext *node_group_compute_context =
          dynamic_cast<const bke::NodeGroupComputeContext *>(&compute_context))
  {
    tree_logger.group_node_id.emplace(node_group_compute_context->node_id());
  }
  return tree_logger;
}

GeoTreeLog &GeoModifierLog::get_tree_log(const ComputeContextHash &compute_context_hash)
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

struct ObjectAndModifier {
  const Object *object;
  const NodesModifierData *nmd;
};

static std::optional<ObjectAndModifier> get_modifier_for_node_editor(const SpaceNode &snode)
{
  if (snode.id == nullptr) {
    return std::nullopt;
  }
  if (GS(snode.id->name) != ID_OB) {
    return std::nullopt;
  }
  const Object *object = reinterpret_cast<Object *>(snode.id);
  const NodesModifierData *used_modifier = nullptr;
  if (snode.flag & SNODE_PIN) {
    LISTBASE_FOREACH (const ModifierData *, md, &object->modifiers) {
      if (md->type == eModifierType_Nodes) {
        const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
        /* Would be good to store the name of the pinned modifier in the node editor. */
        if (nmd->node_group == snode.nodetree) {
          used_modifier = nmd;
          break;
        }
      }
    }
  }
  else {
    LISTBASE_FOREACH (const ModifierData *, md, &object->modifiers) {
      if (md->type == eModifierType_Nodes) {
        const NodesModifierData *nmd = reinterpret_cast<const NodesModifierData *>(md);
        if (nmd->node_group == snode.nodetree) {
          if (md->flag & eModifierFlag_Active) {
            used_modifier = nmd;
            break;
          }
        }
      }
    }
  }
  if (used_modifier == nullptr) {
    return std::nullopt;
  }
  return ObjectAndModifier{object, used_modifier};
}

static void find_tree_zone_hash_recursive(
    const bNodeTreeZone &zone,
    ComputeContextBuilder &compute_context_builder,
    Map<const bNodeTreeZone *, ComputeContextHash> &r_hash_by_zone)
{
  switch (zone.output_node->type) {
    case GEO_NODE_SIMULATION_OUTPUT: {
      compute_context_builder.push<bke::SimulationZoneComputeContext>(*zone.output_node);
      break;
    }
    case GEO_NODE_REPEAT_OUTPUT: {
      /* Only show data from the first iteration for now. */
      const int iteration = 0;
      compute_context_builder.push<bke::RepeatZoneComputeContext>(*zone.output_node, iteration);
      break;
    }
  }
  r_hash_by_zone.add_new(&zone, compute_context_builder.hash());
  for (const bNodeTreeZone *child_zone : zone.child_zones) {
    find_tree_zone_hash_recursive(*child_zone, compute_context_builder, r_hash_by_zone);
  }
  compute_context_builder.pop();
}

Map<const bNodeTreeZone *, ComputeContextHash> GeoModifierLog::
    get_context_hash_by_zone_for_node_editor(const SpaceNode &snode, StringRefNull modifier_name)
{
  const Vector<const bNodeTreePath *> tree_path = snode.treepath;
  if (tree_path.is_empty()) {
    return {};
  }

  ComputeContextBuilder compute_context_builder;
  compute_context_builder.push<bke::ModifierComputeContext>(modifier_name);

  for (const int i : tree_path.index_range().drop_back(1)) {
    bNodeTree *tree = tree_path[i]->nodetree;
    const char *group_node_name = tree_path[i + 1]->node_name;
    const bNode *group_node = nodeFindNodebyName(tree, group_node_name);
    if (group_node == nullptr) {
      return {};
    }
    const bNodeTreeZones *tree_zones = tree->zones();
    if (tree_zones == nullptr) {
      return {};
    }
    const Vector<const bNodeTreeZone *> zone_stack = tree_zones->get_zone_stack_for_node(
        group_node->identifier);
    for (const bNodeTreeZone *zone : zone_stack) {
      switch (zone->output_node->type) {
        case GEO_NODE_SIMULATION_OUTPUT: {
          compute_context_builder.push<bke::SimulationZoneComputeContext>(*zone->output_node);
          break;
        }
        case GEO_NODE_REPEAT_OUTPUT: {
          /* Only show data from the first iteration for now. */
          const int repeat_iteration = 0;
          compute_context_builder.push<bke::RepeatZoneComputeContext>(*zone->output_node,
                                                                      repeat_iteration);
          break;
        }
      }
    }
    compute_context_builder.push<bke::NodeGroupComputeContext>(*group_node);
  }

  const bNodeTreeZones *tree_zones = snode.edittree->zones();
  if (tree_zones == nullptr) {
    return {};
  }
  Map<const bNodeTreeZone *, ComputeContextHash> hash_by_zone;
  hash_by_zone.add_new(nullptr, compute_context_builder.hash());
  for (const bNodeTreeZone *zone : tree_zones->root_zones) {
    find_tree_zone_hash_recursive(*zone, compute_context_builder, hash_by_zone);
  }
  return hash_by_zone;
}

Map<const bNodeTreeZone *, GeoTreeLog *> GeoModifierLog::get_tree_log_by_zone_for_node_editor(
    const SpaceNode &snode)
{
  std::optional<ObjectAndModifier> object_and_modifier = get_modifier_for_node_editor(snode);
  if (!object_and_modifier) {
    return {};
  }
  GeoModifierLog *modifier_log = object_and_modifier->nmd->runtime->eval_log.get();
  if (modifier_log == nullptr) {
    return {};
  }
  const Map<const bNodeTreeZone *, ComputeContextHash> hash_by_zone =
      GeoModifierLog::get_context_hash_by_zone_for_node_editor(
          snode, object_and_modifier->nmd->modifier.name);
  Map<const bNodeTreeZone *, GeoTreeLog *> log_by_zone;
  for (const auto item : hash_by_zone.items()) {
    GeoTreeLog &tree_log = modifier_log->get_tree_log(item.value);
    log_by_zone.add(item.key, &tree_log);
  }
  return log_by_zone;
}

const ViewerNodeLog *GeoModifierLog::find_viewer_node_log_for_path(const ViewerPath &viewer_path)
{
  const std::optional<ed::viewer_path::ViewerPathForGeometryNodesViewer> parsed_path =
      ed::viewer_path::parse_geometry_nodes_viewer(viewer_path);
  if (!parsed_path.has_value()) {
    return nullptr;
  }
  const Object *object = parsed_path->object;
  NodesModifierData *nmd = nullptr;
  LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
    if (md->name == parsed_path->modifier_name) {
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
  nodes::geo_eval_log::GeoModifierLog *modifier_log = nmd->runtime->eval_log.get();

  ComputeContextBuilder compute_context_builder;
  compute_context_builder.push<bke::ModifierComputeContext>(parsed_path->modifier_name);
  for (const ViewerPathElem *elem : parsed_path->node_path) {
    switch (elem->type) {
      case VIEWER_PATH_ELEM_TYPE_GROUP_NODE: {
        const auto &typed_elem = *reinterpret_cast<const GroupNodeViewerPathElem *>(elem);
        compute_context_builder.push<bke::NodeGroupComputeContext>(typed_elem.node_id);
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE: {
        const auto &typed_elem = *reinterpret_cast<const SimulationZoneViewerPathElem *>(elem);
        compute_context_builder.push<bke::SimulationZoneComputeContext>(
            typed_elem.sim_output_node_id);
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_REPEAT_ZONE: {
        const auto &typed_elem = *reinterpret_cast<const RepeatZoneViewerPathElem *>(elem);
        compute_context_builder.push<bke::RepeatZoneComputeContext>(
            typed_elem.repeat_output_node_id, typed_elem.iteration);
        break;
      }
      default: {
        BLI_assert_unreachable();
        break;
      }
    }
  }
  const ComputeContextHash context_hash = compute_context_builder.hash();
  nodes::geo_eval_log::GeoTreeLog &tree_log = modifier_log->get_tree_log(context_hash);
  tree_log.ensure_viewer_node_logs();

  const ViewerNodeLog *viewer_log = tree_log.viewer_node_logs.lookup_default(
      parsed_path->viewer_node_id, nullptr);
  return viewer_log;
}

}  // namespace blender::nodes::geo_eval_log
