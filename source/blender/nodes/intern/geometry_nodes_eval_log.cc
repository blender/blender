/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "NOD_geometry_nodes_eval_log.hh"

#include "BKE_geometry_set_instances.hh"

#include "DNA_modifier_types.h"
#include "DNA_space_types.h"

#include "FN_field_cpp_type.hh"

#include "BLT_translation.h"

namespace blender::nodes::geometry_nodes_eval_log {

using fn::CPPType;
using fn::FieldCPPType;
using fn::FieldInput;
using fn::GField;

ModifierLog::ModifierLog(GeoLogger &logger)
    : input_geometry_log_(std::move(logger.input_geometry_log_)),
      output_geometry_log_(std::move(logger.output_geometry_log_))
{
  root_tree_logs_ = allocator_.construct<TreeLog>();

  LogByTreeContext log_by_tree_context;

  /* Combine all the local loggers that have been used by separate threads. */
  for (LocalGeoLogger &local_logger : logger) {
    /* Take ownership of the allocator. */
    logger_allocators_.append(std::move(local_logger.allocator_));

    for (ValueOfSockets &value_of_sockets : local_logger.values_) {
      ValueLog *value_log = value_of_sockets.value.get();

      /* Take centralized ownership of the logged value. It might be referenced by multiple
       * sockets. */
      logged_values_.append(std::move(value_of_sockets.value));

      for (const DSocket &socket : value_of_sockets.sockets) {
        SocketLog &socket_log = this->lookup_or_add_socket_log(log_by_tree_context, socket);
        socket_log.value_ = value_log;
      }
    }

    for (NodeWithWarning &node_with_warning : local_logger.node_warnings_) {
      NodeLog &node_log = this->lookup_or_add_node_log(log_by_tree_context,
                                                       node_with_warning.node);
      node_log.warnings_.append(node_with_warning.warning);
    }
  }
}

TreeLog &ModifierLog::lookup_or_add_tree_log(LogByTreeContext &log_by_tree_context,
                                             const DTreeContext &tree_context)
{
  TreeLog *tree_log = log_by_tree_context.lookup_default(&tree_context, nullptr);
  if (tree_log != nullptr) {
    return *tree_log;
  }

  const DTreeContext *parent_context = tree_context.parent_context();
  if (parent_context == nullptr) {
    return *root_tree_logs_.get();
  }
  TreeLog &parent_log = this->lookup_or_add_tree_log(log_by_tree_context, *parent_context);
  destruct_ptr<TreeLog> owned_tree_log = allocator_.construct<TreeLog>();
  tree_log = owned_tree_log.get();
  log_by_tree_context.add_new(&tree_context, tree_log);
  parent_log.child_logs_.add_new(tree_context.parent_node()->name(), std::move(owned_tree_log));
  return *tree_log;
}

NodeLog &ModifierLog::lookup_or_add_node_log(LogByTreeContext &log_by_tree_context, DNode node)
{
  TreeLog &tree_log = this->lookup_or_add_tree_log(log_by_tree_context, *node.context());
  NodeLog &node_log = *tree_log.node_logs_.lookup_or_add_cb(node->name(), [&]() {
    destruct_ptr<NodeLog> node_log = allocator_.construct<NodeLog>();
    node_log->input_logs_.resize(node->inputs().size());
    node_log->output_logs_.resize(node->outputs().size());
    return node_log;
  });
  return node_log;
}

SocketLog &ModifierLog::lookup_or_add_socket_log(LogByTreeContext &log_by_tree_context,
                                                 DSocket socket)
{
  NodeLog &node_log = this->lookup_or_add_node_log(log_by_tree_context, socket.node());
  MutableSpan<SocketLog> socket_logs = socket->is_input() ? node_log.input_logs_ :
                                                            node_log.output_logs_;
  SocketLog &socket_log = socket_logs[socket->index()];
  return socket_log;
}

void ModifierLog::foreach_node_log(FunctionRef<void(const NodeLog &)> fn) const
{
  if (root_tree_logs_) {
    root_tree_logs_->foreach_node_log(fn);
  }
}

const GeometryValueLog *ModifierLog::input_geometry_log() const
{
  return input_geometry_log_.get();
}
const GeometryValueLog *ModifierLog::output_geometry_log() const
{
  return output_geometry_log_.get();
}

const NodeLog *TreeLog::lookup_node_log(StringRef node_name) const
{
  const destruct_ptr<NodeLog> *node_log = node_logs_.lookup_ptr_as(node_name);
  if (node_log == nullptr) {
    return nullptr;
  }
  return node_log->get();
}

const NodeLog *TreeLog::lookup_node_log(const bNode &node) const
{
  return this->lookup_node_log(node.name);
}

const TreeLog *TreeLog::lookup_child_log(StringRef node_name) const
{
  const destruct_ptr<TreeLog> *tree_log = child_logs_.lookup_ptr_as(node_name);
  if (tree_log == nullptr) {
    return nullptr;
  }
  return tree_log->get();
}

void TreeLog::foreach_node_log(FunctionRef<void(const NodeLog &)> fn) const
{
  for (auto node_log : node_logs_.items()) {
    fn(*node_log.value);
  }

  for (auto child : child_logs_.items()) {
    child.value->foreach_node_log(fn);
  }
}

const SocketLog *NodeLog::lookup_socket_log(eNodeSocketInOut in_out, int index) const
{
  BLI_assert(index >= 0);
  Span<SocketLog> socket_logs = (in_out == SOCK_IN) ? input_logs_ : output_logs_;
  if (index >= socket_logs.size()) {
    return nullptr;
  }
  return &socket_logs[index];
}

const SocketLog *NodeLog::lookup_socket_log(const bNode &node, const bNodeSocket &socket) const
{
  ListBase sockets = socket.in_out == SOCK_IN ? node.inputs : node.outputs;
  int index = BLI_findindex(&sockets, &socket);
  return this->lookup_socket_log((eNodeSocketInOut)socket.in_out, index);
}

GFieldValueLog::GFieldValueLog(fn::GField field, bool log_full_field) : type_(field.cpp_type())
{
  VectorSet<std::reference_wrapper<const FieldInput>> field_inputs;
  field.node().foreach_field_input(
      [&](const FieldInput &field_input) { field_inputs.add(field_input); });
  for (const FieldInput &field_input : field_inputs) {
    input_tooltips_.append(field_input.socket_inspection_name());
  }

  if (log_full_field) {
    field_ = std::move(field);
  }
}

GeometryValueLog::GeometryValueLog(const GeometrySet &geometry_set, bool log_full_geometry)
{
  static std::array all_component_types = {GEO_COMPONENT_TYPE_CURVE,
                                           GEO_COMPONENT_TYPE_INSTANCES,
                                           GEO_COMPONENT_TYPE_MESH,
                                           GEO_COMPONENT_TYPE_POINT_CLOUD,
                                           GEO_COMPONENT_TYPE_VOLUME};

  /* Keep track handled attribute names to make sure that we do not return the same name twice.
   * Currently #GeometrySet::attribute_foreach does not do that. Note that this will merge
   * attributes with the same name but different domains or data types on separate components. */
  Set<StringRef> names;

  geometry_set.attribute_foreach(
      all_component_types,
      true,
      [&](const bke::AttributeIDRef &attribute_id,
          const AttributeMetaData &meta_data,
          const GeometryComponent &UNUSED(component)) {
        if (attribute_id.is_named() && names.add(attribute_id.name())) {
          this->attributes_.append({attribute_id.name(), meta_data.domain, meta_data.data_type});
        }
      });

  for (const GeometryComponent *component : geometry_set.get_components_for_read()) {
    component_types_.append(component->type());
    switch (component->type()) {
      case GEO_COMPONENT_TYPE_MESH: {
        const MeshComponent &mesh_component = *(const MeshComponent *)component;
        MeshInfo &info = this->mesh_info.emplace();
        info.tot_verts = mesh_component.attribute_domain_size(ATTR_DOMAIN_POINT);
        info.tot_edges = mesh_component.attribute_domain_size(ATTR_DOMAIN_EDGE);
        info.tot_faces = mesh_component.attribute_domain_size(ATTR_DOMAIN_FACE);
        break;
      }
      case GEO_COMPONENT_TYPE_CURVE: {
        const CurveComponent &curve_component = *(const CurveComponent *)component;
        CurveInfo &info = this->curve_info.emplace();
        info.tot_splines = curve_component.attribute_domain_size(ATTR_DOMAIN_CURVE);
        break;
      }
      case GEO_COMPONENT_TYPE_POINT_CLOUD: {
        const PointCloudComponent &pointcloud_component = *(const PointCloudComponent *)component;
        PointCloudInfo &info = this->pointcloud_info.emplace();
        info.tot_points = pointcloud_component.attribute_domain_size(ATTR_DOMAIN_POINT);
        break;
      }
      case GEO_COMPONENT_TYPE_INSTANCES: {
        const InstancesComponent &instances_component = *(const InstancesComponent *)component;
        InstancesInfo &info = this->instances_info.emplace();
        info.tot_instances = instances_component.instances_amount();
        break;
      }
      case GEO_COMPONENT_TYPE_VOLUME: {
        break;
      }
    }
  }
  if (log_full_geometry) {
    full_geometry_ = std::make_unique<GeometrySet>(geometry_set);
    full_geometry_->ensure_owns_direct_data();
  }
}

Vector<const GeometryAttributeInfo *> NodeLog::lookup_available_attributes() const
{
  Vector<const GeometryAttributeInfo *> attributes;
  Set<StringRef> names;
  for (const SocketLog &socket_log : input_logs_) {
    const ValueLog *value_log = socket_log.value();
    if (const GeometryValueLog *geo_value_log = dynamic_cast<const GeometryValueLog *>(
            value_log)) {
      for (const GeometryAttributeInfo &attribute : geo_value_log->attributes()) {
        if (names.add(attribute.name)) {
          attributes.append(&attribute);
        }
      }
    }
  }
  return attributes;
}

const ModifierLog *ModifierLog::find_root_by_node_editor_context(const SpaceNode &snode)
{
  if (snode.id == nullptr) {
    return nullptr;
  }
  if (GS(snode.id->name) != ID_OB) {
    return nullptr;
  }
  Object *object = (Object *)snode.id;
  LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
    if (md->type == eModifierType_Nodes) {
      NodesModifierData *nmd = (NodesModifierData *)md;
      if (nmd->node_group == snode.nodetree) {
        return (ModifierLog *)nmd->runtime_eval_log;
      }
    }
  }
  return nullptr;
}

const TreeLog *ModifierLog::find_tree_by_node_editor_context(const SpaceNode &snode)
{
  const ModifierLog *eval_log = ModifierLog::find_root_by_node_editor_context(snode);
  if (eval_log == nullptr) {
    return nullptr;
  }
  Vector<bNodeTreePath *> tree_path_vec = snode.treepath;
  if (tree_path_vec.is_empty()) {
    return nullptr;
  }
  TreeLog *current = eval_log->root_tree_logs_.get();
  for (bNodeTreePath *path : tree_path_vec.as_span().drop_front(1)) {
    destruct_ptr<TreeLog> *tree_log = current->child_logs_.lookup_ptr_as(path->node_name);
    if (tree_log == nullptr) {
      return nullptr;
    }
    current = tree_log->get();
  }
  return current;
}

const NodeLog *ModifierLog::find_node_by_node_editor_context(const SpaceNode &snode,
                                                             const bNode &node)
{
  const TreeLog *tree_log = ModifierLog::find_tree_by_node_editor_context(snode);
  if (tree_log == nullptr) {
    return nullptr;
  }
  return tree_log->lookup_node_log(node);
}

const SocketLog *ModifierLog::find_socket_by_node_editor_context(const SpaceNode &snode,
                                                                 const bNode &node,
                                                                 const bNodeSocket &socket)
{
  const NodeLog *node_log = ModifierLog::find_node_by_node_editor_context(snode, node);
  if (node_log == nullptr) {
    return nullptr;
  }
  return node_log->lookup_socket_log(node, socket);
}

const NodeLog *ModifierLog::find_node_by_spreadsheet_editor_context(
    const SpaceSpreadsheet &sspreadsheet)
{
  Vector<SpreadsheetContext *> context_path = sspreadsheet.context_path;
  if (context_path.size() <= 2) {
    return nullptr;
  }
  if (context_path[0]->type != SPREADSHEET_CONTEXT_OBJECT) {
    return nullptr;
  }
  if (context_path[1]->type != SPREADSHEET_CONTEXT_MODIFIER) {
    return nullptr;
  }
  for (SpreadsheetContext *context : context_path.as_span().drop_front(2)) {
    if (context->type != SPREADSHEET_CONTEXT_NODE) {
      return nullptr;
    }
  }
  Span<SpreadsheetContextNode *> node_contexts =
      context_path.as_span().drop_front(2).cast<SpreadsheetContextNode *>();

  Object *object = ((SpreadsheetContextObject *)context_path[0])->object;
  StringRefNull modifier_name = ((SpreadsheetContextModifier *)context_path[1])->modifier_name;
  if (object == nullptr) {
    return nullptr;
  }

  const ModifierLog *eval_log = nullptr;
  LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
    if (md->type == eModifierType_Nodes) {
      if (md->name == modifier_name) {
        NodesModifierData *nmd = (NodesModifierData *)md;
        eval_log = (const ModifierLog *)nmd->runtime_eval_log;
        break;
      }
    }
  }
  if (eval_log == nullptr) {
    return nullptr;
  }

  const TreeLog *tree_log = &eval_log->root_tree();
  for (SpreadsheetContextNode *context : node_contexts.drop_back(1)) {
    tree_log = tree_log->lookup_child_log(context->node_name);
    if (tree_log == nullptr) {
      return nullptr;
    }
  }
  const NodeLog *node_log = tree_log->lookup_node_log(node_contexts.last()->node_name);
  return node_log;
}

void LocalGeoLogger::log_value_for_sockets(Span<DSocket> sockets, GPointer value)
{
  const CPPType &type = *value.type();
  Span<DSocket> copied_sockets = allocator_->construct_array_copy(sockets);
  if (type.is<GeometrySet>()) {
    bool log_full_geometry = false;
    for (const DSocket &socket : sockets) {
      if (main_logger_->log_full_sockets_.contains(socket)) {
        log_full_geometry = true;
        break;
      }
    }

    const GeometrySet &geometry_set = *value.get<GeometrySet>();
    destruct_ptr<GeometryValueLog> value_log = allocator_->construct<GeometryValueLog>(
        geometry_set, log_full_geometry);
    values_.append({copied_sockets, std::move(value_log)});
  }
  else if (const FieldCPPType *field_type = dynamic_cast<const FieldCPPType *>(&type)) {
    GField field = field_type->get_gfield(value.get());
    bool log_full_field = false;
    if (!field.node().depends_on_input()) {
      /* Always log constant fields so that their value can be shown in socket inspection.
       * In the future we can also evaluate the field here and only store the value. */
      log_full_field = true;
    }
    if (!log_full_field) {
      for (const DSocket &socket : sockets) {
        if (main_logger_->log_full_sockets_.contains(socket)) {
          log_full_field = true;
          break;
        }
      }
    }
    destruct_ptr<GFieldValueLog> value_log = allocator_->construct<GFieldValueLog>(
        std::move(field), log_full_field);
    values_.append({copied_sockets, std::move(value_log)});
  }
  else {
    void *buffer = allocator_->allocate(type.size(), type.alignment());
    type.copy_construct(value.get(), buffer);
    destruct_ptr<GenericValueLog> value_log = allocator_->construct<GenericValueLog>(
        GMutablePointer{type, buffer});
    values_.append({copied_sockets, std::move(value_log)});
  }
}

void LocalGeoLogger::log_multi_value_socket(DSocket socket, Span<GPointer> values)
{
  /* Doesn't have to be logged currently. */
  UNUSED_VARS(socket, values);
}

void LocalGeoLogger::log_node_warning(DNode node, NodeWarningType type, std::string message)
{
  node_warnings_.append({node, {type, std::move(message)}});
}

}  // namespace blender::nodes::geometry_nodes_eval_log
