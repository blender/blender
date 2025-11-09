/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 *
 * Many geometry nodes related UI features need access to data produced during evaluation. Not only
 * is the final output required but also the intermediate results. Those features include attribute
 * search, node warnings, socket inspection and the viewer node.
 *
 * This file provides the system for logging data during evaluation and accessing the data after
 * evaluation. At the root of the logging data is a #GeoNodesLog which is created by the code that
 * invokes Geometry Nodes (e.g. the Geometry Nodes modifier).
 *
 * The system makes a distinction between "loggers" and the "log":
 * - Logger (#GeoTreeLogger): Is used during geometry nodes evaluation. Each thread logs data
 *   independently to avoid communication between threads. Logging should generally be fast.
 *   Generally, the logged data is just dumped into simple containers. Any processing of the data
 *   happens later if necessary. This is important for performance, because in practice, most of
 *   the logged data is never used again. So any processing of the data is likely to be a waste of
 *   resources.
 * - Log (#GeoTreeLog, #GeoNodeLog): Those are used when accessing logged data in UI code. They
 *   contain and cache preprocessed data produced during logging. The log combines data from all
 *   thread-local loggers to provide simple access. Importantly, the (preprocessed) log is only
 *   created when it is actually used by UI code.
 */

#pragma once

#include <chrono>

#include "BLI_cache_mutex.hh"
#include "BLI_compute_context.hh"
#include "BLI_enum_flags.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_linear_allocator_chunked_list.hh"

#include "BKE_compute_context_cache_fwd.hh"
#include "BKE_geometry_set.hh"
#include "BKE_node.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_volume_grid_fwd.hh"

#include "NOD_geometry_nodes_closure_location.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_geometry_nodes_warning.hh"

#include "FN_field.hh"

#include "DNA_node_types.h"

struct SpaceNode;
struct NodesModifierData;
struct Report;

namespace blender::nodes::geo_eval_log {

using fn::GField;

struct NodeWarning {
  NodeWarningType type;
  std::string message;

  NodeWarning(NodeWarningType type, StringRef message) : type(type), message(message) {}
  NodeWarning(const Report &report);

  uint64_t hash() const
  {
    return get_default_hash(this->type, this->message);
  }

  BLI_STRUCT_EQUALITY_OPERATORS_2(NodeWarning, type, message)
};

enum class NamedAttributeUsage {
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,
  Remove = 1 << 2,
};
ENUM_OPERATORS(NamedAttributeUsage);

/**
 * Values of different types are logged differently. This is necessary because some types are so
 * simple that we can log them entirely (e.g. `int`), while we don't want to log all intermediate
 * geometries in their entirety.
 *
 * #ValueLog is a base class for the different ways we log values.
 */
class ValueLog {
 public:
  virtual ~ValueLog() = default;
};

/**
 * Simplest logger. It just stores a copy of the entire value. This is used for most simple types
 * like `int`.
 */
class GenericValueLog : public ValueLog {
 public:
  /**
   * This is owning the value, but not the memory.
   */
  GMutablePointer value;

  GenericValueLog(const GMutablePointer value) : value(value) {}

  ~GenericValueLog() override;
};

/**
 * Fields are not logged entirely, because they might contain arbitrarily large data (e.g.
 * geometries that are sampled). Instead, only the data needed for UI features is logged.
 */
class FieldInfoLog : public ValueLog {
 public:
  const CPPType &type;
  Vector<std::string> input_tooltips;

  FieldInfoLog(const GField &field);
};

struct StringLog : public ValueLog {
  StringRef value;
  bool truncated;
  StringLog(StringRef string, LinearAllocator<> &allocator);
};

struct GeometryAttributeInfo {
  std::string name;
  /** Can be empty when #name does not actually exist on a geometry yet. */
  std::optional<bke::AttrDomain> domain;
  std::optional<bke::AttrType> data_type;
};

struct VolumeGridInfo {
  std::string name;
  VolumeGridType grid_type;
};

/**
 * Geometries are not logged entirely, because that would result in a lot of time and memory
 * overhead. Instead, only the data needed for UI features is logged.
 */
class GeometryInfoLog : public ValueLog {
 public:
  std::string name;
  Vector<GeometryAttributeInfo> attributes;
  Vector<bke::GeometryComponent::Type> component_types;

  struct MeshInfo {
    int verts_num, edges_num, faces_num;
  };
  struct CurveInfo {
    int points_num;
    int splines_num;
  };
  struct PointCloudInfo {
    int points_num;
  };
  struct GreasePencilInfo {
    int layers_num;
    Vector<std::string> layer_names;
  };
  struct InstancesInfo {
    int instances_num;
  };
  struct EditDataInfo {
    bool has_deformed_positions = false;
    bool has_deform_matrices = false;
    int gizmo_transforms_num = 0;
  };
  struct VolumeInfo {
    Vector<VolumeGridInfo> grids;
  };

  std::optional<MeshInfo> mesh_info;
  std::optional<CurveInfo> curve_info;
  std::optional<PointCloudInfo> pointcloud_info;
  std::optional<GreasePencilInfo> grease_pencil_info;
  std::optional<InstancesInfo> instances_info;
  std::optional<EditDataInfo> edit_data_info;
  std::optional<VolumeInfo> volume_info;

  GeometryInfoLog(const bke::GeometrySet &geometry_set);
};

class GridInfoLog : public ValueLog {
 public:
  bool is_empty = false;

  GridInfoLog(const bke::GVolumeGrid &grid);
};

class BundleValueLog : public ValueLog {
 public:
  struct Item {
    std::string key;
    std::variant<const bke::bNodeSocketType *, StringRefNull> type;
  };

  Vector<Item> items;

  BundleValueLog(Vector<Item> items);
};

class ClosureValueLog : public ValueLog {
 public:
  struct Item {
    std::string key;
    const bke::bNodeSocketType *type;
  };

  /**
   * Similar to #ClosureSourceLocation but does not keep pointer references to potentially
   * temporary data.
   */
  struct Source {
    uint32_t orig_node_tree_session_uid;
    int closure_output_node_id;
    ComputeContextHash compute_context_hash;
  };

  Vector<Item> inputs;
  Vector<Item> outputs;
  std::optional<Source> source;
  std::shared_ptr<ClosureEvalLog> eval_log;

  ClosureValueLog(Vector<Item> inputs,
                  Vector<Item> outputs,
                  const std::optional<ClosureSourceLocation> &source_location,
                  std::shared_ptr<ClosureEvalLog> eval_log);
};

class ListInfoLog : public ValueLog {
 public:
  int64_t size;

  ListInfoLog(const List *list);
};

/**
 * Data logged by a viewer node when it is executed.
 */
class ViewerNodeLog {
  mutable CacheMutex main_geometry_cache_mutex_;
  mutable std::optional<bke::GeometrySet> main_geometry_cache_;

 public:
  struct Item {
    int identifier;
    std::string name;
    bke::SocketValueVariant value;
  };

  struct ItemIdentifierGetter {
    int operator()(const Item &item) const
    {
      return item.identifier;
    }
  };

  CustomIDVectorSet<Item, ItemIdentifierGetter> items;

  const bke::GeometrySet *main_geometry() const;
};

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

/**
 * Logs all data for a specific geometry node tree in a specific context. When the same node group
 * is used in multiple times each instantiation will have a separate logger.
 */
class GeoTreeLogger {
 public:
  std::optional<ComputeContextHash> parent_hash;
  std::optional<int32_t> parent_node_id;
  Vector<ComputeContextHash> children_hashes;
  /**
   * The #ID.session_uid of the tree that this logger is for. It's an optional value because under
   * some circumstances it's not possible to know this exactly currently (e.g. for closures).
   */
  std::optional<uint32_t> tree_orig_session_uid;
  /** The time spend in the compute context that this logger corresponds to. */
  std::chrono::nanoseconds execution_time{};

  LinearAllocator<> *allocator = nullptr;

  struct WarningWithNode {
    int32_t node_id;
    NodeWarning warning;
  };
  struct SocketValueLog {
    int32_t node_id;
    int socket_index;
    destruct_ptr<ValueLog> value;
  };
  struct NodeExecutionTime {
    int32_t node_id;
    TimePoint start;
    TimePoint end;
  };
  struct ViewerNodeLogWithNode {
    int32_t node_id;
    destruct_ptr<ViewerNodeLog> viewer_log;
  };
  struct AttributeUsageWithNode {
    int32_t node_id;
    StringRefNull attribute_name;
    NamedAttributeUsage usage;
  };
  struct DebugMessage {
    int32_t node_id;
    StringRefNull message;
  };
  struct EvaluatedGizmoNode {
    int32_t node_id;
  };

  linear_allocator::ChunkedList<WarningWithNode> node_warnings;
  linear_allocator::ChunkedList<SocketValueLog, 16> input_socket_values;
  linear_allocator::ChunkedList<SocketValueLog, 16> output_socket_values;
  linear_allocator::ChunkedList<NodeExecutionTime, 16> node_execution_times;
  linear_allocator::ChunkedList<ViewerNodeLogWithNode> viewer_node_logs;
  linear_allocator::ChunkedList<AttributeUsageWithNode> used_named_attributes;
  linear_allocator::ChunkedList<DebugMessage> debug_messages;
  /** Keeps track of which gizmo nodes have been tracked by this evaluation. */
  linear_allocator::ChunkedList<EvaluatedGizmoNode> evaluated_gizmo_nodes;

  GeoTreeLogger();
  ~GeoTreeLogger();

  void log_value(const bNode &node, const bNodeSocket &socket, GPointer value);
};

/**
 * Contains data that has been logged for a specific node in a context. So when the node is in a
 * node group that is used multiple times, there will be a different #GeoNodeLog for every
 * instance.
 *
 * By default, not all of the info below is valid. A #GeoTreeLog::ensure_* method has to be called
 * first.
 */
class GeoNodeLog {
 public:
  /** Warnings generated for that node. */
  VectorSet<NodeWarning> warnings;
  /** Time spent in this node. */
  std::chrono::nanoseconds execution_time{0};
  /** Maps from socket indices to their values. */
  Map<int, ValueLog *> input_values_;
  Map<int, ValueLog *> output_values_;
  /** Maps from attribute name to their usage flags. */
  Map<StringRefNull, NamedAttributeUsage> used_named_attributes;
  /** Messages that are used for debugging purposes during development. */
  Vector<StringRefNull> debug_messages;

  GeoNodeLog();
  ~GeoNodeLog();
};

class GeoNodesLog;

/**
 * Contains data that has been logged for a specific node group in a context. If the same node
 * group is used multiple times, there will be a different #GeoTreeLog for every instance.
 *
 * This contains lazily evaluated data. Call the corresponding `ensure_*` methods before accessing
 * data.
 */
class GeoTreeLog {
 private:
  GeoNodesLog *root_log_;
  Vector<GeoTreeLogger *> tree_loggers_;
  VectorSet<ComputeContextHash> children_hashes_;
  bool reduced_node_warnings_ = false;
  bool reduced_execution_times_ = false;
  bool reduced_socket_values_ = false;
  bool reduced_viewer_node_logs_ = false;
  bool reduced_existing_attributes_ = false;
  bool reduced_used_named_attributes_ = false;
  bool reduced_debug_messages_ = false;
  bool reduced_evaluated_gizmo_nodes_ = false;
  bool reduced_layer_names_ = false;

 public:
  Map<int32_t, GeoNodeLog> nodes;
  Map<int32_t, ViewerNodeLog *, 0> viewer_node_logs;
  VectorSet<NodeWarning> all_warnings;
  std::chrono::nanoseconds execution_time{0};
  Vector<const GeometryAttributeInfo *> existing_attributes;
  Map<StringRefNull, NamedAttributeUsage> used_named_attributes;
  Set<int> evaluated_gizmo_nodes;
  Vector<std::string> all_layer_names;

  GeoTreeLog(GeoNodesLog *root_log, Vector<GeoTreeLogger *> tree_loggers);
  ~GeoTreeLog();

  /**
   * Propagate node warnings. This needs access to the node group pointers, because propagation
   * settings are stored on the nodes. However, the log can only store weak pointers (in the form
   * of e.g. session ids) to original data to avoid dangling pointers.
   */
  void ensure_node_warnings(const NodesModifierData &nmd);
  void ensure_node_warnings(const Main &bmain);
  void ensure_node_warnings(const Map<uint32_t, const bNodeTree *> &orig_tree_by_session_uid);

  void ensure_execution_times();
  void ensure_socket_values();
  void ensure_viewer_node_logs();
  void ensure_existing_attributes();
  void ensure_used_named_attributes();
  void ensure_debug_messages();
  void ensure_evaluated_gizmo_nodes();
  void ensure_layer_names();

  ValueLog *find_socket_value_log(const bNodeSocket &query_socket);
  [[nodiscard]] bool try_convert_primitive_socket_value(const GenericValueLog &value_log,
                                                        const CPPType &dst_type,
                                                        void *dst);

  template<typename T>
  std::optional<T> find_primitive_socket_value(const bNodeSocket &query_socket)
  {
    if (auto *value_log = dynamic_cast<GenericValueLog *>(
            this->find_socket_value_log(query_socket)))
    {
      T value;
      if (this->try_convert_primitive_socket_value(*value_log, CPPType::get<T>(), &value)) {
        return value;
      }
    }
    return std::nullopt;
  }
};

class ContextualGeoTreeLogs {
 private:
  Map<const bke::bNodeTreeZone *, GeoTreeLog *> tree_logs_by_zone_;

 public:
  ContextualGeoTreeLogs(Map<const bke::bNodeTreeZone *, GeoTreeLog *> tree_logs_by_zone = {});

  /**
   * Get a tree log for the given zone/node/socket if available.
   */
  GeoTreeLog *get_main_tree_log(const bke::bNodeTreeZone *zone) const;
  GeoTreeLog *get_main_tree_log(const bNode &node) const;
  GeoTreeLog *get_main_tree_log(const bNodeSocket &socket) const;

  /**
   * Runs a callback for each tree log that may be returned above.
   */
  void foreach_tree_log(FunctionRef<void(GeoTreeLog &)> callback) const;
};

/**
 * There is one #GeoNodesLog for every modifier that evaluates geometry nodes. It contains all
 * the loggers that are used during evaluation as well as the preprocessed logs that are used by UI
 * code.
 */
class GeoNodesLog {
 private:
  /** Data that is stored for each thread. */
  struct LocalData {
    /** Each thread has its own allocator. */
    LinearAllocator<> allocator;
    /**
     * Store a separate #GeoTreeLogger for each instance of the corresponding node group (e.g.
     * when the same node group is used multiple times).
     */
    Map<ComputeContextHash, destruct_ptr<GeoTreeLogger>> tree_logger_by_context;
  };

  /** Container for all thread-local data. */
  threading::EnumerableThreadSpecific<LocalData> data_per_thread_;
  /**
   * A #GeoTreeLog for every compute context. Those are created lazily when requested by UI code.
   */
  Map<ComputeContextHash, std::unique_ptr<GeoTreeLog>> tree_logs_;

 public:
  GeoNodesLog();
  ~GeoNodesLog();

  /**
   * Get a thread-local logger for the current node tree.
   */
  GeoTreeLogger &get_local_tree_logger(const ComputeContext &compute_context);

  /**
   * Get a log a specific node tree instance.
   */
  GeoTreeLog &get_tree_log(const ComputeContextHash &compute_context_hash);

  /**
   * Utility accessor to logged data.
   */
  static Map<const bke::bNodeTreeZone *, ComputeContextHash>
  get_context_hash_by_zone_for_node_editor(const SpaceNode &snode,
                                           bke::ComputeContextCache &compute_context_cache);

  static ContextualGeoTreeLogs get_contextual_tree_logs(const SpaceNode &snode);
  static const ViewerNodeLog *find_viewer_node_log_for_path(const ViewerPath &viewer_path);
};

}  // namespace blender::nodes::geo_eval_log
