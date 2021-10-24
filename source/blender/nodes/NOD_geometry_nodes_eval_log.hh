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

#pragma once

/**
 * Many geometry nodes related UI features need access to data produced during evaluation. Not only
 * is the final output required but also the intermediate results. Those features include
 * attribute search, node warnings, socket inspection and the viewer node.
 *
 * This file provides the framework for logging data during evaluation and accessing the data after
 * evaluation.
 *
 * During logging every thread gets its own local logger to avoid too much locking (logging
 * generally happens for every socket). After geometry nodes evaluation is done, the thread-local
 * logging information is combined and post-processed to make it easier for the UI to lookup.
 * necessary information.
 */

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_function_ref.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"

#include "BKE_geometry_set.hh"

#include "FN_generic_pointer.hh"

#include "NOD_derived_node_tree.hh"

struct SpaceNode;
struct SpaceSpreadsheet;

namespace blender::nodes::geometry_nodes_eval_log {

using fn::GMutablePointer;
using fn::GPointer;

/** Contains information about a value that has been computed during geometry nodes evaluation. */
class ValueLog {
 public:
  virtual ~ValueLog() = default;
};

/** Contains an owned copy of a value of a generic type. */
class GenericValueLog : public ValueLog {
 private:
  GMutablePointer data_;

 public:
  GenericValueLog(GMutablePointer data) : data_(data)
  {
  }

  ~GenericValueLog()
  {
    data_.destruct();
  }

  GPointer value() const
  {
    return data_;
  }
};

struct GeometryAttributeInfo {
  std::string name;
  AttributeDomain domain;
  CustomDataType data_type;
};

/** Contains information about a geometry set. In most cases this does not store the entire
 * geometry set as this would require too much memory. */
class GeometryValueLog : public ValueLog {
 private:
  Vector<GeometryAttributeInfo> attributes_;
  Vector<GeometryComponentType> component_types_;
  std::unique_ptr<GeometrySet> full_geometry_;

 public:
  struct MeshInfo {
    int tot_verts, tot_edges, tot_faces;
  };
  struct CurveInfo {
    int tot_splines;
  };
  struct PointCloudInfo {
    int tot_points;
  };
  struct InstancesInfo {
    int tot_instances;
  };

  std::optional<MeshInfo> mesh_info;
  std::optional<CurveInfo> curve_info;
  std::optional<PointCloudInfo> pointcloud_info;
  std::optional<InstancesInfo> instances_info;

  GeometryValueLog(const GeometrySet &geometry_set, bool log_full_geometry = false);

  Span<GeometryAttributeInfo> attributes() const
  {
    return attributes_;
  }

  Span<GeometryComponentType> component_types() const
  {
    return component_types_;
  }

  const GeometrySet *full_geometry() const
  {
    return full_geometry_.get();
  }
};

enum class NodeWarningType {
  Error,
  Warning,
  Info,
  Legacy,
};

struct NodeWarning {
  NodeWarningType type;
  std::string message;
};

struct NodeWithWarning {
  DNode node;
  NodeWarning warning;
};

/** The same value can be referenced by multiple sockets when they are linked. */
struct ValueOfSockets {
  Span<DSocket> sockets;
  destruct_ptr<ValueLog> value;
};

class GeoLogger;
class ModifierLog;

/** Every thread has its own local logger to avoid having to communicate between threads during
 * evaluation. After evaluation the individual logs are combined. */
class LocalGeoLogger {
 private:
  /* Back pointer to the owner of this local logger. */
  GeoLogger *main_logger_;
  /* Allocator for the many small allocations during logging. This is in a `unique_ptr` so that
   * ownership can be transferred later on. */
  std::unique_ptr<LinearAllocator<>> allocator_;
  Vector<ValueOfSockets> values_;
  Vector<NodeWithWarning> node_warnings_;

  friend ModifierLog;

 public:
  LocalGeoLogger(GeoLogger &main_logger) : main_logger_(&main_logger)
  {
    this->allocator_ = std::make_unique<LinearAllocator<>>();
  }

  void log_value_for_sockets(Span<DSocket> sockets, GPointer value);
  void log_multi_value_socket(DSocket socket, Span<GPointer> values);
  void log_node_warning(DNode node, NodeWarningType type, std::string message);
};

/** The root logger class. */
class GeoLogger {
 private:
  /**
   * Log the entire value for these sockets, because they may be inspected afterwards.
   * We don't log everything, because that would take up too much memory and cause significant
   * slowdowns.
   */
  Set<DSocket> log_full_sockets_;
  threading::EnumerableThreadSpecific<LocalGeoLogger> threadlocals_;

  /* These are only optional since they don't have a default constructor. */
  std::unique_ptr<GeometryValueLog> input_geometry_log_;
  std::unique_ptr<GeometryValueLog> output_geometry_log_;

  friend LocalGeoLogger;
  friend ModifierLog;

 public:
  GeoLogger(Set<DSocket> log_full_sockets)
      : log_full_sockets_(std::move(log_full_sockets)),
        threadlocals_([this]() { return LocalGeoLogger(*this); })
  {
  }

  void log_input_geometry(const GeometrySet &geometry)
  {
    input_geometry_log_ = std::make_unique<GeometryValueLog>(geometry);
  }

  void log_output_geometry(const GeometrySet &geometry)
  {
    output_geometry_log_ = std::make_unique<GeometryValueLog>(geometry);
  }

  LocalGeoLogger &local()
  {
    return threadlocals_.local();
  }

  auto begin()
  {
    return threadlocals_.begin();
  }

  auto end()
  {
    return threadlocals_.end();
  }
};

/** Contains information that has been logged for one specific socket. */
class SocketLog {
 private:
  ValueLog *value_ = nullptr;

  friend ModifierLog;

 public:
  const ValueLog *value() const
  {
    return value_;
  }
};

/** Contains information that has been logged for one specific node. */
class NodeLog {
 private:
  Vector<SocketLog> input_logs_;
  Vector<SocketLog> output_logs_;
  Vector<NodeWarning, 0> warnings_;

  friend ModifierLog;

 public:
  const SocketLog *lookup_socket_log(eNodeSocketInOut in_out, int index) const;
  const SocketLog *lookup_socket_log(const bNode &node, const bNodeSocket &socket) const;

  Span<SocketLog> input_logs() const
  {
    return input_logs_;
  }

  Span<SocketLog> output_logs() const
  {
    return output_logs_;
  }

  Span<NodeWarning> warnings() const
  {
    return warnings_;
  }

  Vector<const GeometryAttributeInfo *> lookup_available_attributes() const;
};

/** Contains information that has been logged for one specific tree. */
class TreeLog {
 private:
  Map<std::string, destruct_ptr<NodeLog>> node_logs_;
  Map<std::string, destruct_ptr<TreeLog>> child_logs_;

  friend ModifierLog;

 public:
  const NodeLog *lookup_node_log(StringRef node_name) const;
  const NodeLog *lookup_node_log(const bNode &node) const;
  const TreeLog *lookup_child_log(StringRef node_name) const;
  void foreach_node_log(FunctionRef<void(const NodeLog &)> fn) const;
};

/** Contains information about an entire geometry nodes evaluation. */
class ModifierLog {
 private:
  LinearAllocator<> allocator_;
  /* Allocators of the individual loggers. */
  Vector<std::unique_ptr<LinearAllocator<>>> logger_allocators_;
  destruct_ptr<TreeLog> root_tree_logs_;
  Vector<destruct_ptr<ValueLog>> logged_values_;

  std::unique_ptr<GeometryValueLog> input_geometry_log_;
  std::unique_ptr<GeometryValueLog> output_geometry_log_;

 public:
  ModifierLog(GeoLogger &logger);

  const TreeLog &root_tree() const
  {
    return *root_tree_logs_;
  }

  /* Utilities to find logged information for a specific context. */
  static const ModifierLog *find_root_by_node_editor_context(const SpaceNode &snode);
  static const TreeLog *find_tree_by_node_editor_context(const SpaceNode &snode);
  static const NodeLog *find_node_by_node_editor_context(const SpaceNode &snode,
                                                         const bNode &node);
  static const SocketLog *find_socket_by_node_editor_context(const SpaceNode &snode,
                                                             const bNode &node,
                                                             const bNodeSocket &socket);
  static const NodeLog *find_node_by_spreadsheet_editor_context(
      const SpaceSpreadsheet &sspreadsheet);
  void foreach_node_log(FunctionRef<void(const NodeLog &)> fn) const;

  const GeometryValueLog *input_geometry_log() const;
  const GeometryValueLog *output_geometry_log() const;

 private:
  using LogByTreeContext = Map<const DTreeContext *, TreeLog *>;

  TreeLog &lookup_or_add_tree_log(LogByTreeContext &log_by_tree_context,
                                  const DTreeContext &tree_context);
  NodeLog &lookup_or_add_node_log(LogByTreeContext &log_by_tree_context, DNode node);
  SocketLog &lookup_or_add_socket_log(LogByTreeContext &log_by_tree_context, DSocket socket);
};

}  // namespace blender::nodes::geometry_nodes_eval_log
