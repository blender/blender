/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_node_types.h"

struct bContext;
struct bNode;
struct PointerRNA;
struct uiLayout;

namespace blender::nodes {

class NodeDeclarationBuilder;

enum class InputSocketFieldType {
  /** The input is required to be a single value. */
  None,
  /** The input can be a field. */
  IsSupported,
  /** The input can be a field and is a field implicitly if nothing is connected. */
  Implicit,
};

enum class OutputSocketFieldType {
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
 * A bit-field that maps to the realtime_compositor::InputRealizationOptions.
 */
enum class CompositorInputRealizationOptions : uint8_t {
  None = 0,
  RealizeOnOperationDomain = (1 << 0),
};
ENUM_OPERATORS(CompositorInputRealizationOptions,
               CompositorInputRealizationOptions::RealizeOnOperationDomain)

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
  Vector<InputSocketFieldType> inputs;
  Vector<OutputFieldDependency> outputs;

  BLI_STRUCT_EQUALITY_OPERATORS_2(FieldInferencingInterface, inputs, outputs)
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

using ImplicitInputValueFn = std::function<void(const bNode &node, void *r_value)>;

/* Socket or panel declaration. */
class ItemDeclaration {
 public:
  virtual ~ItemDeclaration() = default;
};

using ItemDeclarationPtr = std::unique_ptr<ItemDeclaration>;

/**
 * Describes a single input or output socket. This is subclassed for different socket types.
 */
class SocketDeclaration : public ItemDeclaration {
 public:
  std::string name;
  std::string short_label;
  std::string identifier;
  std::string description;
  std::string translation_context;
  /** Defined by whether the socket is part of the node's input or
   * output socket declaration list. Included here for convenience. */
  eNodeSocketInOut in_out;
  bool hide_label = false;
  bool hide_value = false;
  bool compact = false;
  bool is_multi_input = false;
  bool no_mute_links = false;
  bool is_unavailable = false;
  bool is_attribute_name = false;
  bool is_default_link_socket = false;

  InputSocketFieldType input_field_type = InputSocketFieldType::None;
  OutputFieldDependency output_field_dependency;

 private:
  CompositorInputRealizationOptions compositor_realization_options_ =
      CompositorInputRealizationOptions::RealizeOnOperationDomain;

  /** The priority of the input for determining the domain of the node. See
   * realtime_compositor::InputDescriptor for more information. */
  int compositor_domain_priority_ = 0;

  /** This input expects a single value and can't operate on non-single values. See
   * realtime_compositor::InputDescriptor for more information. */
  bool compositor_expects_single_value_ = false;

  /** Utility method to make the socket available if there is a straightforward way to do so. */
  std::function<void(bNode &)> make_available_fn_;

 public:
  /** Some input sockets can have non-trivial values in the case when they are unlinked. This
   * callback computes the default input of a values in geometry nodes when nothing is linked. */
  std::unique_ptr<ImplicitInputValueFn> implicit_input_fn;

  friend NodeDeclarationBuilder;
  friend class BaseSocketDeclarationBuilder;
  template<typename SocketDecl> friend class SocketDeclarationBuilder;

 public:
  virtual ~SocketDeclaration() = default;

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

  const CompositorInputRealizationOptions &compositor_realization_options() const;
  int compositor_domain_priority() const;
  bool compositor_expects_single_value() const;

 protected:
  void set_common_flags(bNodeSocket &socket) const;
  bool matches_common_data(const bNodeSocket &socket) const;
};

class NodeDeclarationBuilder;

class BaseSocketDeclarationBuilder {
 protected:
  /* Socket builder can hold both an input and an output declaration.
   * Each socket declaration has its own index for dependencies. */
  int index_in_ = -1;
  int index_out_ = -1;
  bool reference_pass_all_ = false;
  bool field_on_all_ = false;
  bool propagate_from_all_ = false;
  NodeDeclarationBuilder *node_decl_builder_ = nullptr;
  SocketDeclaration *decl_in_base_ = nullptr;
  SocketDeclaration *decl_out_base_ = nullptr;

  friend class NodeDeclarationBuilder;

 public:
  virtual ~BaseSocketDeclarationBuilder() = default;

  BaseSocketDeclarationBuilder &hide_label(bool value = true);

  BaseSocketDeclarationBuilder &hide_value(bool value = true);

  BaseSocketDeclarationBuilder &multi_input(bool value = true);

  BaseSocketDeclarationBuilder &short_label(std::string value = "");

  BaseSocketDeclarationBuilder &description(std::string value = "");

  BaseSocketDeclarationBuilder &translation_context(std::string value = BLT_I18NCONTEXT_DEFAULT);

  BaseSocketDeclarationBuilder &no_muted_links(bool value = true);

  /**
   * Used for sockets that are always unavailable and should not be seen by the user.
   * Ideally, no new calls to this method should be added over time.
   */
  BaseSocketDeclarationBuilder &unavailable(bool value = true);

  BaseSocketDeclarationBuilder &is_attribute_name(bool value = true);

  BaseSocketDeclarationBuilder &is_default_link_socket(bool value = true);

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
  BaseSocketDeclarationBuilder &implicit_field(ImplicitInputValueFn fn);

  /** The input is an implicit field that is evaluated on all geometry inputs. */
  BaseSocketDeclarationBuilder &implicit_field_on_all(ImplicitInputValueFn fn);

  /** The input is evaluated on a subset of the geometry inputs. */
  BaseSocketDeclarationBuilder &implicit_field_on(ImplicitInputValueFn fn,
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

  BaseSocketDeclarationBuilder &compositor_realization_options(
      CompositorInputRealizationOptions value);

  /**
   * The priority of the input for determining the domain of the node. See
   * realtime_compositor::InputDescriptor for more information.
   */
  BaseSocketDeclarationBuilder &compositor_domain_priority(int priority);

  /**
   * This input expects a single value and can't operate on non-single values. See
   * realtime_compositor::InputDescriptor for more information.
   */
  BaseSocketDeclarationBuilder &compositor_expects_single_value(bool value = true);

  /**
   * Pass a function that sets properties on the node required to make the corresponding socket
   * available, if it is not available on the default state of the node. The function is allowed to
   * make other sockets unavailable, since it is meant to be called when the node is first added.
   * The node type's update function is called afterwards.
   */
  BaseSocketDeclarationBuilder &make_available(std::function<void(bNode &)> fn);

  int input_index() const
  {
    BLI_assert(decl_in_base_ != nullptr);
    return index_in_;
  }

  int output_index() const
  {
    BLI_assert(decl_out_base_ != nullptr);
    return index_out_;
  }
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
  SocketDecl *decl_in_;
  SocketDecl *decl_out_;

  friend class NodeDeclarationBuilder;
};

using SocketDeclarationPtr = std::unique_ptr<SocketDeclaration>;

using PanelDrawButtonsFunction = void (*)(uiLayout *, bContext *, PointerRNA *);

/**
 * Describes a panel containing sockets or other panels.
 */
class PanelDeclaration : public ItemDeclaration {
 public:
  int identifier;
  std::string name;
  std::string description;
  std::string translation_context;
  bool default_collapsed = false;
  int num_child_decls = 0;
  PanelDrawButtonsFunction draw_buttons = nullptr;

 private:
  friend NodeDeclarationBuilder;
  friend class PanelDeclarationBuilder;

 public:
  virtual ~PanelDeclaration() = default;

  void build(bNodePanelState &panel) const;
  bool matches(const bNodePanelState &panel) const;
  void update_or_build(const bNodePanelState &old_panel, bNodePanelState &new_panel) const;
};

class PanelDeclarationBuilder {
 protected:
  using Self = PanelDeclarationBuilder;
  NodeDeclarationBuilder *node_decl_builder_ = nullptr;
  PanelDeclaration *decl_;
  /**
   * Panel is complete once items are added after it.
   * Completed panels are locked and no more items can be added.
   */
  bool is_complete_ = false;

  friend class NodeDeclarationBuilder;

 public:
  Self &description(std::string value = "");
  Self &default_closed(bool closed);
  Self &draw_buttons(PanelDrawButtonsFunction func);

  template<typename DeclType>
  typename DeclType::Builder &add_input(StringRef name, StringRef identifier = "");
  template<typename DeclType>
  typename DeclType::Builder &add_output(StringRef name, StringRef identifier = "");
};

using PanelDeclarationPtr = std::unique_ptr<PanelDeclaration>;

class NodeDeclaration {
 public:
  /* Combined list of socket and panel declarations.
   * This determines order of sockets in the UI and panel content. */
  Vector<ItemDeclarationPtr> items;
  /* Note: inputs and outputs pointers are owned by the items list. */
  Vector<SocketDeclaration *> inputs;
  Vector<SocketDeclaration *> outputs;
  std::unique_ptr<aal::RelationsInNode> anonymous_attribute_relations_;

  /** Leave the sockets in place, even if they don't match the declaration. Used for dynamic
   * declarations when the information used to build the declaration is missing, but might become
   * available again in the future. */
  bool skip_updating_sockets = false;

  /** Use order of socket declarations for socket order instead of conventional
   * outputs | buttons | inputs order. Panels are only supported when using custom socket order. */
  bool use_custom_socket_order = false;

  /**
   * True if any context was used to build this declaration.
   */
  bool is_context_dependent = false;

  friend NodeDeclarationBuilder;

  /** Returns true if the declaration is considered valid. */
  bool is_valid() const;

  bool matches(const bNode &node) const;
  Span<SocketDeclaration *> sockets(eNodeSocketInOut in_out) const;

  const aal::RelationsInNode *anonymous_attribute_relations() const
  {
    return anonymous_attribute_relations_.get();
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("NodeDeclaration")
};

class NodeDeclarationBuilder {
 private:
  NodeDeclaration &declaration_;
  const bNodeTree *ntree_ = nullptr;
  const bNode *node_ = nullptr;
  Vector<std::unique_ptr<BaseSocketDeclarationBuilder>> socket_builders_;
  Vector<std::unique_ptr<PanelDeclarationBuilder>> panel_builders_;
  bool is_function_node_ = false;

 private:
  friend PanelDeclarationBuilder;

 public:
  NodeDeclarationBuilder(NodeDeclaration &declaration,
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

  template<typename DeclType>
  typename DeclType::Builder &add_input(StringRef name, StringRef identifier = "");
  template<typename DeclType>
  typename DeclType::Builder &add_output(StringRef name, StringRef identifier = "");
  PanelDeclarationBuilder &add_panel(StringRef name, int identifier = -1);

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
  /* Note: in_out can be a combination of SOCK_IN and SOCK_OUT.
   * The generated socket declarations only have a single flag set. */
  template<typename DeclType>
  typename DeclType::Builder &add_socket(StringRef name,
                                         StringRef identifier_in,
                                         StringRef identifier_out,
                                         eNodeSocketInOut in_out);

  /* Mark the most recent builder as 'complete' when changing builders
   * so no more items can be added. */
  void set_active_panel_builder(const PanelDeclarationBuilder *panel_builder);
};

namespace implicit_field_inputs {
void position(const bNode &node, void *r_value);
void normal(const bNode &node, void *r_value);
void index(const bNode &node, void *r_value);
void id_or_index(const bNode &node, void *r_value);
}  // namespace implicit_field_inputs

void build_node_declaration(const bNodeType &typeinfo,
                            NodeDeclaration &r_declaration,
                            const bNodeTree *ntree,
                            const bNode *node);

std::unique_ptr<SocketDeclaration> make_declaration_for_socket_type(
    eNodeSocketDatatype socket_type);

/* -------------------------------------------------------------------- */
/** \name #PanelDeclarationBuilder Inline Methods
 * \{ */

template<typename DeclType>
typename DeclType::Builder &PanelDeclarationBuilder::add_input(StringRef name,
                                                               StringRef identifier)
{
  if (is_complete_) {
    static typename DeclType::Builder dummy_builder = {};
    BLI_assert_unreachable();
    return dummy_builder;
  }
  ++this->decl_->num_child_decls;
  return node_decl_builder_->add_socket<DeclType>(name, identifier, "", SOCK_IN);
}

template<typename DeclType>
typename DeclType::Builder &PanelDeclarationBuilder::add_output(StringRef name,
                                                                StringRef identifier)
{
  if (is_complete_) {
    static typename DeclType::Builder dummy_builder = {};
    BLI_assert_unreachable();
    return dummy_builder;
  }
  ++this->decl_->num_child_decls;
  return node_decl_builder_->add_socket<DeclType>(name, "", identifier, SOCK_OUT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #NodeDeclarationBuilder Inline Methods
 * \{ */

template<typename DeclType>
inline typename DeclType::Builder &NodeDeclarationBuilder::add_input(StringRef name,
                                                                     StringRef identifier)
{
  set_active_panel_builder(nullptr);
  return this->add_socket<DeclType>(name, identifier, "", SOCK_IN);
}

template<typename DeclType>
inline typename DeclType::Builder &NodeDeclarationBuilder::add_output(StringRef name,
                                                                      StringRef identifier)
{
  set_active_panel_builder(nullptr);
  return this->add_socket<DeclType>(name, "", identifier, SOCK_OUT);
}

template<typename DeclType>
inline typename DeclType::Builder &NodeDeclarationBuilder::add_socket(StringRef name,
                                                                      StringRef identifier_in,
                                                                      StringRef identifier_out,
                                                                      eNodeSocketInOut in_out)
{
  static_assert(std::is_base_of_v<SocketDeclaration, DeclType>);
  using Builder = typename DeclType::Builder;

  std::unique_ptr<Builder> socket_decl_builder = std::make_unique<Builder>();
  socket_decl_builder->node_decl_builder_ = this;

  if (in_out & SOCK_IN) {
    std::unique_ptr<DeclType> socket_decl = std::make_unique<DeclType>();
    socket_decl_builder->decl_in_ = &*socket_decl;
    socket_decl_builder->decl_in_base_ = &*socket_decl;
    socket_decl->name = name;
    socket_decl->identifier = identifier_in.is_empty() ? name : identifier_in;
    socket_decl->in_out = SOCK_IN;
    socket_decl_builder->index_in_ = declaration_.inputs.append_and_get_index(socket_decl.get());
    declaration_.items.append(std::move(socket_decl));
  }
  if (in_out & SOCK_OUT) {
    std::unique_ptr<DeclType> socket_decl = std::make_unique<DeclType>();
    socket_decl_builder->decl_out_ = &*socket_decl;
    socket_decl_builder->decl_out_base_ = &*socket_decl;
    socket_decl->name = name;
    socket_decl->identifier = identifier_out.is_empty() ? name : identifier_out;
    socket_decl->in_out = SOCK_OUT;
    socket_decl_builder->index_out_ = declaration_.outputs.append_and_get_index(socket_decl.get());
    declaration_.items.append(std::move(socket_decl));
  }

  Builder &socket_decl_builder_ref = *socket_decl_builder;
  socket_builders_.append(std::move(socket_decl_builder));

  return socket_decl_builder_ref;
}

/** \} */

}  // namespace blender::nodes
