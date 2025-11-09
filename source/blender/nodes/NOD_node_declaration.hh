/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

#include "BLI_array.hh"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh" /* IWYU pragma: export */

#include "DNA_node_types.h"

#include "RNA_types.hh"

#include "NOD_socket_usage_inference_fwd.hh"

struct bContext;
struct bNode;
struct uiLayout;

namespace blender::nodes {

class NodeDeclarationBuilder;
class PanelDeclaration;

enum class InputSocketFieldType : int8_t {
  /** The input is required to be a single value. */
  None,
  /** The input can be a field. */
  IsSupported,
  /** The input can be a field and is a field implicitly if nothing is connected. */
  Implicit,
};

enum class OutputSocketFieldType : int8_t {
  /** The output is always a single value. */
  None,
  /** The output is always a field, independent of the inputs. */
  FieldSource,
  /** If any input is a field, this output will be a field as well. */
  DependentField,
  /** If any of a subset of inputs is a field, this out will be a field as well.
   * The subset is defined by the vector of indices. */
  PartiallyDependent,
};

/**
 * An enum that maps to the #compositor::InputRealizationMode.
 */
enum class CompositorInputRealizationMode : int8_t {
  None,
  Transforms,
  OperationDomain,
};

/**
 * Contains information about how a node output's field state depends on inputs of the same node.
 */
class OutputFieldDependency {
 private:
  OutputSocketFieldType type_ = OutputSocketFieldType::None;
  Vector<int> linked_input_indices_;

 public:
  static OutputFieldDependency ForFieldSource();
  static OutputFieldDependency ForDataSource();
  static OutputFieldDependency ForDependentField();
  static OutputFieldDependency ForPartiallyDependentField(Vector<int> indices);

  OutputSocketFieldType field_type() const;
  Span<int> linked_input_indices() const;

  BLI_STRUCT_EQUALITY_OPERATORS_2(OutputFieldDependency, type_, linked_input_indices_)
};

/**
 * Information about how a node interacts with fields.
 */
struct FieldInferencingInterface {
  Array<InputSocketFieldType> inputs;
  Array<OutputFieldDependency> outputs;

  BLI_STRUCT_EQUALITY_OPERATORS_2(FieldInferencingInterface, inputs, outputs)
};

struct StructureTypeInterface {
  struct OutputDependency {
    StructureType type;
    Array<int> linked_inputs;

    BLI_STRUCT_EQUALITY_OPERATORS_2(OutputDependency, type, linked_inputs)
  };

  Array<StructureType> inputs;
  Array<OutputDependency> outputs;

  BLI_STRUCT_EQUALITY_OPERATORS_2(StructureTypeInterface, inputs, outputs)
};

namespace anonymous_attribute_lifetime {

/**
 * Attributes can be propagated from an input geometry to an output geometry.
 */
struct PropagateRelation {
  int from_geometry_input;
  int to_geometry_output;

  BLI_STRUCT_EQUALITY_OPERATORS_2(PropagateRelation, from_geometry_input, to_geometry_output)
};

/**
 * References to attributes can be propagated from an input field to an output field.
 */
struct ReferenceRelation {
  int from_field_input;
  int to_field_output;

  BLI_STRUCT_EQUALITY_OPERATORS_2(ReferenceRelation, from_field_input, to_field_output)
};

/**
 * An input field is evaluated on an input geometry.
 */
struct EvalRelation {
  int field_input;
  int geometry_input;

  BLI_STRUCT_EQUALITY_OPERATORS_2(EvalRelation, field_input, geometry_input)
};

/**
 * An output field is available on an output geometry.
 */
struct AvailableRelation {
  int field_output;
  int geometry_output;

  BLI_STRUCT_EQUALITY_OPERATORS_2(AvailableRelation, field_output, geometry_output)
};

struct RelationsInNode {
  Vector<PropagateRelation> propagate_relations;
  Vector<ReferenceRelation> reference_relations;
  Vector<EvalRelation> eval_relations;
  Vector<AvailableRelation> available_relations;
  Vector<int> available_on_none;

  BLI_STRUCT_EQUALITY_OPERATORS_5(RelationsInNode,
                                  propagate_relations,
                                  reference_relations,
                                  eval_relations,
                                  available_relations,
                                  available_on_none)
};

std::ostream &operator<<(std::ostream &stream, const RelationsInNode &relations);

}  // namespace anonymous_attribute_lifetime
namespace aal = anonymous_attribute_lifetime;

/** Socket or panel declaration. */
class ItemDeclaration {
 public:
  const PanelDeclaration *parent = nullptr;

  virtual ~ItemDeclaration() = default;
};

using ItemDeclarationPtr = std::unique_ptr<ItemDeclaration>;

struct SocketNameRNA {
  PointerRNA owner = PointerRNA_NULL;
  std::string property_name;
};

struct CustomSocketDrawParams {
  const bContext &C;
  uiLayout &layout;
  bNodeTree &tree;
  bNode &node;
  bNodeSocket &socket;
  PointerRNA node_ptr;
  PointerRNA socket_ptr;
  StringRefNull label;
  const Map<const bNode *, const bNode *> *menu_switch_source_by_index_switch = nullptr;

  void draw_standard(uiLayout &layout, std::optional<StringRefNull> label_override = std::nullopt);
};

using CustomSocketDrawFn = std::function<void(CustomSocketDrawParams &params)>;
using CustomSocketLabelFn = std::function<StringRefNull(bNode node)>;
using SocketUsageInferenceFn =
    std::function<std::optional<bool>(const socket_usage_inference::SocketUsageParams &params)>;

/**
 * Describes a single input or output socket. This is subclassed for different socket types.
 */
class SocketDeclaration : public ItemDeclaration {
 public:
  std::string name;
  std::string short_label;
  std::string identifier;
  std::string description;
  std::optional<std::string> translation_context;
  /** Defined by whether the socket is part of the node's input or
   * output socket declaration list. Included here for convenience. */
  eNodeSocketInOut in_out;
  /** Socket type that corresponds to this socket declaration. */
  eNodeSocketDatatype socket_type;
  /**
   * Indicates that the meaning of the socket values is clear even if the label is not shown. This
   * can result in cleaner UIs in some cases. The drawing code will still draw the label sometimes.
   */
  bool optional_label = false;
  bool hide_value = false;
  bool compact = false;
  bool is_multi_input = false;
  bool no_mute_links = false;
  bool is_available = true;
  bool is_attribute_name = false;
  bool is_default_link_socket = false;
  /** Puts this socket on the same line as the previous one in the UI. */
  bool align_with_previous_socket = false;
  /** This socket is used as a toggle for the parent panel. */
  bool is_panel_toggle = false;
  bool is_layer_name = false;
  bool is_volume_grid_name = false;

  /** Index in the list of inputs or outputs of the node. */
  int index = -1;

  InputSocketFieldType input_field_type = InputSocketFieldType::None;
  OutputFieldDependency output_field_dependency;

  StructureType structure_type = StructureType::Single;

 private:
  CompositorInputRealizationMode compositor_realization_mode_ =
      CompositorInputRealizationMode::OperationDomain;

  /** The priority of the input for determining the domain of the node. If negative, then the
   * domain priority is not set and the index of the input is assumed to be the priority instead.
   * See compositor::InputDescriptor for more information. */
  int compositor_domain_priority_ = -1;

  /** Utility method to make the socket available if there is a straightforward way to do so. */
  std::function<void(bNode &)> make_available_fn_;

 public:
  /** Some input sockets can have non-trivial values in the case when they are unlinked. */
  NodeDefaultInputType default_input_type = NodeDefaultInputType::NODE_DEFAULT_INPUT_VALUE;
  /**
   * Property that stores the name of the socket so that it can be modified directly from the
   * node without going to the side-bar.
   */
  std::unique_ptr<SocketNameRNA> socket_name_rna;
  /**
   * Draw function that overrides how the socket is drawn for a specific node.
   */
  std::unique_ptr<CustomSocketDrawFn> custom_draw_fn;
  /**
   * Custom label function so a socket can display a different text depending on what it does.
   */
  std::unique_ptr<CustomSocketLabelFn> label_fn;
  /**
   * Determines whether this socket is used based on other input values and based on which outputs
   * are used.
   */
  std::unique_ptr<SocketUsageInferenceFn> usage_inference_fn;

  friend NodeDeclarationBuilder;
  friend class BaseSocketDeclarationBuilder;
  template<typename SocketDecl> friend class SocketDeclarationBuilder;

  ~SocketDeclaration() override = default;

  virtual bNodeSocket &build(bNodeTree &ntree, bNode &node) const = 0;
  virtual bool matches(const bNodeSocket &socket) const = 0;
  virtual bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const;

  /**
   * Determine if a new socket described by this declaration could have a valid connection
   * the other socket.
   */
  virtual bool can_connect(const bNodeSocket &socket) const = 0;

  /**
   * Change the node such that the socket will become visible. The node type's update method
   * should be called afterwards.
   * \note this is not necessarily implemented for all node types.
   */
  void make_available(bNode &node) const;

  const CompositorInputRealizationMode &compositor_realization_mode() const;
  int compositor_domain_priority() const;

 protected:
  void set_common_flags(bNodeSocket &socket) const;
  bool matches_common_data(const bNodeSocket &socket) const;
};

class NodeDeclarationBuilder;
class DeclarationListBuilder;
class PanelDeclarationBuilder;

class BaseSocketDeclarationBuilder {
 protected:
  bool reference_pass_all_ = false;
  bool field_on_all_ = false;
  bool propagate_from_all_ = false;
  NodeDeclarationBuilder *node_decl_builder_ = nullptr;
  SocketDeclaration *decl_base_ = nullptr;

  friend class NodeDeclarationBuilder;
  friend class DeclarationListBuilder;

 public:
  virtual ~BaseSocketDeclarationBuilder() = default;

  BaseSocketDeclarationBuilder &optional_label(bool value = true);

  BaseSocketDeclarationBuilder &hide_value(bool value = true);

  BaseSocketDeclarationBuilder &multi_input(bool value = true);

  BaseSocketDeclarationBuilder &compact(bool value = true);

  BaseSocketDeclarationBuilder &short_label(std::string value = "");

  BaseSocketDeclarationBuilder &description(std::string value = "");

  BaseSocketDeclarationBuilder &translation_context(
      std::optional<std::string> value = std::nullopt);

  BaseSocketDeclarationBuilder &no_muted_links(bool value = true);

  /**
   * Can be used to make a socket unavailable. It's still stored in DNA, but it's not shown in the
   * UI and also can't be unhidden.
   */
  BaseSocketDeclarationBuilder &available(bool value = true);

  BaseSocketDeclarationBuilder &is_attribute_name(bool value = true);

  BaseSocketDeclarationBuilder &is_default_link_socket(bool value = true);

  BaseSocketDeclarationBuilder &default_input_type(NodeDefaultInputType value);

  /** The input socket allows passing in a field. */
  BaseSocketDeclarationBuilder &supports_field();

  /**
   * For inputs this means that the input field is evaluated on all geometry inputs. For outputs
   * it means that this contains an anonymous attribute reference that is available on all geometry
   * outputs. This sockets value does not have to be output manually in the node. It's done
   * automatically by #LazyFunctionForGeometryNode. This allows outputting this field even if the
   * geometry output does not have to be computed.
   */
  BaseSocketDeclarationBuilder &field_on_all();

  /** The output is always a field, regardless of any inputs. */
  BaseSocketDeclarationBuilder &field_source();

  /** The input supports a field and is a field by default when nothing is connected. */
  BaseSocketDeclarationBuilder &implicit_field(NodeDefaultInputType default_input);

  /** The input is an implicit field that is evaluated on all geometry inputs. */
  BaseSocketDeclarationBuilder &implicit_field_on_all(NodeDefaultInputType default_input);

  /** The input is evaluated on a subset of the geometry inputs. */
  BaseSocketDeclarationBuilder &implicit_field_on(NodeDefaultInputType default_input,
                                                  Span<int> input_indices);

  /** For inputs that are evaluated or available on a subset of the geometry sockets. */
  BaseSocketDeclarationBuilder &field_on(Span<int> indices);

  /** The output is a field if any of the inputs are a field. */
  BaseSocketDeclarationBuilder &dependent_field();

  /** The output is a field if any of the inputs with indices in the given list is a field. */
  BaseSocketDeclarationBuilder &dependent_field(Vector<int> input_dependencies);

  /**
   * For outputs that combine all input fields into a new field. The output is a field even if none
   * of the inputs is a field.
   */
  BaseSocketDeclarationBuilder &field_source_reference_all();

  /**
   * For outputs that combine a subset of input fields into a new field.
   */
  BaseSocketDeclarationBuilder &reference_pass(Span<int> input_indices);

  /**
   * For outputs that combine all input fields into a new field.
   */
  BaseSocketDeclarationBuilder &reference_pass_all();

  /** Attributes from the all geometry inputs can be propagated. */
  BaseSocketDeclarationBuilder &propagate_all();
  /** Instance attributes from all geometry inputs can be propagated. */
  BaseSocketDeclarationBuilder &propagate_all_instance_attributes();

  BaseSocketDeclarationBuilder &compositor_realization_mode(CompositorInputRealizationMode value);

  /**
   * The priority of the input for determining the domain of the node. Needs to be positive. See
   * compositor::InputDescriptor for more information.
   */
  BaseSocketDeclarationBuilder &compositor_domain_priority(int priority);

  /**
   * Pass a function that sets properties on the node required to make the corresponding socket
   * available, if it is not available on the default state of the node. The function is allowed to
   * make other sockets unavailable, since it is meant to be called when the node is first added.
   * The node type's update function is called afterwards.
   */
  BaseSocketDeclarationBuilder &make_available(std::function<void(bNode &)> fn);

  /**
   * Provide a fully custom draw function for the socket that overrides any default behavior.
   */
  BaseSocketDeclarationBuilder &custom_draw(CustomSocketDrawFn fn);

  /**
   * Provide a function that determines whether this socket is used based on other input values and
   * based on which outputs are used.
   */
  BaseSocketDeclarationBuilder &usage_inference(SocketUsageInferenceFn fn);

  /**
   * Provide a function that determines the UI label of this socket.
   */
  BaseSocketDeclarationBuilder &label_fn(CustomSocketLabelFn fn);

  /**
   * Utility method for the case when the node has a single menu input and this socket is only used
   * when the menu input has a specific value.
   */
  BaseSocketDeclarationBuilder &usage_by_single_menu(const int menu_value);

  /**
   * Utility method for the case when this socket is only used when the menu input of the given
   * identifier has a specific value.
   */
  BaseSocketDeclarationBuilder &usage_by_menu(const StringRef menu_input_identifier,
                                              const int menu_value);

  /**
   * Utility method for the case when this socket is only used when the menu input of the given
   * identifier has one of the specifies values.
   */
  BaseSocketDeclarationBuilder &usage_by_menu(const StringRef menu_input_identifier,
                                              const Array<int> menu_values);

  /**
   * Puts this socket on the same row as the previous socket. This only works when one of them is
   * an input and the other is an output.
   */
  BaseSocketDeclarationBuilder &align_with_previous(bool value = true);

  /**
   * Set a function that retrieves an RNA pointer to the name of the socket. This can be used to be
   * able to rename the socket within the node.
   */
  BaseSocketDeclarationBuilder &socket_name_ptr(PointerRNA ptr, StringRef property_name);
  BaseSocketDeclarationBuilder &socket_name_ptr(const ID *id,
                                                const StructRNA *srna,
                                                const void *data,
                                                StringRef property_name);
  /**
   * Use the socket as a toggle in its panel.
   */
  BaseSocketDeclarationBuilder &panel_toggle(bool value = true);

  BaseSocketDeclarationBuilder &structure_type(StructureType structure_type);

  BaseSocketDeclarationBuilder &is_layer_name(bool value = true);
  BaseSocketDeclarationBuilder &is_volume_grid_name(bool value = true);

  /** Index in the list of inputs or outputs. */
  int index() const;

  bool is_input() const;
  bool is_output() const;
};

/**
 * Wraps a #SocketDeclaration and provides methods to set it up correctly.
 * This is separate from #SocketDeclaration, because it allows separating the API used by nodes to
 * declare themselves from how the declaration is stored internally.
 */
template<typename SocketDecl>
class SocketDeclarationBuilder : public BaseSocketDeclarationBuilder {
 protected:
  using Self = typename SocketDecl::Builder;
  static_assert(std::is_base_of_v<SocketDeclaration, SocketDecl>);
  SocketDecl *decl_;

  friend class NodeDeclarationBuilder;
  friend class DeclarationListBuilder;
};

using SocketDeclarationPtr = std::unique_ptr<SocketDeclaration>;

using DrawNodeLayoutFn = void(uiLayout *, bContext *, PointerRNA *);

class SeparatorDeclaration : public ItemDeclaration {};

class LayoutDeclaration : public ItemDeclaration {
 public:
  std::function<DrawNodeLayoutFn> draw;
  /**
   * Sometimes the default layout has special handling (e.g. choose between #draw_buttons and
   * #draw_buttons_ex).
   */
  bool is_default = false;
};

/**
 * Describes a panel containing sockets or other panels.
 */
class PanelDeclaration : public ItemDeclaration {
 public:
  int identifier;
  std::string name;
  std::string description;
  std::optional<std::string> translation_context;
  bool default_collapsed = false;
  Vector<ItemDeclaration *> items;
  /** Index in the list of panels on the node. */
  int index = -1;
  PanelDeclaration *parent_panel = nullptr;

 private:
  friend NodeDeclarationBuilder;
  friend class PanelDeclarationBuilder;

 public:
  ~PanelDeclaration() override = default;

  void build(bNodePanelState &panel) const;
  bool matches(const bNodePanelState &panel) const;
  void update_or_build(const bNodePanelState &old_panel, bNodePanelState &new_panel) const;

  int depth() const;

  /** Get the declaration for a child item that should be drawn as part of the panel header. */
  const SocketDeclaration *panel_input_decl() const;
};

/**
 * This is a base class for #NodeDeclarationBuilder and #PanelDeclarationBuilder. It unifies the
 * behavior of adding sockets and other items to the root node and to panels.
 */
class DeclarationListBuilder {
 public:
  NodeDeclarationBuilder &node_decl_builder;
  Vector<ItemDeclaration *> &items;
  PanelDeclaration *parent_panel_decl = nullptr;

  DeclarationListBuilder(NodeDeclarationBuilder &node_decl_builder,
                         Vector<ItemDeclaration *> &items)
      : node_decl_builder(node_decl_builder), items(items)
  {
  }

  template<typename DeclType>
  typename DeclType::Builder &add_socket(StringRef name,
                                         StringRef identifier,
                                         eNodeSocketInOut in_out);

  template<typename DeclType>
  typename DeclType::Builder &add_input(StringRef name, StringRef identifier = "");
  template<typename DeclType>
  typename DeclType::Builder &add_output(StringRef name, StringRef identifier = "");

  BaseSocketDeclarationBuilder &add_input(eNodeSocketDatatype socket_type,
                                          StringRef name,
                                          StringRef identifier = "");
  BaseSocketDeclarationBuilder &add_input(eCustomDataType data_type,
                                          StringRef name,
                                          StringRef identifier = "");
  BaseSocketDeclarationBuilder &add_output(eNodeSocketDatatype socket_type,
                                           StringRef name,
                                           StringRef identifier = "");
  BaseSocketDeclarationBuilder &add_output(eCustomDataType data_type,
                                           StringRef name,
                                           StringRef identifier = "");

  PanelDeclarationBuilder &add_panel(StringRef name, int identifier = -1);

  void add_separator();
  void add_default_layout();
  void add_layout(std::function<void(uiLayout *, bContext *, PointerRNA *)> draw);
};

class PanelDeclarationBuilder : public DeclarationListBuilder {
 protected:
  using Self = PanelDeclarationBuilder;
  PanelDeclaration *decl_;

  friend class NodeDeclarationBuilder;

 public:
  PanelDeclarationBuilder(NodeDeclarationBuilder &node_builder, PanelDeclaration &decl)
      : DeclarationListBuilder(node_builder, decl.items), decl_(&decl)
  {
    this->parent_panel_decl = &decl;
  }

  Self &description(std::string value = "");
  Self &translation_context(std::optional<std::string> value = std::nullopt);
  Self &default_closed(bool closed);
};

using PanelDeclarationPtr = std::unique_ptr<PanelDeclaration>;

class NodeDeclaration {
 public:
  /** Contains all items including recursive children. */
  Vector<ItemDeclarationPtr> all_items;
  /** Contains only the items in the root. */
  Vector<ItemDeclaration *> root_items;
  /** All input and output socket declarations. */
  Vector<SocketDeclaration *> inputs;
  Vector<SocketDeclaration *> outputs;
  Vector<PanelDeclaration *> panels;
  std::unique_ptr<aal::RelationsInNode> anonymous_attribute_relations_;

  /** Leave the sockets in place, even if they don't match the declaration. Used for dynamic
   * declarations when the information used to build the declaration is missing, but might become
   * available again in the future. */
  bool skip_updating_sockets = false;

  /** Use order of socket declarations for socket order instead of conventional
   * outputs | buttons | inputs order. Panels are only supported when using custom socket order. */
  bool use_custom_socket_order = false;

  /** Usually output sockets come before input sockets currently. Only some specific nodes are
   * exempt from that rule for now. */
  bool allow_any_socket_order = false;

  /**
   * True if any context was used to build this declaration.
   */
  bool is_context_dependent = false;

  friend NodeDeclarationBuilder;

  /** Asserts that the declaration is considered valid. */
  void assert_valid() const;

  bool matches(const bNode &node) const;
  Span<SocketDeclaration *> sockets(eNodeSocketInOut in_out) const;

  const aal::RelationsInNode *anonymous_attribute_relations() const
  {
    return anonymous_attribute_relations_.get();
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("NodeDeclaration")
};

class NodeDeclarationBuilder : public DeclarationListBuilder {
 private:
  const bke::bNodeType &typeinfo_;
  NodeDeclaration &declaration_;
  const bNodeTree *ntree_ = nullptr;
  const bNode *node_ = nullptr;
  Vector<std::unique_ptr<BaseSocketDeclarationBuilder>> socket_builders_;
  Vector<BaseSocketDeclarationBuilder *> input_socket_builders_;
  Vector<BaseSocketDeclarationBuilder *> output_socket_builders_;
  Vector<std::unique_ptr<PanelDeclarationBuilder>> panel_builders_;
  bool is_function_node_ = false;

  friend DeclarationListBuilder;

 public:
  NodeDeclarationBuilder(const bke::bNodeType &typeinfo,
                         NodeDeclaration &declaration,
                         const bNodeTree *ntree = nullptr,
                         const bNode *node = nullptr);

  const bNode *node_or_null() const
  {
    declaration_.is_context_dependent = true;
    return node_;
  }

  const bNodeTree *tree_or_null() const
  {
    declaration_.is_context_dependent = true;
    return ntree_;
  }

  /**
   * All inputs support fields, and all outputs are fields if any of the inputs is a field.
   * Calling field status definitions on each socket is unnecessary.
   */
  void is_function_node()
  {
    is_function_node_ = true;
  }

  void finalize();

  void use_custom_socket_order(bool enable = true);
  void allow_any_socket_order(bool enable = true);

  aal::RelationsInNode &get_anonymous_attribute_relations()
  {
    if (!declaration_.anonymous_attribute_relations_) {
      declaration_.anonymous_attribute_relations_ = std::make_unique<aal::RelationsInNode>();
    }
    return *declaration_.anonymous_attribute_relations_;
  }

  NodeDeclaration &declaration()
  {
    return declaration_;
  }

 private:
  void build_remaining_anonymous_attribute_relations();
};

using ImplicitInputValueFn = std::function<void(const bNode &node, void *r_value)>;
std::optional<ImplicitInputValueFn> get_implicit_input_value_fn(NodeDefaultInputType type);
bool socket_type_supports_default_input_type(const bke::bNodeSocketType &socket_type,
                                             NodeDefaultInputType input_type);

void build_node_declaration(const bke::bNodeType &typeinfo,
                            NodeDeclaration &r_declaration,
                            const bNodeTree *ntree,
                            const bNode *node);

std::unique_ptr<SocketDeclaration> make_declaration_for_socket_type(
    eNodeSocketDatatype socket_type);

/* -------------------------------------------------------------------- */
/** \name #DeclarationListBuilder Inline Methods
 * \{ */

template<typename DeclType>
inline typename DeclType::Builder &DeclarationListBuilder::add_input(StringRef name,
                                                                     StringRef identifier)
{
  return this->add_socket<DeclType>(name, identifier, SOCK_IN);
}

template<typename DeclType>
inline typename DeclType::Builder &DeclarationListBuilder::add_output(StringRef name,
                                                                      StringRef identifier)
{
  return this->add_socket<DeclType>(name, identifier, SOCK_OUT);
}

template<typename DeclType>
inline typename DeclType::Builder &DeclarationListBuilder::add_socket(StringRef name,
                                                                      StringRef identifier,
                                                                      eNodeSocketInOut in_out)
{
  static_assert(std::is_base_of_v<SocketDeclaration, DeclType>);
  using SocketBuilder = typename DeclType::Builder;

  BLI_assert(ELEM(in_out, SOCK_IN, SOCK_OUT));

  std::unique_ptr<SocketBuilder> socket_decl_builder_ptr = std::make_unique<SocketBuilder>();
  SocketBuilder &socket_decl_builder = *socket_decl_builder_ptr;
  this->node_decl_builder.socket_builders_.append(std::move(socket_decl_builder_ptr));

  std::unique_ptr<DeclType> socket_decl_ptr = std::make_unique<DeclType>();
  DeclType &socket_decl = *socket_decl_ptr;
  this->node_decl_builder.declaration_.all_items.append(std::move(socket_decl_ptr));
  this->items.append(&socket_decl);

  socket_decl.parent = this->parent_panel_decl;
  socket_decl_builder.node_decl_builder_ = &this->node_decl_builder;

  socket_decl_builder.decl_ = &socket_decl;
  socket_decl_builder.decl_base_ = &socket_decl;
  socket_decl.name = name;
  socket_decl.identifier = identifier.is_empty() ? name : identifier;
  socket_decl.in_out = in_out;
  socket_decl.socket_type = DeclType::static_socket_type;

  if (this->node_decl_builder.is_function_node_) {
    if (in_out == SOCK_IN) {
      socket_decl_builder.supports_field();
    }
    else {
      socket_decl_builder.dependent_field();
    }
  }

  if (in_out == SOCK_IN) {
    this->node_decl_builder.input_socket_builders_.append(&socket_decl_builder);
    socket_decl.index = this->node_decl_builder.declaration_.inputs.append_and_get_index(
        &socket_decl);
  }
  else {
    this->node_decl_builder.output_socket_builders_.append(&socket_decl_builder);
    socket_decl.index = this->node_decl_builder.declaration_.outputs.append_and_get_index(
        &socket_decl);
  }
  return socket_decl_builder;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #BaseSocketDeclarationBuilder Inline Methods
 * \{ */

inline int BaseSocketDeclarationBuilder::index() const
{
  return decl_base_->index;
}

inline bool BaseSocketDeclarationBuilder::is_input() const
{
  return decl_base_->in_out == SOCK_IN;
}

inline bool BaseSocketDeclarationBuilder::is_output() const
{
  return decl_base_->in_out == SOCK_OUT;
}

/** \} */

}  // namespace blender::nodes
