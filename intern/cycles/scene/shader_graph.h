/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "graph/node.h"
#include "graph/node_type.h"

#include "kernel/types.h"

#include "util/map.h"
#include "util/param.h"
#include "util/set.h"
#include "util/string.h"
#include "util/types.h"
#include "util/unique_ptr_vector.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class AttributeRequestSet;
class Scene;
class Shader;
class ShaderInput;
class ShaderOutput;
class ShaderNode;
class ShaderGraph;
class SVMCompiler;
class OSLCompiler;
class OutputNode;
class ConstantFolder;
class MD5Hash;

/* Bump
 *
 * For bump mapping, a node may be evaluated multiple times, using different
 * samples to reconstruct the normal, this indicates the sample position */

enum ShaderBump { SHADER_BUMP_NONE, SHADER_BUMP_CENTER, SHADER_BUMP_DX, SHADER_BUMP_DY };

/* Identifiers for some special node types.
 *
 * The graph needs to identify these in the clean function.
 * Cannot use dynamic_cast, as this is disabled for OSL. */

enum ShaderNodeSpecialType {
  SHADER_SPECIAL_TYPE_NONE,
  SHADER_SPECIAL_TYPE_PROXY,
  SHADER_SPECIAL_TYPE_AUTOCONVERT,
  SHADER_SPECIAL_TYPE_GEOMETRY,
  SHADER_SPECIAL_TYPE_OSL,
  SHADER_SPECIAL_TYPE_IMAGE_SLOT,
  SHADER_SPECIAL_TYPE_CLOSURE,
  SHADER_SPECIAL_TYPE_COMBINE_CLOSURE,
  SHADER_SPECIAL_TYPE_OUTPUT,
  SHADER_SPECIAL_TYPE_BUMP,
  SHADER_SPECIAL_TYPE_OUTPUT_AOV,
  SHADER_SPECIAL_TYPE_LIGHT_PATH,
};

/* Input
 *
 * Input socket for a shader node. May be linked to an output or not. If not
 * linked, it will either get a fixed default value, or e.g. a texture
 * coordinate. */

class ShaderInput {
 public:
  ShaderInput(const SocketType &socket_type_, ShaderNode *parent_)
      : socket_type(socket_type_), parent(parent_)

  {
  }

  ustring name() const
  {
    return socket_type.ui_name;
  }
  int flags() const
  {
    return socket_type.flags;
  }
  SocketType::Type type() const
  {
    return socket_type.type;
  }

  void set(const float f)
  {
    ((Node *)parent)->set(socket_type, f);
  }
  void set(const float3 f)
  {
    ((Node *)parent)->set(socket_type, f);
  }
  void set(const int f)
  {
    ((Node *)parent)->set(socket_type, f);
  }

  void disconnect();

  const SocketType &socket_type;
  ShaderNode *parent;
  ShaderOutput *link = nullptr;
  int stack_offset = SVM_STACK_INVALID; /* for SVM compiler */

  /* Keeps track of whether a constant was folded in this socket, to avoid over-optimizing when the
   * link is null. */
  bool constant_folded_in = false;
};

/* Output
 *
 * Output socket for a shader node. */

class ShaderOutput {
 public:
  ShaderOutput(const SocketType &socket_type_, ShaderNode *parent_)
      : socket_type(socket_type_), parent(parent_)
  {
  }

  ustring name() const
  {
    return socket_type.ui_name;
  }
  SocketType::Type type() const
  {
    return socket_type.type;
  }

  void disconnect();

  const SocketType &socket_type;
  ShaderNode *parent;
  vector<ShaderInput *> links;
  int stack_offset = SVM_STACK_INVALID; /* for SVM compiler */
};

/* Node
 *
 * Shader node in graph, with input and output sockets. This is the virtual
 * base class for all node types. */

class ShaderNode : public Node {
 public:
  explicit ShaderNode(const NodeType *type);
  ShaderNode(const ShaderNode &other);

  void create_inputs_outputs(const NodeType *type);
  void remove_input(ShaderInput *input);

  ShaderInput *input(const char *name);
  ShaderOutput *output(const char *name);
  ShaderInput *input(ustring name);
  ShaderOutput *output(ustring name);

  virtual ShaderNode *clone(ShaderGraph *graph) const = 0;
  virtual void attributes(Shader *shader, AttributeRequestSet *attributes);
  virtual void compile(SVMCompiler &compiler) = 0;
  virtual void compile(OSLCompiler &compiler) = 0;

  /* Expand node into additional nodes. */
  virtual void expand(ShaderGraph * /* graph */) {}

  /* ** Node optimization ** */
  /* Check whether the node can be replaced with single constant. */
  virtual void constant_fold(const ConstantFolder & /*folder*/) {}

  /* Simplify settings used by artists to the ones which are simpler to
   * evaluate in the kernel but keep the final result unchanged.
   */
  virtual void simplify_settings(Scene * /*scene*/) {};

  virtual bool has_surface_emission()
  {
    return false;
  }
  virtual bool has_surface_transparent()
  {
    return false;
  }
  virtual bool has_surface_bssrdf()
  {
    return false;
  }
  virtual bool has_bump()
  {
    return false;
  }
  virtual bool has_bssrdf_bump()
  {
    return false;
  }
  virtual bool has_spatial_varying()
  {
    return false;
  }
  virtual bool has_attribute_dependency()
  {
    return false;
  }
  virtual bool has_volume_support()
  {
    return false;
  }
  /* True if the node only multiplies or adds a constant values. */
  virtual bool is_linear_operation()
  {
    return false;
  }

  unique_ptr_vector<ShaderInput> inputs;
  unique_ptr_vector<ShaderOutput> outputs;

  /* index in graph node array */
  int id = -1;
  /* for bump mapping utility */
  ShaderBump bump = SHADER_BUMP_NONE;
  float bump_filter_width = 0.0f;
  /* special node type */
  ShaderNodeSpecialType special_type = SHADER_SPECIAL_TYPE_NONE;

  /* ** Selective nodes compilation ** */

  /* TODO(sergey): More explicitly mention in the function names
   * that those functions are for selective compilation only?
   */

  /* Node feature are used to disable huge nodes inside the group,
   * so it's possible to disable huge nodes inside of the required
   * nodes group.
   */
  virtual uint get_feature()
  {
    return bump == SHADER_BUMP_NONE ? 0 : KERNEL_FEATURE_NODE_BUMP;
  }

  /* Get closure ID to which the node compiles into. */
  virtual ClosureType get_closure_type()
  {
    return CLOSURE_NONE_ID;
  }

  /* Check whether settings of the node equals to another one.
   *
   * This is mainly used to check whether two nodes can be merged
   * together. Meaning, runtime stuff like node id and unbound slots
   * will be ignored for comparison.
   *
   * NOTE: If some node can't be de-duplicated for whatever reason it
   * is to be handled in the subclass.
   */
  virtual bool equals(const ShaderNode &other);

 protected:
  /* Disconnect the input with the given name if it is connected.
   * Used to optimize away unused inputs. */
  void disconnect_unused_input(const char *name);
};

/* Node definition utility macros */

#define SHADER_NODE_CLASS(type) \
  NODE_DECLARE \
  type(); \
  ShaderNode *clone(ShaderGraph *graph) const override \
  { \
    return graph->create_node<type>(*this); \
  } \
  void compile(SVMCompiler &compiler) override; \
  void compile(OSLCompiler &compiler) override;

#define SHADER_NODE_NO_CLONE_CLASS(type) \
  NODE_DECLARE \
  type(); \
  void compile(SVMCompiler &compiler) override; \
  void compile(OSLCompiler &compiler) override;

#define SHADER_NODE_BASE_CLASS(type) \
  ShaderNode *clone(ShaderGraph *graph) const override \
  { \
    return graph->create_node<type>(*this); \
  } \
  void compile(SVMCompiler &compiler) override; \
  void compile(OSLCompiler &compiler) override;

class ShaderNodeIDComparator {
 public:
  bool operator()(const ShaderNode *n1, const ShaderNode *n2) const
  {
    return n1->id < n2->id;
  }
};

class ShaderNodeIDAndBoolComparator {
 public:
  bool operator()(const std::pair<ShaderNode *, bool> p1,
                  const std::pair<ShaderNode *, bool> p2) const
  {
    return p1.first->id < p2.first->id || (p1.first->id == p2.first->id && p1.second < p2.second);
  }
};

using ShaderNodeSet = set<ShaderNode *, ShaderNodeIDComparator>;
using ShaderNodeMap = map<ShaderNode *, ShaderNode *, ShaderNodeIDComparator>;

/* Graph
 *
 * Shader graph of nodes. Also does graph manipulations for default inputs,
 * bump mapping from displacement, and possibly other things in the future. */

class ShaderGraph : public NodeOwner {
 public:
  unique_ptr_vector<ShaderNode> nodes;
  size_t num_node_ids;
  bool finalized;
  bool simplified;
  string displacement_hash;

  ShaderGraph();
  ~ShaderGraph() override;

  OutputNode *output();

  void connect(ShaderOutput *from, ShaderInput *to);
  void disconnect(ShaderOutput *from);
  void disconnect(ShaderInput *to);
  void relink(ShaderInput *from, ShaderInput *to);
  void relink(ShaderOutput *from, ShaderOutput *to);
  void relink(ShaderNode *node, ShaderOutput *from, ShaderOutput *to);

  void remove_proxy_nodes();
  void compute_displacement_hash();
  void simplify(Scene *scene);
  void finalize(Scene *scene, bool do_bump = false, bool bump_in_object_space = false);

  int get_num_closures();

  void dump_graph(const char *filename);

  /* Create node from class and add it to the shader graph. */
  template<typename T, typename... Args> T *create_node(Args &&...args)
  {
    unique_ptr<T> node = make_unique<T>(args...);
    T *node_ptr = node.get();
    this->add_node(std::move(node));
    return node_ptr;
  }

  /* Create OSL node from class and add it to the shader graph. */
  template<typename T, typename... Args> T *create_osl_node(void *node_memory, Args &&...args)
  {
    T *node_ptr = new (node_memory) T(args...);
    unique_ptr<T> node(node_ptr);
    this->add_node(std::move(node));
    return node_ptr;
  }

  /* Create node from node type and add it to the shader graph. */
  ShaderNode *create_node(const NodeType *node_type)
  {
    unique_ptr<Node> node = node_type->create(node_type);
    unique_ptr<ShaderNode> shader_node(static_cast<ShaderNode *>(node.release()));
    ShaderNode *shader_node_ptr = shader_node.get();
    this->add_node(std::move(shader_node));
    return shader_node_ptr;
  }

 protected:
  using NodePair = pair<ShaderNode *const, ShaderNode *>;

  void add_node(unique_ptr<ShaderNode> &&node);

  void find_dependencies(ShaderNodeSet &dependencies, ShaderInput *input);
  void clear_nodes();
  void copy_nodes(ShaderNodeSet &nodes, ShaderNodeMap &nnodemap);

  void break_cycles(ShaderNode *node, vector<bool> &visited, vector<bool> &on_stack);
  void bump_from_displacement(bool use_object_space);
  void refine_bump_nodes();
  void expand();
  void default_inputs(bool do_osl);
  void transform_multi_closure(ShaderNode *node, ShaderOutput *weight_out, bool volume);

  /* Graph simplification routines. */
  void clean(Scene *scene);
  void constant_fold(Scene *scene);
  void simplify_settings(Scene *scene);
  void deduplicate_nodes();
  void optimize_volume_output();
};

CCL_NAMESPACE_END
