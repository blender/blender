/* SPDX-FileCopyrightText: 2023 Blender Foundation
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
 * evaluation. Geometry nodes is executed by a modifier, therefore the "root" of logging is
 * #GeoModifierLog which will contain all data generated in a modifier.
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

#include "BLI_compute_context.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_multi_value_map.hh"

#include "BKE_attribute.h"
#include "BKE_geometry_set.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_viewer_path.h"

#include "FN_field.hh"

#include "DNA_node_types.h"

struct SpaceNode;

namespace blender::nodes::geo_eval_log {

using fn::GField;

enum class NodeWarningType {
  Error,
  Warning,
  Info,
};

struct NodeWarning {
  NodeWarningType type;
  std::string message;
};

enum class NamedAttributeUsage {
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,
  Remove = 1 << 2,
};
ENUM_OPERATORS(NamedAttributeUsage, NamedAttributeUsage::Remove);

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

  ~GenericValueLog();
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

struct GeometryAttributeInfo {
  std::string name;
  /** Can be empty when #name does not actually exist on a geometry yet. */
  std::optional<eAttrDomain> domain;
  std::optional<eCustomDataType> data_type;
};

/**
 * Geometries are not logged entirely, because that would result in a lot of time and memory
 * overhead. Instead, only the data needed for UI features is logged.
 */
class GeometryInfoLog : public ValueLog {
 public:
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
  struct InstancesInfo {
    int instances_num;
  };
  struct EditDataInfo {
    bool has_deformed_positions;
    bool has_deform_matrices;
  };

  std::optional<MeshInfo> mesh_info;
  std::optional<CurveInfo> curve_info;
  std::optional<PointCloudInfo> pointcloud_info;
  std::optional<InstancesInfo> instances_info;
  std::optional<EditDataInfo> edit_data_info;

  GeometryInfoLog(const bke::GeometrySet &geometry_set);
};

/**
 * Data logged by a viewer node when it is executed. In this case, we do want to log the entire
 * geometry.
 */
class ViewerNodeLog {
 public:
  bke::GeometrySet geometry;
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
  std::optional<int32_t> group_node_id;
  Vector<ComputeContextHash> children_hashes;

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

  Vector<WarningWithNode> node_warnings;
  Vector<SocketValueLog> input_socket_values;
  Vector<SocketValueLog> output_socket_values;
  Vector<NodeExecutionTime> node_execution_times;
  Vector<ViewerNodeLogWithNode, 0> viewer_node_logs;
  Vector<AttributeUsageWithNode, 0> used_named_attributes;
  Vector<DebugMessage, 0> debug_messages;

  GeoTreeLogger();
  ~GeoTreeLogger();

  void log_value(const bNode &node, const bNodeSocket &socket, GPointer value);
  void log_viewer_node(const bNode &viewer_node, bke::GeometrySet geometry);
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
  Vector<NodeWarning> warnings;
  /**
   * Time spent in this node. For node groups this is the sum of the run times of the nodes
   * inside.
   */
  std::chrono::nanoseconds run_time{0};
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

class GeoModifierLog;

/**
 * Contains data that has been logged for a specific node group in a context. If the same node
 * group is used multiple times, there will be a different #GeoTreeLog for every instance.
 *
 * This contains lazily evaluated data. Call the corresponding `ensure_*` methods before accessing
 * data.
 */
class GeoTreeLog {
 private:
  GeoModifierLog *modifier_log_;
  Vector<GeoTreeLogger *> tree_loggers_;
  VectorSet<ComputeContextHash> children_hashes_;
  bool reduced_node_warnings_ = false;
  bool reduced_node_run_times_ = false;
  bool reduced_socket_values_ = false;
  bool reduced_viewer_node_logs_ = false;
  bool reduced_existing_attributes_ = false;
  bool reduced_used_named_attributes_ = false;
  bool reduced_debug_messages_ = false;

 public:
  Map<int32_t, GeoNodeLog> nodes;
  Map<int32_t, ViewerNodeLog *, 0> viewer_node_logs;
  Vector<NodeWarning> all_warnings;
  std::chrono::nanoseconds run_time_sum{0};
  Vector<const GeometryAttributeInfo *> existing_attributes;
  Map<StringRefNull, NamedAttributeUsage> used_named_attributes;

  GeoTreeLog(GeoModifierLog *modifier_log, Vector<GeoTreeLogger *> tree_loggers);
  ~GeoTreeLog();

  void ensure_node_warnings();
  void ensure_node_run_time();
  void ensure_socket_values();
  void ensure_viewer_node_logs();
  void ensure_existing_attributes();
  void ensure_used_named_attributes();
  void ensure_debug_messages();

  ValueLog *find_socket_value_log(const bNodeSocket &query_socket);
};

/**
 * There is one #GeoModifierLog for every modifier that evaluates geometry nodes. It contains all
 * the loggers that are used during evaluation as well as the preprocessed logs that are used by UI
 * code.
 */
class GeoModifierLog {
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
  GeoModifierLog();
  ~GeoModifierLog();

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
  get_context_hash_by_zone_for_node_editor(const SpaceNode &snode, StringRefNull modifier_name);

  static Map<const bke::bNodeTreeZone *, GeoTreeLog *> get_tree_log_by_zone_for_node_editor(
      const SpaceNode &snode);
  static const ViewerNodeLog *find_viewer_node_log_for_path(const ViewerPath &viewer_path);
};

}  // namespace blender::nodes::geo_eval_log
