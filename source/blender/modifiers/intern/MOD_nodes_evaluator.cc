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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "MOD_nodes_evaluator.hh"

#include "NOD_geometry_exec.hh"
#include "NOD_type_conversions.hh"

#include "DEG_depsgraph_query.h"

#include "FN_generic_value_map.hh"
#include "FN_multi_function.hh"

namespace blender::modifiers::geometry_nodes {

using fn::CPPType;
using fn::GValueMap;
using nodes::GeoNodeExecParams;
using namespace fn::multi_function_types;

class NodeParamsProvider : public nodes::GeoNodeExecParamsProvider {
 public:
  LinearAllocator<> *allocator;
  GValueMap<StringRef> *input_values;
  GValueMap<StringRef> *output_values;

  bool can_get_input(StringRef identifier) const override
  {
    return input_values->contains(identifier);
  }

  bool can_set_output(StringRef identifier) const override
  {
    return !output_values->contains(identifier);
  }

  GMutablePointer extract_input(StringRef identifier) override
  {
    return this->input_values->extract(identifier);
  }

  Vector<GMutablePointer> extract_multi_input(StringRef identifier) override
  {
    Vector<GMutablePointer> values;
    int index = 0;
    while (true) {
      std::string sub_identifier = identifier;
      if (index > 0) {
        sub_identifier += "[" + std::to_string(index) + "]";
      }
      if (!this->input_values->contains(sub_identifier)) {
        break;
      }
      values.append(input_values->extract(sub_identifier));
      index++;
    }
    return values;
  }

  GPointer get_input(StringRef identifier) const override
  {
    return this->input_values->lookup(identifier);
  }

  GMutablePointer alloc_output_value(StringRef identifier, const CPPType &type) override
  {
    void *buffer = this->allocator->allocate(type.size(), type.alignment());
    GMutablePointer ptr{&type, buffer};
    this->output_values->add_new_direct(identifier, ptr);
    return ptr;
  }
};

class GeometryNodesEvaluator {
 public:
  using LogSocketValueFn = std::function<void(DSocket, Span<GPointer>)>;

 private:
  blender::LinearAllocator<> &allocator_;
  Map<std::pair<DInputSocket, DOutputSocket>, GMutablePointer> value_by_input_;
  Vector<DInputSocket> group_outputs_;
  blender::nodes::MultiFunctionByNode &mf_by_node_;
  const blender::nodes::DataTypeConversions &conversions_;
  const Object *self_object_;
  const ModifierData *modifier_;
  Depsgraph *depsgraph_;
  LogSocketValueFn log_socket_value_fn_;

 public:
  GeometryNodesEvaluator(GeometryNodesEvaluationParams &params)
      : allocator_(params.allocator),
        group_outputs_(std::move(params.output_sockets)),
        mf_by_node_(*params.mf_by_node),
        conversions_(blender::nodes::get_implicit_type_conversions()),
        self_object_(params.self_object),
        modifier_(&params.modifier_->modifier),
        depsgraph_(params.depsgraph),
        log_socket_value_fn_(std::move(params.log_socket_value_fn))
  {
    for (auto item : params.input_values.items()) {
      this->log_socket_value(item.key, item.value);
      this->forward_to_inputs(item.key, item.value);
    }
  }

  Vector<GMutablePointer> execute()
  {
    Vector<GMutablePointer> results;
    for (const DInputSocket &group_output : group_outputs_) {
      Vector<GMutablePointer> result = this->get_input_values(group_output);
      this->log_socket_value(group_output, result);
      results.append(result[0]);
    }
    for (GMutablePointer value : value_by_input_.values()) {
      value.destruct();
    }
    return results;
  }

 private:
  Vector<GMutablePointer> get_input_values(const DInputSocket socket_to_compute)
  {
    Vector<DSocket> from_sockets;
    socket_to_compute.foreach_origin_socket([&](DSocket socket) { from_sockets.append(socket); });

    if (from_sockets.is_empty()) {
      /* The input is not connected, use the value from the socket itself. */
      const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket_to_compute->typeinfo());
      return {get_unlinked_input_value(socket_to_compute, type)};
    }

    /* Multi-input sockets contain a vector of inputs. */
    if (socket_to_compute->is_multi_input_socket()) {
      return this->get_inputs_from_incoming_links(socket_to_compute, from_sockets);
    }

    const DSocket from_socket = from_sockets[0];
    GMutablePointer value = this->get_input_from_incoming_link(socket_to_compute, from_socket);
    return {value};
  }

  Vector<GMutablePointer> get_inputs_from_incoming_links(const DInputSocket socket_to_compute,
                                                         const Span<DSocket> from_sockets)
  {
    Vector<GMutablePointer> values;
    for (const int i : from_sockets.index_range()) {
      const DSocket from_socket = from_sockets[i];
      const int first_occurence = from_sockets.take_front(i).first_index_try(from_socket);
      if (first_occurence == -1) {
        values.append(this->get_input_from_incoming_link(socket_to_compute, from_socket));
      }
      else {
        /* If the same from-socket occurs more than once, we make a copy of the first value. This
         * can happen when a node linked to a multi-input-socket is muted. */
        GMutablePointer value = values[first_occurence];
        const CPPType *type = value.type();
        void *copy_buffer = allocator_.allocate(type->size(), type->alignment());
        type->copy_to_uninitialized(value.get(), copy_buffer);
        values.append({type, copy_buffer});
      }
    }
    return values;
  }

  GMutablePointer get_input_from_incoming_link(const DInputSocket socket_to_compute,
                                               const DSocket from_socket)
  {
    if (from_socket->is_output()) {
      const DOutputSocket from_output_socket{from_socket};
      const std::pair<DInputSocket, DOutputSocket> key = std::make_pair(socket_to_compute,
                                                                        from_output_socket);
      std::optional<GMutablePointer> value = value_by_input_.pop_try(key);
      if (value.has_value()) {
        /* This input has been computed before, return it directly. */
        return {*value};
      }

      /* Compute the socket now. */
      this->compute_output_and_forward(from_output_socket);
      return {value_by_input_.pop(key)};
    }

    /* Get value from an unlinked input socket. */
    const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket_to_compute->typeinfo());
    const DInputSocket from_input_socket{from_socket};
    return {get_unlinked_input_value(from_input_socket, type)};
  }

  void compute_output_and_forward(const DOutputSocket socket_to_compute)
  {
    const DNode node{socket_to_compute.context(), &socket_to_compute->node()};

    if (!socket_to_compute->is_available()) {
      /* If the output is not available, use a default value. */
      const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket_to_compute->typeinfo());
      void *buffer = allocator_.allocate(type.size(), type.alignment());
      type.copy_to_uninitialized(type.default_value(), buffer);
      this->forward_to_inputs(socket_to_compute, {type, buffer});
      return;
    }

    /* Prepare inputs required to execute the node. */
    GValueMap<StringRef> node_inputs_map{allocator_};
    for (const InputSocketRef *input_socket : node->inputs()) {
      if (input_socket->is_available()) {
        DInputSocket dsocket{node.context(), input_socket};
        Vector<GMutablePointer> values = this->get_input_values(dsocket);
        this->log_socket_value(dsocket, values);
        for (int i = 0; i < values.size(); ++i) {
          /* Values from Multi Input Sockets are stored in input map with the format
           * <identifier>[<index>]. */
          blender::StringRefNull key = allocator_.copy_string(
              input_socket->identifier() + (i > 0 ? ("[" + std::to_string(i)) + "]" : ""));
          node_inputs_map.add_new_direct(key, std::move(values[i]));
        }
      }
    }

    /* Execute the node. */
    GValueMap<StringRef> node_outputs_map{allocator_};
    NodeParamsProvider params_provider;
    params_provider.dnode = node;
    params_provider.self_object = self_object_;
    params_provider.depsgraph = depsgraph_;
    params_provider.allocator = &allocator_;
    params_provider.input_values = &node_inputs_map;
    params_provider.output_values = &node_outputs_map;
    params_provider.modifier = modifier_;
    this->execute_node(node, params_provider);

    /* Forward computed outputs to linked input sockets. */
    for (const OutputSocketRef *output_socket : node->outputs()) {
      if (output_socket->is_available()) {
        const DOutputSocket dsocket{node.context(), output_socket};
        GMutablePointer value = node_outputs_map.extract(output_socket->identifier());
        this->log_socket_value(dsocket, value);
        this->forward_to_inputs(dsocket, value);
      }
    }
  }

  void log_socket_value(const DSocket socket, Span<GPointer> values)
  {
    if (log_socket_value_fn_) {
      log_socket_value_fn_(socket, values);
    }
  }

  void log_socket_value(const DSocket socket, Span<GMutablePointer> values)
  {
    this->log_socket_value(socket, values.cast<GPointer>());
  }

  void log_socket_value(const DSocket socket, GPointer value)
  {
    this->log_socket_value(socket, Span<GPointer>(&value, 1));
  }

  void execute_node(const DNode node, NodeParamsProvider &params_provider)
  {
    const bNode &bnode = *params_provider.dnode->bnode();

    /* Use the geometry-node-execute callback if it exists. */
    if (bnode.typeinfo->geometry_node_execute != nullptr) {
      GeoNodeExecParams params{params_provider};
      bnode.typeinfo->geometry_node_execute(params);
      return;
    }

    /* Use the multi-function implementation if it exists. */
    const MultiFunction *multi_function = mf_by_node_.lookup_default(node, nullptr);
    if (multi_function != nullptr) {
      this->execute_multi_function_node(node, params_provider, *multi_function);
      return;
    }

    /* Just output default values if no implementation exists. */
    this->execute_unknown_node(node, params_provider);
  }

  void execute_multi_function_node(const DNode node,
                                   NodeParamsProvider &params_provider,
                                   const MultiFunction &fn)
  {
    MFContextBuilder fn_context;
    MFParamsBuilder fn_params{fn, 1};
    Vector<GMutablePointer> input_data;
    for (const InputSocketRef *socket_ref : node->inputs()) {
      if (socket_ref->is_available()) {
        GMutablePointer data = params_provider.extract_input(socket_ref->identifier());
        fn_params.add_readonly_single_input(GSpan(*data.type(), data.get(), 1));
        input_data.append(data);
      }
    }
    Vector<GMutablePointer> output_data;
    for (const OutputSocketRef *socket_ref : node->outputs()) {
      if (socket_ref->is_available()) {
        const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket_ref->typeinfo());
        GMutablePointer output_value = params_provider.alloc_output_value(socket_ref->identifier(),
                                                                          type);
        fn_params.add_uninitialized_single_output(GMutableSpan{type, output_value.get(), 1});
        output_data.append(output_value);
      }
    }
    fn.call(IndexRange(1), fn_params, fn_context);
    for (GMutablePointer value : input_data) {
      value.destruct();
    }
  }

  void execute_unknown_node(const DNode node, NodeParamsProvider &params_provider)
  {
    for (const OutputSocketRef *socket : node->outputs()) {
      if (socket->is_available()) {
        const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket->typeinfo());
        params_provider.output_values->add_new_by_copy(socket->identifier(),
                                                       {type, type.default_value()});
      }
    }
  }

  void forward_to_inputs(const DOutputSocket from_socket, GMutablePointer value_to_forward)
  {
    /* For all sockets that are linked with the from_socket push the value to their node. */
    Vector<DInputSocket> to_sockets_all;

    auto handle_target_socket_fn = [&](DInputSocket to_socket) {
      to_sockets_all.append_non_duplicates(to_socket);
    };
    auto handle_skipped_socket_fn = [&, this](DSocket socket) {
      this->log_socket_value(socket, value_to_forward);
    };

    from_socket.foreach_target_socket(handle_target_socket_fn, handle_skipped_socket_fn);

    const CPPType &from_type = *value_to_forward.type();
    Vector<DInputSocket> to_sockets_same_type;
    for (const DInputSocket &to_socket : to_sockets_all) {
      const CPPType &to_type = *blender::nodes::socket_cpp_type_get(*to_socket->typeinfo());
      const std::pair<DInputSocket, DOutputSocket> key = std::make_pair(to_socket, from_socket);
      if (from_type == to_type) {
        to_sockets_same_type.append(to_socket);
      }
      else {
        void *buffer = allocator_.allocate(to_type.size(), to_type.alignment());
        if (conversions_.is_convertible(from_type, to_type)) {
          conversions_.convert_to_uninitialized(
              from_type, to_type, value_to_forward.get(), buffer);
        }
        else {
          to_type.copy_to_uninitialized(to_type.default_value(), buffer);
        }
        add_value_to_input_socket(key, GMutablePointer{to_type, buffer});
      }
    }

    if (to_sockets_same_type.size() == 0) {
      /* This value is not further used, so destruct it. */
      value_to_forward.destruct();
    }
    else if (to_sockets_same_type.size() == 1) {
      /* This value is only used on one input socket, no need to copy it. */
      const DInputSocket to_socket = to_sockets_same_type[0];
      const std::pair<DInputSocket, DOutputSocket> key = std::make_pair(to_socket, from_socket);

      add_value_to_input_socket(key, value_to_forward);
    }
    else {
      /* Multiple inputs use the value, make a copy for every input except for one. */
      const DInputSocket first_to_socket = to_sockets_same_type[0];
      Span<DInputSocket> other_to_sockets = to_sockets_same_type.as_span().drop_front(1);
      const CPPType &type = *value_to_forward.type();
      const std::pair<DInputSocket, DOutputSocket> first_key = std::make_pair(first_to_socket,
                                                                              from_socket);
      add_value_to_input_socket(first_key, value_to_forward);
      for (const DInputSocket &to_socket : other_to_sockets) {
        const std::pair<DInputSocket, DOutputSocket> key = std::make_pair(to_socket, from_socket);
        void *buffer = allocator_.allocate(type.size(), type.alignment());
        type.copy_to_uninitialized(value_to_forward.get(), buffer);
        add_value_to_input_socket(key, GMutablePointer{type, buffer});
      }
    }
  }

  void add_value_to_input_socket(const std::pair<DInputSocket, DOutputSocket> key,
                                 GMutablePointer value)
  {
    value_by_input_.add_new(key, value);
  }

  GMutablePointer get_unlinked_input_value(const DInputSocket &socket,
                                           const CPPType &required_type)
  {
    bNodeSocket *bsocket = socket->bsocket();
    const CPPType &type = *blender::nodes::socket_cpp_type_get(*socket->typeinfo());
    void *buffer = allocator_.allocate(type.size(), type.alignment());
    blender::nodes::socket_cpp_value_get(*bsocket, buffer);

    if (type == required_type) {
      return {type, buffer};
    }
    if (conversions_.is_convertible(type, required_type)) {
      void *converted_buffer = allocator_.allocate(required_type.size(),
                                                   required_type.alignment());
      conversions_.convert_to_uninitialized(type, required_type, buffer, converted_buffer);
      type.destruct(buffer);
      return {required_type, converted_buffer};
    }
    void *default_buffer = allocator_.allocate(required_type.size(), required_type.alignment());
    required_type.copy_to_uninitialized(required_type.default_value(), default_buffer);
    return {required_type, default_buffer};
  }
};

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params)
{
  GeometryNodesEvaluator evaluator{params};
  params.r_output_values = evaluator.execute();
}

}  // namespace blender::modifiers::geometry_nodes
