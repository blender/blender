/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"

#include "BKE_node_runtime.hh"
#include "BKE_node_tree_anonymous_attributes.hh"
#include "BKE_node_tree_dot_export.hh"
#include "BKE_node_tree_zones.hh"

#include "BLI_bit_group_vector.hh"
#include "BLI_bit_span_ops.hh"

#include "BLI_resource_scope.hh"

#include <iostream>
#include <sstream>

namespace blender::bke::anonymous_attribute_inferencing {
namespace aal = nodes::aal;
using nodes::NodeDeclaration;

static bool socket_is_field(const bNodeSocket &socket)
{
  return socket.display_shape == SOCK_DISPLAY_SHAPE_DIAMOND;
}

static const aal::RelationsInNode &get_relations_in_node(const bNode &node, ResourceScope &scope)
{
  if (node.is_group()) {
    if (const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node.id)) {
      /* Undefined tree types have no relations. */
      if (!bke::ntreeIsRegistered(group)) {
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
  if (node.is_muted()) {
    aal::RelationsInNode &relations = scope.construct<aal::RelationsInNode>();
    for (const bNodeLink &link : node.internal_links()) {
      const bNodeSocket &input = *link.fromsock;
      const bNodeSocket &output = *link.tosock;
      if (socket_is_field(input) || socket_is_field(output)) {
        relations.reference_relations.append({input.index(), output.index()});
      }
      else if (input.type == SOCK_GEOMETRY) {
        BLI_assert(input.type == output.type);
        relations.propagate_relations.append({input.index(), output.index()});
      }
    }
    return relations;
  }
  if (ELEM(node.type, GEO_NODE_SIMULATION_INPUT, GEO_NODE_SIMULATION_OUTPUT, GEO_NODE_BAKE)) {
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
  if (ELEM(node.type, GEO_NODE_REPEAT_INPUT, GEO_NODE_REPEAT_OUTPUT)) {
    aal::RelationsInNode &relations = scope.construct<aal::RelationsInNode>();
    /* TODO: Use smaller set of eval and available relations. For now this makes the pessimistic
     * assumption that every field may belong to any geometry. In many cases it should be possible
     * to reduce this set a bit with static analysis. */
    for (const bNodeSocket *socket : node.output_sockets()) {
      if (socket->type == SOCK_GEOMETRY) {
        for (const bNodeSocket *other_output : node.output_sockets()) {
          if (socket_is_field(*other_output)) {
            relations.available_relations.append({other_output->index(), socket->index()});
          }
        }
      }
    }
    for (const bNodeSocket *socket : node.input_sockets()) {
      if (socket->type == SOCK_GEOMETRY) {
        for (const bNodeSocket *other_input : node.input_sockets()) {
          if (socket_is_field(*other_input)) {
            relations.eval_relations.append({other_input->index(), socket->index()});
          }
        }
      }
    }
    const int items_num = node.output_sockets().size() - 1;
    for (const int i : IndexRange(items_num)) {
      const int input_index = (node.type == GEO_NODE_REPEAT_INPUT) ? i + 1 : i;
      const int output_index = i;
      const bNodeSocket &input_socket = node.input_socket(input_index);
      if (input_socket.type == SOCK_GEOMETRY) {
        relations.propagate_relations.append({input_index, output_index});
      }
      else if (socket_is_field(input_socket)) {
        relations.reference_relations.append({input_index, output_index});
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

  std::string socket_name(const bNodeSocket &socket) const override
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
    if (nodes::socket_type_supports_fields(eNodeSocketDatatype(socket.type))) {
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

static bool or_into_each_other_masked(MutableBoundedBitSpan a,
                                      MutableBoundedBitSpan b,
                                      const BoundedBitSpan mask)
{
  if (bits::spans_equal_masked(a, b, mask)) {
    return false;
  }
  bits::inplace_or_masked(a, mask, b);
  bits::inplace_or_masked(b, mask, a);
  return true;
}

static bool or_into_each_other(MutableBoundedBitSpan a, MutableBoundedBitSpan b)
{
  if (bits::spans_equal(a, b)) {
    return false;
  }
  a |= b;
  b |= a;
  return true;
}

static bool or_into_each_other_masked(BitGroupVector<> &vec,
                                      const int64_t a,
                                      const int64_t b,
                                      const BoundedBitSpan mask)
{
  return or_into_each_other_masked(vec[a], vec[b], mask);
}

static bool or_into_each_other(BitGroupVector<> &vec, const int64_t a, const int64_t b)
{
  return or_into_each_other(vec[a], vec[b]);
}

static AnonymousAttributeInferencingResult analyze_anonymous_attribute_usages(
    const bNodeTree &tree)
{
  BLI_assert(!tree.has_available_link_cycle());
  tree.ensure_interface_cache();

  ResourceScope scope;
  const Array<const aal::RelationsInNode *> relations_by_node = get_relations_by_node(tree, scope);

  /* Repeat zones need some special behavior because they can propagate anonymous attributes from
   * right to left (from the repeat output to the repeat input node). */
  const bNodeTreeZones *zones = tree.zones();
  Vector<const bNodeTreeZone *> repeat_zones_to_consider;
  if (zones) {
    for (const std::unique_ptr<bNodeTreeZone> &zone : zones->zones) {
      if (ELEM(nullptr, zone->input_node, zone->output_node)) {
        continue;
      }
      if (zone->output_node->type != GEO_NODE_REPEAT_OUTPUT) {
        continue;
      }
      repeat_zones_to_consider.append(zone.get());
    }
  }

  Vector<FieldSource> all_field_sources;
  Vector<GeometrySource> all_geometry_sources;

  /* Find input field and geometry sources. */
  for (const int i : tree.interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket &interface_socket = *tree.interface_inputs()[i];
    const bNodeSocketType *typeinfo = bke::nodeSocketTypeFind(interface_socket.socket_type);
    const eNodeSocketDatatype type = typeinfo ? eNodeSocketDatatype(typeinfo->type) : SOCK_CUSTOM;
    if (type == SOCK_GEOMETRY) {
      all_geometry_sources.append_and_get_index({InputGeometrySource{i}});
    }
    else if (nodes::socket_type_supports_fields(type)) {
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
  auto pass_left_to_right = [&]() {
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
            propagated_geometries_by_socket[dst_index] |=
                propagated_geometries_by_socket[src_index];
            available_fields_by_geometry_socket[dst_index] |=
                available_fields_by_geometry_socket[src_index];
          }
        }
      }
      switch (node->type) {
        default: {
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
            propagated_geometries_by_socket[dst_index] |=
                propagated_geometries_by_socket[src_index];
            available_fields_by_geometry_socket[dst_index] |=
                available_fields_by_geometry_socket[src_index];
          }
          break;
        }
        /* The repeat output node needs special handling for two reasons:
         * - It propagates data directly from the zone input in case the iteration count is zero.
         * - Fields coming out of the repeat zone are wrapped by a new #FieldSource, because the
         *   intermediate fields from within the zone are not available afterwards. */
        case GEO_NODE_REPEAT_OUTPUT: {
          if (zones == nullptr) {
            break;
          }
          /* If the amount of iterations is zero, the data is directly forwarded from the Repeat
           * Input to the Repeat Output node. Therefor, all anonymous attributes may be propagated
           * as well. */
          const bNodeTreeZone *zone = zones->get_zone_by_node(node->identifier);
          const int items_num = node->output_sockets().size() - 1;
          if (const bNode *input_node = zone->input_node) {
            for (const int i : IndexRange(items_num)) {
              const int src_index = input_node->input_socket(i + 1).index_in_tree();
              const int dst_index = node->output_socket(i).index_in_tree();
              propagated_fields_by_socket[dst_index] |= propagated_fields_by_socket[src_index];
              propagated_geometries_by_socket[dst_index] |=
                  propagated_geometries_by_socket[src_index];
              available_fields_by_geometry_socket[dst_index] |=
                  available_fields_by_geometry_socket[src_index];
            }
          }

          auto can_propagate_field_source_out_of_zone = [&](const int field_source_index) {
            const FieldSource &field_source = all_field_sources[field_source_index];
            if (const auto *socket_field_source = std::get_if<SocketFieldSource>(
                    &field_source.data))
            {
              const bNode &field_source_node = socket_field_source->socket->owner_node();
              if (zone->contains_node_recursively(field_source_node)) {
                return false;
              }
            }
            return true;
          };
          auto can_propagated_geometry_source_out_of_zone = [&](const int geometry_source_index) {
            const GeometrySource &geometry_source = all_geometry_sources[geometry_source_index];
            if (const auto *socket_geometry_source = std::get_if<SocketGeometrySource>(
                    &geometry_source.data))
            {
              const bNode &geometry_source_node = socket_geometry_source->socket->owner_node();
              if (zone->contains_node_recursively(geometry_source_node)) {
                return false;
              }
            }
            return true;
          };

          /* Propagate fields that have not been created inside of the repeat zones. Field sources
           * from inside the repeat zone become new field sources on the outside. */
          for (const int i : IndexRange(items_num)) {
            const int src_index = node->input_socket(i).index_in_tree();
            const int dst_index = node->output_socket(i).index_in_tree();
            bits::foreach_1_index(
                propagated_fields_by_socket[src_index], [&](const int field_source_index) {
                  if (can_propagate_field_source_out_of_zone(field_source_index)) {
                    propagated_fields_by_socket[dst_index][field_source_index].set();
                  }
                });
            bits::foreach_1_index(
                available_fields_by_geometry_socket[src_index], [&](const int field_source_index) {
                  if (can_propagate_field_source_out_of_zone(field_source_index)) {
                    available_fields_by_geometry_socket[dst_index][field_source_index].set();
                  }
                });
            bits::foreach_1_index(
                propagated_geometries_by_socket[src_index], [&](const int geometry_source_index) {
                  if (can_propagated_geometry_source_out_of_zone(geometry_source_index)) {
                    propagated_geometries_by_socket[dst_index][geometry_source_index].set();
                  }
                });
          }
          break;
        }
      }
    }
  };

  while (true) {
    pass_left_to_right();

    /* Repeat zones may need multiple inference passes. That's because anonymous attributes
     * propagated to a repeat output node also come out of the corresponding repeat input node. */
    bool changed = false;
    for (const bNodeTreeZone *zone : repeat_zones_to_consider) {
      const auto &storage = *static_cast<const NodeGeometryRepeatOutput *>(
          zone->output_node->storage);
      /* Only field and geometry sources that come before the repeat zone, can be propagated from
       * the repeat output to the repeat input node. Otherwise, a socket can depend on the field
       * source that only comes later in the tree, which leads to a cyclic dependency. */
      BitVector<> input_propagated_fields(all_field_sources.size(), false);
      BitVector<> input_propagated_geometries(all_geometry_sources.size(), false);
      for (const bNodeSocket *socket : zone->input_node->input_sockets()) {
        const int src = socket->index_in_tree();
        input_propagated_fields |= propagated_fields_by_socket[src];
        input_propagated_geometries |= propagated_geometries_by_socket[src];
      }
      for (const bNodeLink *link : zone->border_links) {
        const int src = link->fromsock->index_in_tree();
        input_propagated_fields |= propagated_fields_by_socket[src];
        input_propagated_geometries |= propagated_geometries_by_socket[src];
      }
      for (const int i : IndexRange(storage.items_num)) {
        const bNodeSocket &body_input_socket = zone->input_node->output_socket(i);
        const bNodeSocket &body_output_socket = zone->output_node->input_socket(i);
        const int in_index = body_input_socket.index_in_tree();
        const int out_index = body_output_socket.index_in_tree();

        changed |= or_into_each_other_masked(
            propagated_fields_by_socket, in_index, out_index, input_propagated_fields);
        changed |= or_into_each_other_masked(
            propagated_geometries_by_socket, in_index, out_index, input_propagated_geometries);
        changed |= or_into_each_other_masked(
            available_fields_by_geometry_socket, in_index, out_index, input_propagated_fields);
      }
    }
    if (!changed) {
      break;
    }
  }

  BitGroupVector<> required_fields_by_geometry_socket(
      sockets_num, all_field_sources.size(), false);
  VectorSet<int> propagated_output_geometry_indices;
  aal::RelationsInNode tree_relations;

  /* Create #PropagateRelation, #AvailableRelation and #ReferenceRelation for the tree based on
   * the propagated data from above. */
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
                   group_output_node->input_sockets().drop_back(1))
              {
                if (!nodes::socket_type_supports_fields(eNodeSocketDatatype(other_socket->type))) {
                  continue;
                }
                if (propagated_fields_by_socket[other_socket->index_in_tree()][field_source_index]
                        .test())
                {
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
      else if (nodes::socket_type_supports_fields(eNodeSocketDatatype(socket->type))) {
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
  auto pass_right_to_left = [&]() {
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
  };

  while (true) {
    pass_right_to_left();

    /* Data required by a repeat input node will also be required by the repeat output node,
     * because that's where the data comes from after the first iteration. */
    bool changed = false;
    for (const bNodeTreeZone *zone : repeat_zones_to_consider) {
      const auto &storage = *static_cast<const NodeGeometryRepeatOutput *>(
          zone->output_node->storage);
      for (const int i : IndexRange(storage.items_num)) {
        const bNodeSocket &body_input_socket = zone->input_node->output_socket(i);
        const bNodeSocket &body_output_socket = zone->output_node->input_socket(i);
        const int in_index = body_input_socket.index_in_tree();
        const int out_index = body_output_socket.index_in_tree();

        changed |= or_into_each_other(required_fields_by_geometry_socket, in_index, out_index);
        changed |= or_into_each_other(propagate_to_output_by_geometry_socket, in_index, out_index);
      }
    }
    if (!changed) {
      break;
    }
  }

  /* Make sure that only available fields are also required. */
  required_fields_by_geometry_socket.all_bits() &= available_fields_by_geometry_socket.all_bits();

  /* Create #EvalRelation for the tree. */
  tree.ensure_topology_cache();

  for (const int interface_i : tree.interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket &interface_socket = *tree.interface_inputs()[interface_i];
    const bNodeSocketType *typeinfo = interface_socket.socket_typeinfo();
    eNodeSocketDatatype socket_type = typeinfo ? eNodeSocketDatatype(typeinfo->type) : SOCK_CUSTOM;
    if (socket_type != SOCK_GEOMETRY) {
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
    const bool changed = bool(tree.runtime->anonymous_attribute_inferencing);
    tree.runtime->anonymous_attribute_inferencing.reset();
    return changed;
  }

  AnonymousAttributeInferencingResult result = analyze_anonymous_attribute_usages(tree);

  const bool group_interface_changed =
      !tree.runtime->anonymous_attribute_inferencing ||
      tree.runtime->anonymous_attribute_inferencing->tree_relations != result.tree_relations;

  tree.runtime->anonymous_attribute_inferencing =
      std::make_unique<AnonymousAttributeInferencingResult>(std::move(result));

  return group_interface_changed;
}

}  // namespace blender::bke::anonymous_attribute_inferencing
