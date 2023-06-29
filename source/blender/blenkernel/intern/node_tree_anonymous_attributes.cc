/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"

#include "BKE_node_runtime.hh"
#include "BKE_node_tree_anonymous_attributes.hh"
#include "BKE_node_tree_dot_export.hh"

#include "BLI_bit_group_vector.hh"
#include "BLI_bit_span_ops.hh"

#include "BLI_resource_scope.hh"

namespace blender::bke::anonymous_attribute_inferencing {
namespace aal = nodes::aal;
using nodes::NodeDeclaration;

static bool is_possible_field_socket(const bNodeSocket &socket)
{
  return ELEM(socket.type, SOCK_FLOAT, SOCK_VECTOR, SOCK_RGBA, SOCK_BOOLEAN, SOCK_INT);
}

static bool socket_is_field(const bNodeSocket &socket)
{
  return socket.display_shape == SOCK_DISPLAY_SHAPE_DIAMOND;
}

static const aal::RelationsInNode &get_relations_in_node(const bNode &node, ResourceScope &scope)
{
  if (node.is_group()) {
    if (const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node.id)) {
      /* Undefined tree types have no relations. */
      if (!ntreeIsRegistered(group)) {
        return scope.construct<aal::RelationsInNode>();
      }
      /* It's possible that the inferencing failed on the group. */
      if (!group->runtime->anonymous_attribute_inferencing) {
        return scope.construct<aal::RelationsInNode>();
      }
      return group->runtime->anonymous_attribute_inferencing->tree_relations;
    }
  }
  if (node.is_reroute()) {
    const bNodeSocket &socket = node.input_socket(0);
    if (socket_is_field(socket)) {
      static const aal::RelationsInNode field_relations = []() {
        aal::RelationsInNode relations;
        relations.reference_relations.append({0, 0});
        return relations;
      }();
      return field_relations;
    }
    if (socket.type == SOCK_GEOMETRY) {
      static const aal::RelationsInNode geometry_relations = []() {
        aal::RelationsInNode relations;
        relations.propagate_relations.append({0, 0});
        return relations;
      }();
      return geometry_relations;
    }
  }
  if (ELEM(node.type, GEO_NODE_SIMULATION_INPUT, GEO_NODE_SIMULATION_OUTPUT)) {
    aal::RelationsInNode &relations = scope.construct<aal::RelationsInNode>();
    {
      /* Add eval relations. */
      int last_geometry_index = -1;
      for (const int i : node.input_sockets().index_range()) {
        const bNodeSocket &socket = node.input_socket(i);
        if (socket.type == SOCK_GEOMETRY) {
          last_geometry_index = i;
        }
        else if (socket_is_field(socket)) {
          if (last_geometry_index != -1) {
            relations.eval_relations.append({i, last_geometry_index});
          }
        }
      }
    }

    {
      /* Add available relations. */
      int last_geometry_index = -1;
      for (const int i : node.output_sockets().index_range()) {
        const bNodeSocket &socket = node.output_socket(i);
        if (socket.type == SOCK_GEOMETRY) {
          last_geometry_index = i;
        }
        else if (socket_is_field(socket)) {
          if (last_geometry_index == -1) {
            relations.available_on_none.append(i);
          }
          else {
            relations.available_relations.append({i, last_geometry_index});
          }
        }
      }
    }
    return relations;
  }
  if (const NodeDeclaration *node_decl = node.declaration()) {
    if (const aal::RelationsInNode *relations = node_decl->anonymous_attribute_relations()) {
      return *relations;
    }
  }
  return scope.construct<aal::RelationsInNode>();
}

Array<const aal::RelationsInNode *> get_relations_by_node(const bNodeTree &tree,
                                                          ResourceScope &scope)
{
  const Span<const bNode *> nodes = tree.all_nodes();
  Array<const aal::RelationsInNode *> relations_by_node(nodes.size());
  for (const int i : nodes.index_range()) {
    relations_by_node[i] = &get_relations_in_node(*nodes[i], scope);
  }
  return relations_by_node;
}

class bNodeTreeToDotOptionsForAnonymousAttributeInferencing : public bNodeTreeToDotOptions {
 private:
  const AnonymousAttributeInferencingResult &result_;

 public:
  bNodeTreeToDotOptionsForAnonymousAttributeInferencing(
      const AnonymousAttributeInferencingResult &result)
      : result_(result)
  {
  }

  std::string socket_name(const bNodeSocket &socket) const
  {
    if (socket.type == SOCK_GEOMETRY) {
      std::stringstream ss;
      ss << socket.identifier << " [";
      bits::foreach_1_index(result_.required_fields_by_geometry_socket[socket.index_in_tree()],
                            [&](const int i) { ss << i << ","; });
      ss << "] [";
      bits::foreach_1_index(
          result_.propagate_to_output_by_geometry_socket[socket.index_in_tree()],
          [&](const int i) { ss << result_.propagated_output_geometry_indices[i] << ","; });
      ss << "]";
      return ss.str();
    }
    else if (is_possible_field_socket(socket)) {
      std::stringstream ss;
      ss << socket.identifier << " [";
      bits::foreach_1_index(result_.propagated_fields_by_socket[socket.index_in_tree()],
                            [&](const int i) { ss << i << ","; });
      ss << "]";
      return ss.str();
    }
    return socket.identifier;
  }
};

static AnonymousAttributeInferencingResult analyse_anonymous_attribute_usages(
    const bNodeTree &tree)
{
  BLI_assert(!tree.has_available_link_cycle());

  ResourceScope scope;
  const Array<const aal::RelationsInNode *> relations_by_node = get_relations_by_node(tree, scope);

  Vector<FieldSource> all_field_sources;
  Vector<GeometrySource> all_geometry_sources;

  /* Find input field and geometry sources. */
  for (const int i : tree.interface_inputs().index_range()) {
    const bNodeSocket &interface_socket = *tree.interface_inputs()[i];
    if (interface_socket.type == SOCK_GEOMETRY) {
      all_geometry_sources.append_and_get_index({InputGeometrySource{i}});
    }
    else if (is_possible_field_socket(interface_socket)) {
      all_field_sources.append_and_get_index({InputFieldSource{i}});
    }
  }
  for (const int geometry_source_index : all_geometry_sources.index_range()) {
    for (const int field_source_index : all_field_sources.index_range()) {
      all_geometry_sources[geometry_source_index].field_sources.append(field_source_index);
      all_field_sources[field_source_index].geometry_sources.append(geometry_source_index);
    }
  }

  /* Find socket field and geometry sources. */
  Map<const bNodeSocket *, int> field_source_by_socket;
  Map<const bNodeSocket *, int> geometry_source_by_socket;
  for (const bNode *node : tree.all_nodes()) {
    const aal::RelationsInNode &relations = *relations_by_node[node->index()];
    for (const aal::AvailableRelation &relation : relations.available_relations) {
      const bNodeSocket &geometry_socket = node->output_socket(relation.geometry_output);
      const bNodeSocket &field_socket = node->output_socket(relation.field_output);
      if (!field_socket.is_available()) {
        continue;
      }
      if (!field_socket.is_directly_linked()) {
        continue;
      }

      const int field_source_index = field_source_by_socket.lookup_or_add_cb(&field_socket, [&]() {
        return all_field_sources.append_and_get_index({SocketFieldSource{&field_socket}});
      });
      const int geometry_source_index = geometry_source_by_socket.lookup_or_add_cb(
          &geometry_socket, [&]() {
            return all_geometry_sources.append_and_get_index(
                {SocketGeometrySource{&geometry_socket}});
          });

      all_field_sources[field_source_index].geometry_sources.append(geometry_source_index);
      all_geometry_sources[geometry_source_index].field_sources.append(field_source_index);
    }
  }

  const int sockets_num = tree.all_sockets().size();
  BitGroupVector<> propagated_fields_by_socket(sockets_num, all_field_sources.size(), false);
  BitGroupVector<> propagated_geometries_by_socket(
      sockets_num, all_geometry_sources.size(), false);
  BitGroupVector<> available_fields_by_geometry_socket(
      sockets_num, all_field_sources.size(), false);

  /* Insert field and geometry sources into the maps for the first inferencing pass. */
  for (const int field_source_index : all_field_sources.index_range()) {
    const FieldSource &field_source = all_field_sources[field_source_index];
    if (const auto *input_field = std::get_if<InputFieldSource>(&field_source.data)) {
      for (const bNode *node : tree.group_input_nodes()) {
        const bNodeSocket &socket = node->output_socket(input_field->input_index);
        propagated_fields_by_socket[socket.index_in_tree()][field_source_index].set();
      }
    }
    else {
      const auto &socket_field = std::get<SocketFieldSource>(field_source.data);
      propagated_fields_by_socket[socket_field.socket->index_in_tree()][field_source_index].set();
    }
  }
  for (const int geometry_source_index : all_geometry_sources.index_range()) {
    const GeometrySource &geometry_source = all_geometry_sources[geometry_source_index];
    if (const auto *input_geometry = std::get_if<InputGeometrySource>(&geometry_source.data)) {
      for (const bNode *node : tree.group_input_nodes()) {
        const bNodeSocket &socket = node->output_socket(input_geometry->input_index);
        const int socket_i = socket.index_in_tree();
        propagated_geometries_by_socket[socket_i][geometry_source_index].set();
        for (const int field_source_index : geometry_source.field_sources) {
          available_fields_by_geometry_socket[socket_i][field_source_index].set();
        }
      }
    }
    else {
      const auto &socket_geometry = std::get<SocketGeometrySource>(geometry_source.data);
      const int socket_i = socket_geometry.socket->index_in_tree();
      propagated_geometries_by_socket[socket_i][geometry_source_index].set();
      for (const int field_source_index : geometry_source.field_sources) {
        available_fields_by_geometry_socket[socket_i][field_source_index].set();
      }
    }
  }

  /* Inferencing pass from left to right to figure out where fields and geometries may be
   * propagated to. */
  for (const bNode *node : tree.toposort_left_to_right()) {
    for (const bNodeSocket *socket : node->input_sockets()) {
      if (!socket->is_available()) {
        continue;
      }
      const int dst_index = socket->index_in_tree();
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (link->is_used()) {
          const int src_index = link->fromsock->index_in_tree();
          propagated_fields_by_socket[dst_index] |= propagated_fields_by_socket[src_index];
          propagated_geometries_by_socket[dst_index] |= propagated_geometries_by_socket[src_index];
          available_fields_by_geometry_socket[dst_index] |=
              available_fields_by_geometry_socket[src_index];
        }
      }
    }
    const aal::RelationsInNode &relations = *relations_by_node[node->index()];
    for (const aal::ReferenceRelation &relation : relations.reference_relations) {
      const bNodeSocket &from_socket = node->input_socket(relation.from_field_input);
      const bNodeSocket &to_socket = node->output_socket(relation.to_field_output);
      if (!from_socket.is_available() || !to_socket.is_available()) {
        continue;
      }
      const int src_index = from_socket.index_in_tree();
      const int dst_index = to_socket.index_in_tree();
      propagated_fields_by_socket[dst_index] |= propagated_fields_by_socket[src_index];
    }
    for (const aal::PropagateRelation &relation : relations.propagate_relations) {
      const bNodeSocket &from_socket = node->input_socket(relation.from_geometry_input);
      const bNodeSocket &to_socket = node->output_socket(relation.to_geometry_output);
      if (!from_socket.is_available() || !to_socket.is_available()) {
        continue;
      }
      const int src_index = from_socket.index_in_tree();
      const int dst_index = to_socket.index_in_tree();
      propagated_geometries_by_socket[dst_index] |= propagated_geometries_by_socket[src_index];
      available_fields_by_geometry_socket[dst_index] |=
          available_fields_by_geometry_socket[src_index];
    }
  }

  BitGroupVector<> required_fields_by_geometry_socket(
      sockets_num, all_field_sources.size(), false);
  VectorSet<int> propagated_output_geometry_indices;
  aal::RelationsInNode tree_relations;

  /* Create #PropagateRelation, #AvailableRelation and #ReferenceRelation for the tree based on the
   * propagated data from above. */
  if (const bNode *group_output_node = tree.group_output_node()) {
    for (const bNodeSocket *socket : group_output_node->input_sockets().drop_back(1)) {
      if (socket->type == SOCK_GEOMETRY) {
        const BoundedBitSpan propagated_geometries =
            propagated_geometries_by_socket[socket->index_in_tree()];
        bits::foreach_1_index(propagated_geometries, [&](const int geometry_source_index) {
          const GeometrySource &geometry_source = all_geometry_sources[geometry_source_index];
          if (const auto *input_geometry = std::get_if<InputGeometrySource>(&geometry_source.data))
          {
            tree_relations.propagate_relations.append(
                aal::PropagateRelation{input_geometry->input_index, socket->index()});
            propagated_output_geometry_indices.add(socket->index());
          }
          else {
            [[maybe_unused]] const auto &socket_geometry = std::get<SocketGeometrySource>(
                geometry_source.data);
            for (const int field_source_index : geometry_source.field_sources) {
              for (const bNodeSocket *other_socket :
                   group_output_node->input_sockets().drop_back(1)) {
                if (!is_possible_field_socket(*other_socket)) {
                  continue;
                }
                if (propagated_fields_by_socket[other_socket->index_in_tree()][field_source_index]
                        .test()) {
                  tree_relations.available_relations.append(
                      aal::AvailableRelation{other_socket->index(), socket->index()});
                  required_fields_by_geometry_socket[socket->index_in_tree()][field_source_index]
                      .set();
                }
              }
            }
          }
        });
      }
      else if (is_possible_field_socket(*socket)) {
        const BoundedBitSpan propagated_fields =
            propagated_fields_by_socket[socket->index_in_tree()];
        bits::foreach_1_index(propagated_fields, [&](const int field_source_index) {
          const FieldSource &field_source = all_field_sources[field_source_index];
          if (const auto *input_field = std::get_if<InputFieldSource>(&field_source.data)) {
            tree_relations.reference_relations.append(
                aal::ReferenceRelation{input_field->input_index, socket->index()});
          }
        });
      }
    }
  }

  /* Initialize map for second inferencing pass. */
  BitGroupVector<> propagate_to_output_by_geometry_socket(
      sockets_num, propagated_output_geometry_indices.size(), false);
  for (const aal::PropagateRelation &relation : tree_relations.propagate_relations) {
    const bNodeSocket &socket = tree.group_output_node()->input_socket(
        relation.to_geometry_output);
    propagate_to_output_by_geometry_socket[socket.index_in_tree()]
                                          [propagated_output_geometry_indices.index_of(
                                               relation.to_geometry_output)]
                                              .set();
  }

  /* Inferencing pass from right to left to determine which anonymous attributes have to be
   * propagated to which geometry sockets. */
  for (const bNode *node : tree.toposort_right_to_left()) {
    for (const bNodeSocket *socket : node->output_sockets()) {
      if (!socket->is_available()) {
        continue;
      }
      const int dst_index = socket->index_in_tree();
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (link->is_used()) {
          const int src_index = link->tosock->index_in_tree();
          required_fields_by_geometry_socket[dst_index] |=
              required_fields_by_geometry_socket[src_index];
          propagate_to_output_by_geometry_socket[dst_index] |=
              propagate_to_output_by_geometry_socket[src_index];
        }
      }
    }
    const aal::RelationsInNode &relations = *relations_by_node[node->index()];
    for (const aal::PropagateRelation &relation : relations.propagate_relations) {
      const bNodeSocket &output_socket = node->output_socket(relation.to_geometry_output);
      const bNodeSocket &input_socket = node->input_socket(relation.from_geometry_input);
      const int src_index = output_socket.index_in_tree();
      const int dst_index = input_socket.index_in_tree();
      required_fields_by_geometry_socket[dst_index] |=
          required_fields_by_geometry_socket[src_index];
      propagate_to_output_by_geometry_socket[dst_index] |=
          propagate_to_output_by_geometry_socket[src_index];
    }
    for (const aal::EvalRelation &relation : relations.eval_relations) {
      const bNodeSocket &geometry_socket = node->input_socket(relation.geometry_input);
      const bNodeSocket &field_socket = node->input_socket(relation.field_input);
      required_fields_by_geometry_socket[geometry_socket.index_in_tree()] |=
          propagated_fields_by_socket[field_socket.index_in_tree()];
    }
  }

  /* Make sure that only available fields are also required. */
  required_fields_by_geometry_socket.all_bits() &= available_fields_by_geometry_socket.all_bits();

  /* Create #EvalRelation for the tree. */
  for (const int interface_i : tree.interface_inputs().index_range()) {
    const bNodeSocket &interface_socket = *tree.interface_inputs()[interface_i];
    if (interface_socket.type != SOCK_GEOMETRY) {
      continue;
    }
    BitVector<> required_fields(all_field_sources.size(), false);
    for (const bNode *node : tree.group_input_nodes()) {
      const bNodeSocket &geometry_socket = node->output_socket(interface_i);
      required_fields |= required_fields_by_geometry_socket[geometry_socket.index_in_tree()];
    }
    bits::foreach_1_index(required_fields, [&](const int field_source_index) {
      const FieldSource &field_source = all_field_sources[field_source_index];
      if (const auto *input_field = std::get_if<InputFieldSource>(&field_source.data)) {
        tree_relations.eval_relations.append(
            aal::EvalRelation{input_field->input_index, interface_i});
      }
    });
  }

  AnonymousAttributeInferencingResult result{std::move(all_field_sources),
                                             std::move(all_geometry_sources),
                                             std::move(propagated_fields_by_socket),
                                             std::move(propagated_geometries_by_socket),
                                             std::move(available_fields_by_geometry_socket),
                                             std::move(required_fields_by_geometry_socket),
                                             std::move(propagated_output_geometry_indices),
                                             std::move(propagate_to_output_by_geometry_socket),
                                             std::move(tree_relations)};

/* Print analysis result for debugging purposes. */
#if 0
  bNodeTreeToDotOptionsForAnonymousAttributeInferencing options{result};
  std::cout << "\n\n" << node_tree_to_dot(tree, options) << "\n\n";
#endif
  return result;
}

bool update_anonymous_attribute_relations(bNodeTree &tree)
{
  tree.ensure_topology_cache();

  if (tree.has_available_link_cycle()) {
    const bool changed = tree.runtime->anonymous_attribute_inferencing.get() != nullptr;
    tree.runtime->anonymous_attribute_inferencing.reset();
    return changed;
  }

  AnonymousAttributeInferencingResult result = analyse_anonymous_attribute_usages(tree);

  const bool group_interface_changed =
      !tree.runtime->anonymous_attribute_inferencing ||
      tree.runtime->anonymous_attribute_inferencing->tree_relations != result.tree_relations;

  tree.runtime->anonymous_attribute_inferencing =
      std::make_unique<AnonymousAttributeInferencingResult>(std::move(result));

  return group_interface_changed;
}

}  // namespace blender::bke::anonymous_attribute_inferencing
