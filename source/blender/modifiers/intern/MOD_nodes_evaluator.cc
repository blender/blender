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

#include "MOD_nodes_evaluator.hh"

#include "NOD_geometry_exec.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_type_conversions.hh"

#include "DEG_depsgraph_query.h"

#include "FN_field.hh"
#include "FN_field_cpp_type.hh"
#include "FN_generic_value_map.hh"
#include "FN_multi_function.hh"

#include "BLT_translation.h"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_stack.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

namespace blender::modifiers::geometry_nodes {

using fn::CPPType;
using fn::Field;
using fn::FieldCPPType;
using fn::GField;
using fn::GValueMap;
using nodes::GeoNodeExecParams;
using namespace fn::multi_function_types;

enum class ValueUsage : uint8_t {
  /* The value is definitely used. */
  Required,
  /* The value may be used. */
  Maybe,
  /* The value will definitely not be used. */
  Unused,
};

struct SingleInputValue {
  /**
   * Points either to null or to a value of the type of input.
   */
  void *value = nullptr;
};

struct MultiInputValueItem {
  /**
   * The socket where this value is coming from. This is required to sort the inputs correctly
   * based on the link order later on.
   */
  DSocket origin;
  /**
   * Should only be null directly after construction. After that it should always point to a value
   * of the correct type.
   */
  void *value = nullptr;
};

struct MultiInputValue {
  /**
   * Collection of all the inputs that have been provided already. Note, the same origin can occur
   * multiple times. However, it is guaranteed that if two items have the same origin, they will
   * also have the same value (the pointer is different, but they point to values that would
   * compare equal).
   */
  Vector<MultiInputValueItem> items;
  /**
   * Number of items that need to be added until all inputs have been provided.
   */
  int expected_size = 0;
};

struct InputState {

  /**
   * Type of the socket. If this is null, the socket should just be ignored.
   */
  const CPPType *type = nullptr;

  /**
   * Value of this input socket. By default, the value is empty. When other nodes are done
   * computing their outputs, the computed values will be forwarded to linked input sockets.
   * The value will then live here until it is consumed by the node or it was found that the value
   * is not needed anymore.
   * Whether the `single` or `multi` value is used depends on the socket.
   */
  union {
    SingleInputValue *single;
    MultiInputValue *multi;
  } value;

  /**
   * How the node intends to use this input. By default all inputs may be used. Based on which
   * outputs are used, a node can tell the evaluator that an input will definitely be used or is
   * never used. This allows the evaluator to free values early, avoid copies and other unnecessary
   * computations.
   */
  ValueUsage usage = ValueUsage::Maybe;

  /**
   * True when this input is/was used for an execution. While a node is running, only the inputs
   * that have this set to true are allowed to be used. This makes sure that inputs created while
   * the node is running correctly trigger the node to run again. Furthermore, it gives the node a
   * consistent view of which inputs are available that does not change unexpectedly.
   *
   * While the node is running, this can be checked without a lock, because no one is writing to
   * it. If this is true, the value can be read without a lock as well, because the value is not
   * changed by others anymore.
   */
  bool was_ready_for_execution = false;
};

struct OutputState {
  /**
   * If this output has been computed and forwarded already. If this is true, the value is not
   * computed/forwarded again.
   */
  bool has_been_computed = false;

  /**
   * Keeps track of how the output value is used. If a connected input becomes required, this
   * output has to become required as well. The output becomes ignored when it has zero potential
   * users that are counted below.
   */
  ValueUsage output_usage = ValueUsage::Maybe;

  /**
   * This is a copy of `output_usage` that is done right before node execution starts. This is
   * done so that the node gets a consistent view of what outputs are used, even when this changes
   * while the node is running (the node might be reevaluated in that case).
   *
   * While the node is running, this can be checked without a lock, because no one is writing to
   * it.
   */
  ValueUsage output_usage_for_execution = ValueUsage::Maybe;

  /**
   * Counts how many times the value from this output might be used. If this number reaches zero,
   * the output is not needed anymore.
   */
  int potential_users = 0;
};

enum class NodeScheduleState {
  /**
   * Default state of every node.
   */
  NotScheduled,
  /**
   * The node has been added to the task group and will be executed by it in the future.
   */
  Scheduled,
  /**
   * The node is currently running.
   */
  Running,
  /**
   * The node is running and has been rescheduled while running. In this case the node will run
   * again. However, we don't add it to the task group immediately, because then the node might run
   * twice at the same time, which is not allowed. Instead, once the node is done running, it will
   * reschedule itself.
   */
  RunningAndRescheduled,
};

struct NodeState {
  /**
   * Needs to be locked when any data in this state is accessed that is not explicitly marked as
   * otherwise.
   */
  std::mutex mutex;

  /**
   * States of the individual input and output sockets. One can index into these arrays without
   * locking. However, to access the data inside a lock is generally necessary.
   *
   * These spans have to be indexed with the socket index. Unavailable sockets have a state as
   * well. Maybe we can handle unavailable sockets differently in Blender in general, so I did not
   * want to add complexity around it here.
   */
  MutableSpan<InputState> inputs;
  MutableSpan<OutputState> outputs;

  /**
   * Nodes that don't support laziness have some special handling the first time they are executed.
   */
  bool non_lazy_node_is_initialized = false;

  /**
   * Used to check that nodes that don't support laziness do not run more than once.
   */
  bool has_been_executed = false;

  /**
   * Becomes true when the node will never be executed again and its inputs are destructed.
   * Generally, a node has finished once all of its outputs with (potential) users have been
   * computed.
   */
  bool node_has_finished = false;

  /**
   * Counts the number of values that still have to be forwarded to this node until it should run
   * again. It counts values from a multi input socket separately.
   * This is used as an optimization so that nodes are not scheduled unnecessarily in many cases.
   */
  int missing_required_inputs = 0;

  /**
   * A node is always in one specific schedule state. This helps to ensure that the same node does
   * not run twice at the same time accidentally.
   */
  NodeScheduleState schedule_state = NodeScheduleState::NotScheduled;
};

/**
 * Container for a node and its state. Packing them into a single struct allows the use of
 * `VectorSet` instead of a `Map` for `node_states_` which simplifies parallel loops over all
 * states.
 *
 * Equality operators and a hash function for `DNode` are provided so that one can lookup this type
 * in `node_states_` just with a `DNode`.
 */
struct NodeWithState {
  DNode node;
  /* Store a pointer instead of `NodeState` directly to keep it small and movable. */
  NodeState *state = nullptr;

  friend bool operator==(const NodeWithState &a, const NodeWithState &b)
  {
    return a.node == b.node;
  }

  friend bool operator==(const NodeWithState &a, const DNode &b)
  {
    return a.node == b;
  }

  friend bool operator==(const DNode &a, const NodeWithState &b)
  {
    return a == b.node;
  }

  uint64_t hash() const
  {
    return node.hash();
  }

  static uint64_t hash_as(const DNode &node)
  {
    return node.hash();
  }
};

class GeometryNodesEvaluator;

/**
 * Utility class that wraps a node whose state is locked. Having this is a separate class is useful
 * because it allows methods to communicate that they expect the node to be locked.
 */
class LockedNode : NonCopyable, NonMovable {
 public:
  /**
   * This is the node that is currently locked.
   */
  const DNode node;
  NodeState &node_state;

  /**
   * Used to delay notifying (and therefore locking) other nodes until the current node is not
   * locked anymore. This might not be strictly necessary to avoid deadlocks in the current code,
   * but it is a good measure to avoid accidentally adding a deadlock later on. By not locking
   * more than one node per thread at a time, deadlocks are avoided.
   *
   * The notifications will be send right after the node is not locked anymore.
   */
  Vector<DOutputSocket> delayed_required_outputs;
  Vector<DOutputSocket> delayed_unused_outputs;
  Vector<DNode> delayed_scheduled_nodes;

  LockedNode(const DNode node, NodeState &node_state) : node(node), node_state(node_state)
  {
  }
};

static const CPPType *get_socket_cpp_type(const SocketRef &socket)
{
  const bNodeSocketType *typeinfo = socket.typeinfo();
  if (typeinfo->get_geometry_nodes_cpp_type == nullptr) {
    return nullptr;
  }
  const CPPType *type = typeinfo->get_geometry_nodes_cpp_type();
  if (type == nullptr) {
    return nullptr;
  }
  /* The evaluator only supports types that have special member functions. */
  if (!type->has_special_member_functions()) {
    return nullptr;
  }
  return type;
}

static const CPPType *get_socket_cpp_type(const DSocket socket)
{
  return get_socket_cpp_type(*socket.socket_ref());
}

/**
 * \note This is not supposed to be a long term solution. Eventually we want that nodes can
 * specify more complex defaults (other than just single values) in their socket declarations.
 */
static bool get_implicit_socket_input(const SocketRef &socket, void *r_value)
{
  const NodeRef &node = socket.node();
  const nodes::NodeDeclaration *node_declaration = node.declaration();
  if (node_declaration == nullptr) {
    return false;
  }
  const nodes::SocketDeclaration &socket_declaration = *node_declaration->inputs()[socket.index()];
  if (socket_declaration.input_field_type() == nodes::InputSocketFieldType::Implicit) {
    if (socket.typeinfo()->type == SOCK_VECTOR) {
      const bNode &bnode = *socket.bnode();
      if (bnode.type == GEO_NODE_SET_CURVE_HANDLES) {
        StringRef side = ((NodeGeometrySetCurveHandlePositions *)bnode.storage)->mode ==
                                 GEO_NODE_CURVE_HANDLE_LEFT ?
                             "handle_left" :
                             "handle_right";
        new (r_value) Field<float3>(bke::AttributeFieldInput::Create<float3>(side));
        return true;
      }
      new (r_value) Field<float3>(bke::AttributeFieldInput::Create<float3>("position"));
      return true;
    }
    if (socket.typeinfo()->type == SOCK_INT) {
      new (r_value) Field<int>(std::make_shared<fn::IndexFieldInput>());
      return true;
    }
  }
  return false;
}

static void get_socket_value(const SocketRef &socket, void *r_value)
{
  if (get_implicit_socket_input(socket, r_value)) {
    return;
  }

  const bNodeSocketType *typeinfo = socket.typeinfo();
  typeinfo->get_geometry_nodes_cpp_value(*socket.bsocket(), r_value);
}

static bool node_supports_laziness(const DNode node)
{
  return node->typeinfo()->geometry_node_execute_supports_laziness;
}

/** Implements the callbacks that might be called when a node is executed. */
class NodeParamsProvider : public nodes::GeoNodeExecParamsProvider {
 private:
  GeometryNodesEvaluator &evaluator_;
  NodeState &node_state_;

 public:
  NodeParamsProvider(GeometryNodesEvaluator &evaluator, DNode dnode, NodeState &node_state);

  bool can_get_input(StringRef identifier) const override;
  bool can_set_output(StringRef identifier) const override;
  GMutablePointer extract_input(StringRef identifier) override;
  Vector<GMutablePointer> extract_multi_input(StringRef identifier) override;
  GPointer get_input(StringRef identifier) const override;
  GMutablePointer alloc_output_value(const CPPType &type) override;
  void set_output(StringRef identifier, GMutablePointer value) override;
  void set_input_unused(StringRef identifier) override;
  bool output_is_required(StringRef identifier) const override;

  bool lazy_require_input(StringRef identifier) override;
  bool lazy_output_is_required(StringRef identifier) const override;
};

class GeometryNodesEvaluator {
 private:
  /**
   * This allocator lives on after the evaluator has been destructed. Therefore outputs of the
   * entire evaluator should be allocated here.
   */
  LinearAllocator<> &outer_allocator_;
  /**
   * A local linear allocator for each thread. Only use this for values that do not need to live
   * longer than the lifetime of the evaluator itself. Considerations for the future:
   * - We could use an allocator that can free here, some temporary values don't live long.
   * - If we ever run into false sharing bottlenecks, we could use local allocators that allocate
   *   on cache line boundaries. Note, just because a value is allocated in one specific thread,
   *   does not mean that it will only be used by that thread.
   */
  threading::EnumerableThreadSpecific<LinearAllocator<>> local_allocators_;

  /**
   * Every node that is reachable from the output gets its own state. Once all states have been
   * constructed, this map can be used for lookups from multiple threads.
   */
  VectorSet<NodeWithState> node_states_;

  /**
   * Contains all the tasks for the nodes that are currently scheduled.
   */
  TaskPool *task_pool_ = nullptr;

  GeometryNodesEvaluationParams &params_;
  const blender::nodes::DataTypeConversions &conversions_;

  friend NodeParamsProvider;

 public:
  GeometryNodesEvaluator(GeometryNodesEvaluationParams &params)
      : outer_allocator_(params.allocator),
        params_(params),
        conversions_(blender::nodes::get_implicit_type_conversions())
  {
  }

  void execute()
  {
    task_pool_ = BLI_task_pool_create(this, TASK_PRIORITY_HIGH);

    this->create_states_for_reachable_nodes();
    this->forward_group_inputs();
    this->schedule_initial_nodes();

    /* This runs until all initially requested inputs have been computed. */
    BLI_task_pool_work_and_wait(task_pool_);
    BLI_task_pool_free(task_pool_);

    this->extract_group_outputs();
    this->destruct_node_states();
  }

  void create_states_for_reachable_nodes()
  {
    /* This does a depth first search for all the nodes that are reachable from the group
     * outputs. This finds all nodes that are relevant. */
    Stack<DNode> nodes_to_check;
    /* Start at the output sockets. */
    for (const DInputSocket &socket : params_.output_sockets) {
      nodes_to_check.push(socket.node());
    }
    for (const DSocket &socket : params_.force_compute_sockets) {
      nodes_to_check.push(socket.node());
    }
    /* Use the local allocator because the states do not need to outlive the evaluator. */
    LinearAllocator<> &allocator = local_allocators_.local();
    while (!nodes_to_check.is_empty()) {
      const DNode node = nodes_to_check.pop();
      if (node_states_.contains_as(node)) {
        /* This node has been handled already. */
        continue;
      }
      /* Create a new state for the node. */
      NodeState &node_state = *allocator.construct<NodeState>().release();
      node_states_.add_new({node, &node_state});

      /* Push all linked origins on the stack. */
      for (const InputSocketRef *input_ref : node->inputs()) {
        const DInputSocket input{node.context(), input_ref};
        input.foreach_origin_socket(
            [&](const DSocket origin) { nodes_to_check.push(origin.node()); });
      }
    }

    /* Initialize the more complex parts of the node states in parallel. At this point no new
     * node states are added anymore, so it is safe to lookup states from `node_states_` from
     * multiple threads. */
    threading::parallel_for(
        IndexRange(node_states_.size()), 50, [&, this](const IndexRange range) {
          LinearAllocator<> &allocator = this->local_allocators_.local();
          for (const NodeWithState &item : node_states_.as_span().slice(range)) {
            this->initialize_node_state(item.node, *item.state, allocator);
          }
        });
  }

  void initialize_node_state(const DNode node, NodeState &node_state, LinearAllocator<> &allocator)
  {
    /* Construct arrays of the correct size. */
    node_state.inputs = allocator.construct_array<InputState>(node->inputs().size());
    node_state.outputs = allocator.construct_array<OutputState>(node->outputs().size());

    /* Initialize input states. */
    for (const int i : node->inputs().index_range()) {
      InputState &input_state = node_state.inputs[i];
      const DInputSocket socket = node.input(i);
      if (!socket->is_available()) {
        /* Unavailable sockets should never be used. */
        input_state.type = nullptr;
        input_state.usage = ValueUsage::Unused;
        continue;
      }
      const CPPType *type = get_socket_cpp_type(socket);
      input_state.type = type;
      if (type == nullptr) {
        /* This is not a known data socket, it shouldn't be used. */
        input_state.usage = ValueUsage::Unused;
        continue;
      }
      /* Construct the correct struct that can hold the input(s). */
      if (socket->is_multi_input_socket()) {
        input_state.value.multi = allocator.construct<MultiInputValue>().release();
        /* Count how many values should be added until the socket is complete. */
        socket.foreach_origin_socket(
            [&](DSocket UNUSED(origin)) { input_state.value.multi->expected_size++; });
        /* If no links are connected, we do read the value from socket itself. */
        if (input_state.value.multi->expected_size == 0) {
          input_state.value.multi->expected_size = 1;
        }
      }
      else {
        input_state.value.single = allocator.construct<SingleInputValue>().release();
      }
    }
    /* Initialize output states. */
    for (const int i : node->outputs().index_range()) {
      OutputState &output_state = node_state.outputs[i];
      const DOutputSocket socket = node.output(i);
      if (!socket->is_available()) {
        /* Unavailable outputs should never be used. */
        output_state.output_usage = ValueUsage::Unused;
        continue;
      }
      const CPPType *type = get_socket_cpp_type(socket);
      if (type == nullptr) {
        /* Non data sockets should never be used. */
        output_state.output_usage = ValueUsage::Unused;
        continue;
      }
      /* Count the number of potential users for this socket. */
      socket.foreach_target_socket(
          [&, this](const DInputSocket target_socket) {
            const DNode target_node = target_socket.node();
            if (!this->node_states_.contains_as(target_node)) {
              /* The target node is not computed because it is not computed to the output. */
              return;
            }
            output_state.potential_users += 1;
          },
          {});
      if (output_state.potential_users == 0) {
        /* If it does not have any potential users, it is unused. It might become required again in
         * `schedule_initial_nodes`. */
        output_state.output_usage = ValueUsage::Unused;
      }
    }
  }

  void destruct_node_states()
  {
    threading::parallel_for(
        IndexRange(node_states_.size()), 50, [&, this](const IndexRange range) {
          for (const NodeWithState &item : node_states_.as_span().slice(range)) {
            this->destruct_node_state(item.node, *item.state);
          }
        });
  }

  void destruct_node_state(const DNode node, NodeState &node_state)
  {
    /* Need to destruct stuff manually, because it's allocated by a custom allocator. */
    for (const int i : node->inputs().index_range()) {
      InputState &input_state = node_state.inputs[i];
      if (input_state.type == nullptr) {
        continue;
      }
      const InputSocketRef &socket_ref = node->input(i);
      if (socket_ref.is_multi_input_socket()) {
        MultiInputValue &multi_value = *input_state.value.multi;
        for (MultiInputValueItem &item : multi_value.items) {
          input_state.type->destruct(item.value);
        }
        multi_value.~MultiInputValue();
      }
      else {
        SingleInputValue &single_value = *input_state.value.single;
        void *value = single_value.value;
        if (value != nullptr) {
          input_state.type->destruct(value);
        }
        single_value.~SingleInputValue();
      }
    }

    destruct_n(node_state.inputs.data(), node_state.inputs.size());
    destruct_n(node_state.outputs.data(), node_state.outputs.size());

    node_state.~NodeState();
  }

  void forward_group_inputs()
  {
    for (auto &&item : params_.input_values.items()) {
      const DOutputSocket socket = item.key;
      GMutablePointer value = item.value;

      const DNode node = socket.node();
      if (!node_states_.contains_as(node)) {
        /* The socket is not connected to any output. */
        this->log_socket_value({socket}, value);
        value.destruct();
        continue;
      }
      this->forward_output(socket, value);
    }
  }

  void schedule_initial_nodes()
  {
    for (const DInputSocket &socket : params_.output_sockets) {
      const DNode node = socket.node();
      NodeState &node_state = this->get_node_state(node);
      this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
        /* Setting an input as required will schedule any linked node. */
        this->set_input_required(locked_node, socket);
      });
    }
    for (const DSocket socket : params_.force_compute_sockets) {
      const DNode node = socket.node();
      NodeState &node_state = this->get_node_state(node);
      this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
        if (socket->is_input()) {
          this->set_input_required(locked_node, DInputSocket(socket));
        }
        else {
          OutputState &output_state = node_state.outputs[socket->index()];
          output_state.output_usage = ValueUsage::Required;
          this->schedule_node(locked_node);
        }
      });
    }
  }

  void schedule_node(LockedNode &locked_node)
  {
    switch (locked_node.node_state.schedule_state) {
      case NodeScheduleState::NotScheduled: {
        /* The node will be scheduled once it is not locked anymore. We could schedule the node
         * right here, but that would result in a deadlock if the task pool decides to run the task
         * immediately (this only happens when Blender is started with a single thread). */
        locked_node.node_state.schedule_state = NodeScheduleState::Scheduled;
        locked_node.delayed_scheduled_nodes.append(locked_node.node);
        break;
      }
      case NodeScheduleState::Scheduled: {
        /* Scheduled already, nothing to do. */
        break;
      }
      case NodeScheduleState::Running: {
        /* Reschedule node while it is running.
         * The node will reschedule itself when it is done. */
        locked_node.node_state.schedule_state = NodeScheduleState::RunningAndRescheduled;
        break;
      }
      case NodeScheduleState::RunningAndRescheduled: {
        /* Scheduled already, nothing to do. */
        break;
      }
    }
  }

  static void run_node_from_task_pool(TaskPool *task_pool, void *task_data)
  {
    void *user_data = BLI_task_pool_user_data(task_pool);
    GeometryNodesEvaluator &evaluator = *(GeometryNodesEvaluator *)user_data;
    const NodeWithState *node_with_state = (const NodeWithState *)task_data;

    evaluator.node_task_run(node_with_state->node, *node_with_state->state);
  }

  void node_task_run(const DNode node, NodeState &node_state)
  {
    /* These nodes are sometimes scheduled. We could also check for them in other places, but
     * it's the easiest to do it here. */
    if (node->is_group_input_node() || node->is_group_output_node()) {
      return;
    }

    const bool do_execute_node = this->node_task_preprocessing(node, node_state);

    /* Only execute the node if all prerequisites are met. There has to be an output that is
     * required and all required inputs have to be provided already. */
    if (do_execute_node) {
      this->execute_node(node, node_state);
    }

    this->node_task_postprocessing(node, node_state);
  }

  bool node_task_preprocessing(const DNode node, NodeState &node_state)
  {
    bool do_execute_node = false;
    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      BLI_assert(node_state.schedule_state == NodeScheduleState::Scheduled);
      node_state.schedule_state = NodeScheduleState::Running;

      /* Early return if the node has finished already. */
      if (locked_node.node_state.node_has_finished) {
        return;
      }
      /* Prepare outputs and check if actually any new outputs have to be computed. */
      if (!this->prepare_node_outputs_for_execution(locked_node)) {
        return;
      }
      /* Initialize nodes that don't support laziness. This is done after at least one output is
       * required and before we check that all required inputs are provided. This reduces the
       * number of "round-trips" through the task pool by one for most nodes. */
      if (!node_state.non_lazy_node_is_initialized && !node_supports_laziness(node)) {
        this->initialize_non_lazy_node(locked_node);
        node_state.non_lazy_node_is_initialized = true;
      }
      /* Prepare inputs and check if all required inputs are provided. */
      if (!this->prepare_node_inputs_for_execution(locked_node)) {
        return;
      }
      do_execute_node = true;
    });
    return do_execute_node;
  }

  /* A node is finished when it has computed all outputs that may be used. */
  bool finish_node_if_possible(LockedNode &locked_node)
  {
    if (locked_node.node_state.node_has_finished) {
      /* Early return in case this node is known to have finished already. */
      return true;
    }

    /* Check if there is any output that might be used but has not been computed yet. */
    bool has_remaining_output = false;
    for (OutputState &output_state : locked_node.node_state.outputs) {
      if (output_state.has_been_computed) {
        continue;
      }
      if (output_state.output_usage != ValueUsage::Unused) {
        has_remaining_output = true;
        break;
      }
    }
    if (!has_remaining_output) {
      /* If there are no remaining outputs, all the inputs can be destructed and/or can become
       * unused. This can also trigger a chain reaction where nodes to the left become finished
       * too. */
      for (const int i : locked_node.node->inputs().index_range()) {
        const DInputSocket socket = locked_node.node.input(i);
        InputState &input_state = locked_node.node_state.inputs[i];
        if (input_state.usage == ValueUsage::Maybe) {
          this->set_input_unused(locked_node, socket);
        }
        else if (input_state.usage == ValueUsage::Required) {
          /* The value was required, so it cannot become unused. However, we can destruct the
           * value. */
          this->destruct_input_value_if_exists(locked_node, socket);
        }
      }
      locked_node.node_state.node_has_finished = true;
    }
    return locked_node.node_state.node_has_finished;
  }

  bool prepare_node_outputs_for_execution(LockedNode &locked_node)
  {
    bool execution_is_necessary = false;
    for (OutputState &output_state : locked_node.node_state.outputs) {
      /* Update the output usage for execution to the latest value. */
      output_state.output_usage_for_execution = output_state.output_usage;
      if (!output_state.has_been_computed) {
        if (output_state.output_usage == ValueUsage::Required) {
          /* Only evaluate when there is an output that is required but has not been computed. */
          execution_is_necessary = true;
        }
      }
    }
    return execution_is_necessary;
  }

  void initialize_non_lazy_node(LockedNode &locked_node)
  {
    for (const int i : locked_node.node->inputs().index_range()) {
      InputState &input_state = locked_node.node_state.inputs[i];
      if (input_state.type == nullptr) {
        /* Ignore unavailable/non-data sockets. */
        continue;
      }
      /* Nodes that don't support laziness require all inputs. */
      const DInputSocket input_socket = locked_node.node.input(i);
      this->set_input_required(locked_node, input_socket);
    }
  }

  /**
   * Checks if requested inputs are available and "marks" all the inputs that are available
   * during the node execution. Inputs that are provided after this function ends but before the
   * node is executed, cannot be read by the node in the execution (note that this only affects
   * nodes that support lazy inputs).
   */
  bool prepare_node_inputs_for_execution(LockedNode &locked_node)
  {
    for (const int i : locked_node.node_state.inputs.index_range()) {
      InputState &input_state = locked_node.node_state.inputs[i];
      if (input_state.type == nullptr) {
        /* Ignore unavailable and non-data sockets. */
        continue;
      }
      const DInputSocket socket = locked_node.node.input(i);
      const bool is_required = input_state.usage == ValueUsage::Required;

      /* No need to check this socket again. */
      if (input_state.was_ready_for_execution) {
        continue;
      }

      if (socket->is_multi_input_socket()) {
        MultiInputValue &multi_value = *input_state.value.multi;
        /* Checks if all the linked sockets have been provided already. */
        if (multi_value.items.size() == multi_value.expected_size) {
          input_state.was_ready_for_execution = true;
        }
        else if (is_required) {
          /* The input is required but is not fully provided yet. Therefore the node cannot be
           * executed yet. */
          return false;
        }
      }
      else {
        SingleInputValue &single_value = *input_state.value.single;
        if (single_value.value != nullptr) {
          input_state.was_ready_for_execution = true;
        }
        else if (is_required) {
          /* The input is required but has not been provided yet. Therefore the node cannot be
           * executed yet. */
          return false;
        }
      }
    }
    /* All required inputs have been provided. */
    return true;
  }

  /**
   * Actually execute the node. All the required inputs are available and at least one output is
   * required.
   */
  void execute_node(const DNode node, NodeState &node_state)
  {
    const bNode &bnode = *node->bnode();

    if (node_state.has_been_executed) {
      if (!node_supports_laziness(node)) {
        /* Nodes that don't support laziness must not be executed more than once. */
        BLI_assert_unreachable();
      }
    }
    node_state.has_been_executed = true;

    /* Use the geometry node execute callback if it exists. */
    if (bnode.typeinfo->geometry_node_execute != nullptr) {
      this->execute_geometry_node(node, node_state);
      return;
    }

    /* Use the multi-function implementation if it exists. */
    const nodes::NodeMultiFunctions::Item &fn_item = params_.mf_by_node->try_get(node);
    if (fn_item.fn != nullptr) {
      this->execute_multi_function_node(node, fn_item, node_state);
      return;
    }

    this->execute_unknown_node(node, node_state);
  }

  void execute_geometry_node(const DNode node, NodeState &node_state)
  {
    const bNode &bnode = *node->bnode();

    NodeParamsProvider params_provider{*this, node, node_state};
    GeoNodeExecParams params{params_provider};
    if (node->idname().find("Legacy") != StringRef::not_found) {
      params.error_message_add(geo_log::NodeWarningType::Legacy,
                               TIP_("Legacy node will be removed before Blender 4.0"));
    }
    bnode.typeinfo->geometry_node_execute(params);
  }

  void execute_multi_function_node(const DNode node,
                                   const nodes::NodeMultiFunctions::Item &fn_item,
                                   NodeState &node_state)
  {
    if (node->idname().find("Legacy") != StringRef::not_found) {
      /* Create geometry nodes params just for creating an error message. */
      NodeParamsProvider params_provider{*this, node, node_state};
      GeoNodeExecParams params{params_provider};
      params.error_message_add(geo_log::NodeWarningType::Legacy,
                               TIP_("Legacy node will be removed before Blender 4.0"));
    }

    LinearAllocator<> &allocator = local_allocators_.local();

    /* Prepare the inputs for the multi function. */
    Vector<GField> input_fields;
    for (const int i : node->inputs().index_range()) {
      const InputSocketRef &socket_ref = node->input(i);
      if (!socket_ref.is_available()) {
        continue;
      }
      BLI_assert(!socket_ref.is_multi_input_socket());
      InputState &input_state = node_state.inputs[i];
      BLI_assert(input_state.was_ready_for_execution);
      SingleInputValue &single_value = *input_state.value.single;
      BLI_assert(single_value.value != nullptr);
      input_fields.append(std::move(*(GField *)single_value.value));
    }

    std::shared_ptr<fn::FieldOperation> operation;
    if (fn_item.owned_fn) {
      operation = std::make_shared<fn::FieldOperation>(fn_item.owned_fn, std::move(input_fields));
    }
    else {
      operation = std::make_shared<fn::FieldOperation>(*fn_item.fn, std::move(input_fields));
    }

    /* Forward outputs. */
    int output_index = 0;
    for (const int i : node->outputs().index_range()) {
      const OutputSocketRef &socket_ref = node->output(i);
      if (!socket_ref.is_available()) {
        continue;
      }
      OutputState &output_state = node_state.outputs[i];
      const DOutputSocket socket{node.context(), &socket_ref};
      const CPPType *cpp_type = get_socket_cpp_type(socket_ref);
      GField new_field{operation, output_index};
      new_field = fn::make_field_constant_if_possible(std::move(new_field));
      GField &field_to_forward = *allocator.construct<GField>(std::move(new_field)).release();
      this->forward_output(socket, {cpp_type, &field_to_forward});
      output_state.has_been_computed = true;
      output_index++;
    }
  }

  void execute_unknown_node(const DNode node, NodeState &node_state)
  {
    LinearAllocator<> &allocator = local_allocators_.local();
    for (const OutputSocketRef *socket : node->outputs()) {
      if (!socket->is_available()) {
        continue;
      }
      const CPPType *type = get_socket_cpp_type(*socket);
      if (type == nullptr) {
        continue;
      }
      /* Just forward the default value of the type as a fallback. That's typically better than
       * crashing or doing nothing. */
      OutputState &output_state = node_state.outputs[socket->index()];
      output_state.has_been_computed = true;
      void *buffer = allocator.allocate(type->size(), type->alignment());
      this->construct_default_value(*type, buffer);
      this->forward_output({node.context(), socket}, {*type, buffer});
    }
  }

  void node_task_postprocessing(const DNode node, NodeState &node_state)
  {
    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      const bool node_has_finished = this->finish_node_if_possible(locked_node);
      const bool reschedule_requested = node_state.schedule_state ==
                                        NodeScheduleState::RunningAndRescheduled;
      node_state.schedule_state = NodeScheduleState::NotScheduled;
      if (reschedule_requested && !node_has_finished) {
        /* Either the node rescheduled itself or another node tried to schedule it while it ran. */
        this->schedule_node(locked_node);
      }

      this->assert_expected_outputs_have_been_computed(locked_node);
    });
  }

  void assert_expected_outputs_have_been_computed(LockedNode &locked_node)
  {
#ifdef DEBUG
    /* Outputs can only be computed when all required inputs have been provided. */
    if (locked_node.node_state.missing_required_inputs > 0) {
      return;
    }
    /* If the node is still scheduled, it is not necessary that all its expected outputs are
     * computed yet. */
    if (locked_node.node_state.schedule_state == NodeScheduleState::Scheduled) {
      return;
    }

    const bool supports_laziness = node_supports_laziness(locked_node.node);
    /* Iterating over sockets instead of the states directly, because that makes it easier to
     * figure out which socket is missing when one of the asserts is hit. */
    for (const OutputSocketRef *socket_ref : locked_node.node->outputs()) {
      OutputState &output_state = locked_node.node_state.outputs[socket_ref->index()];
      if (supports_laziness) {
        /* Expected that at least all required sockets have been computed. If more outputs become
         * required later, the node will be executed again. */
        if (output_state.output_usage_for_execution == ValueUsage::Required) {
          BLI_assert(output_state.has_been_computed);
        }
      }
      else {
        /* Expect that all outputs that may be used have been computed, because the node cannot
         * be executed again. */
        if (output_state.output_usage_for_execution != ValueUsage::Unused) {
          BLI_assert(output_state.has_been_computed);
        }
      }
    }
#else
    UNUSED_VARS(locked_node);
#endif
  }

  void extract_group_outputs()
  {
    for (const DInputSocket &socket : params_.output_sockets) {
      BLI_assert(socket->is_available());
      BLI_assert(!socket->is_multi_input_socket());

      const DNode node = socket.node();
      NodeState &node_state = this->get_node_state(node);
      InputState &input_state = node_state.inputs[socket->index()];

      SingleInputValue &single_value = *input_state.value.single;
      void *value = single_value.value;

      /* The value should have been computed by now. If this assert is hit, it means that there
       * was some scheduling issue before. */
      BLI_assert(value != nullptr);

      /* Move value into memory owned by the outer allocator. */
      const CPPType &type = *input_state.type;
      void *buffer = outer_allocator_.allocate(type.size(), type.alignment());
      type.move_construct(value, buffer);

      params_.r_output_values.append({type, buffer});
    }
  }

  /**
   * Load the required input from the socket or trigger nodes to the left to compute the value.
   * When this function is called, the node will always be executed again eventually (either
   * immediately, or when all required inputs have been computed by other nodes).
   */
  void set_input_required(LockedNode &locked_node, const DInputSocket input_socket)
  {
    BLI_assert(locked_node.node == input_socket.node());
    InputState &input_state = locked_node.node_state.inputs[input_socket->index()];

    /* Value set as unused cannot become used again. */
    BLI_assert(input_state.usage != ValueUsage::Unused);

    if (input_state.usage == ValueUsage::Required) {
      /* The value is already required, but the node might expect to be evaluated again. */
      this->schedule_node(locked_node);
      /* Returning here also ensure that the code below is executed at most once per input. */
      return;
    }
    input_state.usage = ValueUsage::Required;

    if (input_state.was_ready_for_execution) {
      /* The value was already ready, but the node might expect to be evaluated again. */
      this->schedule_node(locked_node);
      return;
    }

    /* Count how many values still have to be added to this input until it is "complete". */
    int missing_values = 0;
    if (input_socket->is_multi_input_socket()) {
      MultiInputValue &multi_value = *input_state.value.multi;
      missing_values = multi_value.expected_size - multi_value.items.size();
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      if (single_value.value == nullptr) {
        missing_values = 1;
      }
    }
    if (missing_values == 0) {
      /* The input is fully available already, but the node might expect to be evaluated again. */
      this->schedule_node(locked_node);
      return;
    }
    /* Increase the total number of missing required inputs. This ensures that the node will be
     * scheduled correctly when all inputs have been provided. */
    locked_node.node_state.missing_required_inputs += missing_values;

    /* Get all origin sockets, because we have to tag those as required as well. */
    Vector<DSocket> origin_sockets;
    input_socket.foreach_origin_socket(
        [&](const DSocket origin_socket) { origin_sockets.append(origin_socket); });

    if (origin_sockets.is_empty()) {
      /* If there are no origin sockets, just load the value from the socket directly. */
      this->load_unlinked_input_value(locked_node, input_socket, input_state, input_socket);
      locked_node.node_state.missing_required_inputs -= 1;
      this->schedule_node(locked_node);
      return;
    }
    bool will_be_triggered_by_other_node = false;
    for (const DSocket &origin_socket : origin_sockets) {
      if (origin_socket->is_input()) {
        /* Load the value directly from the origin socket. In most cases this is an unlinked
         * group input. */
        this->load_unlinked_input_value(locked_node, input_socket, input_state, origin_socket);
        locked_node.node_state.missing_required_inputs -= 1;
        this->schedule_node(locked_node);
      }
      else {
        /* The value has not been computed yet, so when it will be forwarded by another node, this
         * node will be triggered. */
        will_be_triggered_by_other_node = true;

        locked_node.delayed_required_outputs.append(DOutputSocket(origin_socket));
      }
    }
    /* If this node will be triggered by another node, we don't have to schedule it now. */
    if (!will_be_triggered_by_other_node) {
      this->schedule_node(locked_node);
    }
  }

  void set_input_unused(LockedNode &locked_node, const DInputSocket socket)
  {
    InputState &input_state = locked_node.node_state.inputs[socket->index()];

    /* A required socket cannot become unused. */
    BLI_assert(input_state.usage != ValueUsage::Required);

    if (input_state.usage == ValueUsage::Unused) {
      /* Nothing to do in this case. */
      return;
    }
    input_state.usage = ValueUsage::Unused;

    /* If the input is unused, it's value can be destructed now. */
    this->destruct_input_value_if_exists(locked_node, socket);

    if (input_state.was_ready_for_execution) {
      /* If the value was already computed, we don't need to notify origin nodes. */
      return;
    }

    /* Notify origin nodes that might want to set its inputs as unused as well. */
    socket.foreach_origin_socket([&](const DSocket origin_socket) {
      if (origin_socket->is_input()) {
        /* Values from these sockets are loaded directly from the sockets, so there is no node to
         * notify. */
        return;
      }
      /* Delay notification of the other node until this node is not locked anymore. */
      locked_node.delayed_unused_outputs.append(DOutputSocket(origin_socket));
    });
  }

  void send_output_required_notification(const DOutputSocket socket)
  {
    const DNode node = socket.node();
    NodeState &node_state = this->get_node_state(node);
    OutputState &output_state = node_state.outputs[socket->index()];

    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      if (output_state.output_usage == ValueUsage::Required) {
        /* Output is marked as required already. So the node is scheduled already. */
        return;
      }
      /* The origin node needs to be scheduled so that it provides the requested input
       * eventually. */
      output_state.output_usage = ValueUsage::Required;
      this->schedule_node(locked_node);
    });
  }

  void send_output_unused_notification(const DOutputSocket socket)
  {
    const DNode node = socket.node();
    NodeState &node_state = this->get_node_state(node);
    OutputState &output_state = node_state.outputs[socket->index()];

    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      output_state.potential_users -= 1;
      if (output_state.potential_users == 0) {
        /* The socket might be required even though the output is not used by other sockets. That
         * can happen when the socket is forced to be computed. */
        if (output_state.output_usage != ValueUsage::Required) {
          /* The output socket has no users anymore. */
          output_state.output_usage = ValueUsage::Unused;
          /* Schedule the origin node in case it wants to set its inputs as unused as well. */
          this->schedule_node(locked_node);
        }
      }
    });
  }

  void add_node_to_task_pool(const DNode node)
  {
    /* Push the task to the pool while it is not locked to avoid a deadlock in case when the task
     * is executed immediately. */
    const NodeWithState *node_with_state = node_states_.lookup_key_ptr_as(node);
    BLI_task_pool_push(
        task_pool_, run_node_from_task_pool, (void *)node_with_state, false, nullptr);
  }

  /**
   * Moves a newly computed value from an output socket to all the inputs that might need it.
   */
  void forward_output(const DOutputSocket from_socket, GMutablePointer value_to_forward)
  {
    BLI_assert(value_to_forward.get() != nullptr);

    Vector<DSocket> sockets_to_log_to;
    sockets_to_log_to.append(from_socket);

    Vector<DInputSocket> to_sockets;
    auto handle_target_socket_fn = [&, this](const DInputSocket to_socket) {
      if (this->should_forward_to_socket(to_socket)) {
        to_sockets.append(to_socket);
      }
    };
    auto handle_skipped_socket_fn = [&](const DSocket socket) {
      sockets_to_log_to.append(socket);
    };
    from_socket.foreach_target_socket(handle_target_socket_fn, handle_skipped_socket_fn);

    LinearAllocator<> &allocator = local_allocators_.local();

    const CPPType &from_type = *value_to_forward.type();
    Vector<DInputSocket> to_sockets_same_type;
    for (const DInputSocket &to_socket : to_sockets) {
      const CPPType &to_type = *get_socket_cpp_type(to_socket);
      if (from_type == to_type) {
        /* All target sockets that do not need a conversion will be handled afterwards. */
        to_sockets_same_type.append(to_socket);
        /* Multi input socket values are logged once all values are available. */
        if (!to_socket->is_multi_input_socket()) {
          sockets_to_log_to.append(to_socket);
        }
        continue;
      }
      this->forward_to_socket_with_different_type(
          allocator, value_to_forward, from_socket, to_socket, to_type);
    }

    this->log_socket_value(sockets_to_log_to, value_to_forward);

    this->forward_to_sockets_with_same_type(
        allocator, to_sockets_same_type, value_to_forward, from_socket);
  }

  bool should_forward_to_socket(const DInputSocket socket)
  {
    const DNode to_node = socket.node();
    const NodeWithState *target_node_with_state = node_states_.lookup_key_ptr_as(to_node);
    if (target_node_with_state == nullptr) {
      /* If the socket belongs to a node that has no state, the entire node is not used. */
      return false;
    }
    NodeState &target_node_state = *target_node_with_state->state;
    InputState &target_input_state = target_node_state.inputs[socket->index()];

    std::lock_guard lock{target_node_state.mutex};
    /* Do not forward to an input socket whose value won't be used. */
    return target_input_state.usage != ValueUsage::Unused;
  }

  void forward_to_socket_with_different_type(LinearAllocator<> &allocator,
                                             const GPointer value_to_forward,
                                             const DOutputSocket from_socket,
                                             const DInputSocket to_socket,
                                             const CPPType &to_type)
  {
    const CPPType &from_type = *value_to_forward.type();

    /* Allocate a buffer for the converted value. */
    void *buffer = allocator.allocate(to_type.size(), to_type.alignment());
    GMutablePointer value{to_type, buffer};

    this->convert_value(from_type, to_type, value_to_forward.get(), buffer);

    /* Multi input socket values are logged once all values are available. */
    if (!to_socket->is_multi_input_socket()) {
      this->log_socket_value({to_socket}, value);
    }
    this->add_value_to_input_socket(to_socket, from_socket, value);
  }

  void forward_to_sockets_with_same_type(LinearAllocator<> &allocator,
                                         Span<DInputSocket> to_sockets,
                                         GMutablePointer value_to_forward,
                                         const DOutputSocket from_socket)
  {
    if (to_sockets.is_empty()) {
      /* Value is not used anymore, so it can be destructed. */
      value_to_forward.destruct();
    }
    else if (to_sockets.size() == 1) {
      /* Value is only used by one input socket, no need to copy it. */
      const DInputSocket to_socket = to_sockets[0];
      this->add_value_to_input_socket(to_socket, from_socket, value_to_forward);
    }
    else {
      /* Multiple inputs use the value, make a copy for every input except for one. */
      /* First make the copies, so that the next node does not start modifying the value while we
       * are still making copies. */
      const CPPType &type = *value_to_forward.type();
      for (const DInputSocket &to_socket : to_sockets.drop_front(1)) {
        void *buffer = allocator.allocate(type.size(), type.alignment());
        type.copy_construct(value_to_forward.get(), buffer);
        this->add_value_to_input_socket(to_socket, from_socket, {type, buffer});
      }
      /* Forward the original value to one of the targets. */
      const DInputSocket to_socket = to_sockets[0];
      this->add_value_to_input_socket(to_socket, from_socket, value_to_forward);
    }
  }

  void add_value_to_input_socket(const DInputSocket socket,
                                 const DOutputSocket origin,
                                 GMutablePointer value)
  {
    BLI_assert(socket->is_available());

    const DNode node = socket.node();
    NodeState &node_state = this->get_node_state(node);
    InputState &input_state = node_state.inputs[socket->index()];

    this->with_locked_node(node, node_state, [&](LockedNode &locked_node) {
      if (socket->is_multi_input_socket()) {
        /* Add a new value to the multi-input. */
        MultiInputValue &multi_value = *input_state.value.multi;
        multi_value.items.append({origin, value.get()});

        if (multi_value.expected_size == multi_value.items.size()) {
          this->log_socket_value({socket}, input_state, multi_value.items);
        }
      }
      else {
        /* Assign the value to the input. */
        SingleInputValue &single_value = *input_state.value.single;
        BLI_assert(single_value.value == nullptr);
        single_value.value = value.get();
      }

      if (input_state.usage == ValueUsage::Required) {
        node_state.missing_required_inputs--;
        if (node_state.missing_required_inputs == 0) {
          /* Schedule node if all the required inputs have been provided. */
          this->schedule_node(locked_node);
        }
      }
    });
  }

  void load_unlinked_input_value(LockedNode &locked_node,
                                 const DInputSocket input_socket,
                                 InputState &input_state,
                                 const DSocket origin_socket)
  {
    /* Only takes locked node as parameter, because the node needs to be locked. */
    UNUSED_VARS(locked_node);

    GMutablePointer value = this->get_value_from_socket(origin_socket, *input_state.type);
    if (input_socket->is_multi_input_socket()) {
      MultiInputValue &multi_value = *input_state.value.multi;
      multi_value.items.append({origin_socket, value.get()});
      if (multi_value.expected_size == multi_value.items.size()) {
        this->log_socket_value({input_socket}, input_state, multi_value.items);
      }
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      single_value.value = value.get();
      this->log_socket_value({input_socket}, value);
    }
  }

  void destruct_input_value_if_exists(LockedNode &locked_node, const DInputSocket socket)
  {
    InputState &input_state = locked_node.node_state.inputs[socket->index()];
    if (socket->is_multi_input_socket()) {
      MultiInputValue &multi_value = *input_state.value.multi;
      for (MultiInputValueItem &item : multi_value.items) {
        input_state.type->destruct(item.value);
      }
      multi_value.items.clear();
    }
    else {
      SingleInputValue &single_value = *input_state.value.single;
      if (single_value.value != nullptr) {
        input_state.type->destruct(single_value.value);
        single_value.value = nullptr;
      }
    }
  }

  GMutablePointer get_value_from_socket(const DSocket socket, const CPPType &required_type)
  {
    LinearAllocator<> &allocator = local_allocators_.local();

    const CPPType &type = *get_socket_cpp_type(socket);
    void *buffer = allocator.allocate(type.size(), type.alignment());
    get_socket_value(*socket.socket_ref(), buffer);

    if (type == required_type) {
      return {type, buffer};
    }
    void *converted_buffer = allocator.allocate(required_type.size(), required_type.alignment());
    this->convert_value(type, required_type, buffer, converted_buffer);
    return {required_type, converted_buffer};
  }

  void convert_value(const CPPType &from_type,
                     const CPPType &to_type,
                     const void *from_value,
                     void *to_value)
  {
    if (from_type == to_type) {
      from_type.copy_construct(from_value, to_value);
      return;
    }

    const FieldCPPType *from_field_type = dynamic_cast<const FieldCPPType *>(&from_type);
    const FieldCPPType *to_field_type = dynamic_cast<const FieldCPPType *>(&to_type);

    if (from_field_type != nullptr && to_field_type != nullptr) {
      const CPPType &from_base_type = from_field_type->field_type();
      const CPPType &to_base_type = to_field_type->field_type();
      if (conversions_.is_convertible(from_base_type, to_base_type)) {
        const MultiFunction &fn = *conversions_.get_conversion_multi_function(
            MFDataType::ForSingle(from_base_type), MFDataType::ForSingle(to_base_type));
        const GField &from_field = *(const GField *)from_value;
        auto operation = std::make_shared<fn::FieldOperation>(fn, Vector<GField>{from_field});
        new (to_value) GField(std::move(operation), 0);
        return;
      }
    }
    if (conversions_.is_convertible(from_type, to_type)) {
      /* Do the conversion if possible. */
      conversions_.convert_to_uninitialized(from_type, to_type, from_value, to_value);
    }
    else {
      /* Cannot convert, use default value instead. */
      this->construct_default_value(to_type, to_value);
    }
  }

  void construct_default_value(const CPPType &type, void *r_value)
  {
    if (const FieldCPPType *field_cpp_type = dynamic_cast<const FieldCPPType *>(&type)) {
      const CPPType &base_type = field_cpp_type->field_type();
      auto constant_fn = std::make_unique<fn::CustomMF_GenericConstant>(
          base_type, base_type.default_value(), false);
      auto operation = std::make_shared<fn::FieldOperation>(std::move(constant_fn));
      new (r_value) GField(std::move(operation), 0);
      return;
    }
    type.copy_construct(type.default_value(), r_value);
  }

  NodeState &get_node_state(const DNode node)
  {
    return *node_states_.lookup_key_as(node).state;
  }

  void log_socket_value(DSocket socket, InputState &input_state, Span<MultiInputValueItem> values)
  {
    if (params_.geo_logger == nullptr) {
      return;
    }

    Vector<GPointer, 16> value_pointers;
    value_pointers.reserve(values.size());
    const CPPType &type = *input_state.type;
    for (const MultiInputValueItem &item : values) {
      value_pointers.append({type, item.value});
    }
    params_.geo_logger->local().log_multi_value_socket(socket, value_pointers);
  }

  void log_socket_value(Span<DSocket> sockets, GPointer value)
  {
    if (params_.geo_logger == nullptr) {
      return;
    }
    params_.geo_logger->local().log_value_for_sockets(sockets, value);
  }

  /* In most cases when `NodeState` is accessed, the node has to be locked first to avoid race
   * conditions. */
  template<typename Function>
  void with_locked_node(const DNode node, NodeState &node_state, const Function &function)
  {
    LockedNode locked_node{node, node_state};

    node_state.mutex.lock();
    /* Isolate this thread because we don't want it to start executing another node. This other
     * node might want to lock the same mutex leading to a deadlock. */
    threading::isolate_task([&] { function(locked_node); });
    node_state.mutex.unlock();

    /* Then send notifications to the other nodes after the node state is unlocked. This avoids
     * locking two nodes at the same time on this thread and helps to prevent deadlocks. */
    for (const DOutputSocket &socket : locked_node.delayed_required_outputs) {
      this->send_output_required_notification(socket);
    }
    for (const DOutputSocket &socket : locked_node.delayed_unused_outputs) {
      this->send_output_unused_notification(socket);
    }
    for (const DNode &node : locked_node.delayed_scheduled_nodes) {
      this->add_node_to_task_pool(node);
    }
  }
};

NodeParamsProvider::NodeParamsProvider(GeometryNodesEvaluator &evaluator,
                                       DNode dnode,
                                       NodeState &node_state)
    : evaluator_(evaluator), node_state_(node_state)
{
  this->dnode = dnode;
  this->self_object = evaluator.params_.self_object;
  this->modifier = &evaluator.params_.modifier_->modifier;
  this->depsgraph = evaluator.params_.depsgraph;
  this->logger = evaluator.params_.geo_logger;
}

bool NodeParamsProvider::can_get_input(StringRef identifier) const
{
  const DInputSocket socket = this->dnode.input_by_identifier(identifier);
  BLI_assert(socket);

  InputState &input_state = node_state_.inputs[socket->index()];
  if (!input_state.was_ready_for_execution) {
    return false;
  }

  if (socket->is_multi_input_socket()) {
    MultiInputValue &multi_value = *input_state.value.multi;
    return multi_value.items.size() == multi_value.expected_size;
  }
  SingleInputValue &single_value = *input_state.value.single;
  return single_value.value != nullptr;
}

bool NodeParamsProvider::can_set_output(StringRef identifier) const
{
  const DOutputSocket socket = this->dnode.output_by_identifier(identifier);
  BLI_assert(socket);

  OutputState &output_state = node_state_.outputs[socket->index()];
  return !output_state.has_been_computed;
}

GMutablePointer NodeParamsProvider::extract_input(StringRef identifier)
{
  const DInputSocket socket = this->dnode.input_by_identifier(identifier);
  BLI_assert(socket);
  BLI_assert(!socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_.inputs[socket->index()];
  SingleInputValue &single_value = *input_state.value.single;
  void *value = single_value.value;
  single_value.value = nullptr;
  return {*input_state.type, value};
}

Vector<GMutablePointer> NodeParamsProvider::extract_multi_input(StringRef identifier)
{
  const DInputSocket socket = this->dnode.input_by_identifier(identifier);
  BLI_assert(socket);
  BLI_assert(socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_.inputs[socket->index()];
  MultiInputValue &multi_value = *input_state.value.multi;

  Vector<GMutablePointer> ret_values;
  socket.foreach_origin_socket([&](DSocket origin) {
    for (MultiInputValueItem &item : multi_value.items) {
      if (item.origin == origin && item.value != nullptr) {
        ret_values.append({*input_state.type, item.value});
        /* Make sure we do not use the same value again if two values have the same origin. */
        item.value = nullptr;
        return;
      }
    }
    BLI_assert_unreachable();
  });
  if (ret_values.is_empty()) {
    /* If the socket is not linked, we just use the value from the socket itself. */
    BLI_assert(multi_value.items.size() == 1);
    MultiInputValueItem &item = multi_value.items[0];
    BLI_assert(item.origin == socket);
    ret_values.append({*input_state.type, item.value});
  }
  multi_value.items.clear();
  return ret_values;
}

GPointer NodeParamsProvider::get_input(StringRef identifier) const
{
  const DInputSocket socket = this->dnode.input_by_identifier(identifier);
  BLI_assert(socket);
  BLI_assert(!socket->is_multi_input_socket());
  BLI_assert(this->can_get_input(identifier));

  InputState &input_state = node_state_.inputs[socket->index()];
  SingleInputValue &single_value = *input_state.value.single;
  return {*input_state.type, single_value.value};
}

GMutablePointer NodeParamsProvider::alloc_output_value(const CPPType &type)
{
  LinearAllocator<> &allocator = evaluator_.local_allocators_.local();
  return {type, allocator.allocate(type.size(), type.alignment())};
}

void NodeParamsProvider::set_output(StringRef identifier, GMutablePointer value)
{
  const DOutputSocket socket = this->dnode.output_by_identifier(identifier);
  BLI_assert(socket);

  OutputState &output_state = node_state_.outputs[socket->index()];
  BLI_assert(!output_state.has_been_computed);
  evaluator_.forward_output(socket, value);
  output_state.has_been_computed = true;
}

bool NodeParamsProvider::lazy_require_input(StringRef identifier)
{
  BLI_assert(node_supports_laziness(this->dnode));
  const DInputSocket socket = this->dnode.input_by_identifier(identifier);
  BLI_assert(socket);

  InputState &input_state = node_state_.inputs[socket->index()];
  if (input_state.was_ready_for_execution) {
    return false;
  }
  evaluator_.with_locked_node(this->dnode, node_state_, [&](LockedNode &locked_node) {
    evaluator_.set_input_required(locked_node, socket);
  });
  return true;
}

void NodeParamsProvider::set_input_unused(StringRef identifier)
{
  const DInputSocket socket = this->dnode.input_by_identifier(identifier);
  BLI_assert(socket);

  evaluator_.with_locked_node(this->dnode, node_state_, [&](LockedNode &locked_node) {
    evaluator_.set_input_unused(locked_node, socket);
  });
}

bool NodeParamsProvider::output_is_required(StringRef identifier) const
{
  const DOutputSocket socket = this->dnode.output_by_identifier(identifier);
  BLI_assert(socket);

  OutputState &output_state = node_state_.outputs[socket->index()];
  if (output_state.has_been_computed) {
    return false;
  }
  return output_state.output_usage_for_execution != ValueUsage::Unused;
}

bool NodeParamsProvider::lazy_output_is_required(StringRef identifier) const
{
  BLI_assert(node_supports_laziness(this->dnode));
  const DOutputSocket socket = this->dnode.output_by_identifier(identifier);
  BLI_assert(socket);

  OutputState &output_state = node_state_.outputs[socket->index()];
  if (output_state.has_been_computed) {
    return false;
  }
  return output_state.output_usage_for_execution == ValueUsage::Required;
}

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params)
{
  GeometryNodesEvaluator evaluator{params};
  evaluator.execute();
}

}  // namespace blender::modifiers::geometry_nodes
