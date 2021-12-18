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

#include <functional>
#include <type_traits>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

struct bNode;

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

  friend bool operator==(const OutputFieldDependency &a, const OutputFieldDependency &b);
};

/**
 * Information about how a node interacts with fields.
 */
struct FieldInferencingInterface {
  Vector<InputSocketFieldType> inputs;
  Vector<OutputFieldDependency> outputs;
};

/**
 * Describes a single input or output socket. This is subclassed for different socket types.
 */
class SocketDeclaration {
 protected:
  std::string name_;
  std::string identifier_;
  std::string description_;
  /** Defined by whether the socket is part of the node's input or
   * output socket declaration list. Included here for convenience. */
  eNodeSocketInOut in_out_;
  bool hide_label_ = false;
  bool hide_value_ = false;
  bool compact_ = false;
  bool is_multi_input_ = false;
  bool no_mute_links_ = false;
  bool is_attribute_name_ = false;
  bool is_default_link_socket_ = false;

  InputSocketFieldType input_field_type_ = InputSocketFieldType::None;
  OutputFieldDependency output_field_dependency_;

  /** Utility method to make the socket available if there is a straightforward way to do so. */
  std::function<void(bNode &)> make_available_fn_;

  friend NodeDeclarationBuilder;
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
   * \note Note that this is not necessarily implemented for all node types.
   */
  void make_available(bNode &node) const;

  StringRefNull name() const;
  StringRefNull description() const;
  StringRefNull identifier() const;
  eNodeSocketInOut in_out() const;
  bool is_attribute_name() const;
  bool is_default_link_socket() const;

  InputSocketFieldType input_field_type() const;
  const OutputFieldDependency &output_field_dependency() const;

 protected:
  void set_common_flags(bNodeSocket &socket) const;
  bool matches_common_data(const bNodeSocket &socket) const;
};

class BaseSocketDeclarationBuilder {
 public:
  virtual ~BaseSocketDeclarationBuilder() = default;
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

 public:
  Self &hide_label(bool value = true)
  {
    decl_->hide_label_ = value;
    return *(Self *)this;
  }

  Self &hide_value(bool value = true)
  {
    decl_->hide_value_ = value;
    return *(Self *)this;
  }

  Self &multi_input(bool value = true)
  {
    decl_->is_multi_input_ = value;
    return *(Self *)this;
  }

  Self &description(std::string value = "")
  {
    decl_->description_ = std::move(value);
    return *(Self *)this;
  }
  Self &no_muted_links(bool value = true)
  {
    decl_->no_mute_links_ = value;
    return *(Self *)this;
  }

  Self &is_attribute_name(bool value = true)
  {
    decl_->is_attribute_name_ = value;
    return *(Self *)this;
  }

  Self &is_default_link_socket(bool value = true)
  {
    decl_->is_default_link_socket_ = value;
    return *(Self *)this;
  }

  /** The input socket allows passing in a field. */
  Self &supports_field()
  {
    decl_->input_field_type_ = InputSocketFieldType::IsSupported;
    return *(Self *)this;
  }

  /** The input supports a field and is a field by default when nothing is connected. */
  Self &implicit_field()
  {
    this->hide_value();
    decl_->input_field_type_ = InputSocketFieldType::Implicit;
    return *(Self *)this;
  }

  /** The output is always a field, regardless of any inputs. */
  Self &field_source()
  {
    decl_->output_field_dependency_ = OutputFieldDependency::ForFieldSource();
    return *(Self *)this;
  }

  /** The output is a field if any of the inputs is a field. */
  Self &dependent_field()
  {
    decl_->output_field_dependency_ = OutputFieldDependency::ForDependentField();
    return *(Self *)this;
  }

  /** The output is a field if any of the inputs with indices in the given list is a field. */
  Self &dependent_field(Vector<int> input_dependencies)
  {
    decl_->output_field_dependency_ = OutputFieldDependency::ForPartiallyDependentField(
        std::move(input_dependencies));
    return *(Self *)this;
  }

  /**
   * Pass a function that sets properties on the node required to make the corresponding socket
   * available, if it is not available on the default state of the node. The function is allowed to
   * make other sockets unavailable, since it is meant to be called when the node is first added.
   * The node type's update function is called afterwards.
   */
  Self &make_available(std::function<void(bNode &)> fn)
  {
    decl_->make_available_fn_ = std::move(fn);
    return *(Self *)this;
  }
};

using SocketDeclarationPtr = std::unique_ptr<SocketDeclaration>;

class NodeDeclaration {
 private:
  Vector<SocketDeclarationPtr> inputs_;
  Vector<SocketDeclarationPtr> outputs_;
  bool is_function_node_ = false;

  friend NodeDeclarationBuilder;

 public:
  bool matches(const bNode &node) const;

  Span<SocketDeclarationPtr> inputs() const;
  Span<SocketDeclarationPtr> outputs() const;
  Span<SocketDeclarationPtr> sockets(eNodeSocketInOut in_out) const;

  bool is_function_node() const
  {
    return is_function_node_;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("NodeDeclaration")
};

class NodeDeclarationBuilder {
 private:
  NodeDeclaration &declaration_;
  Vector<std::unique_ptr<BaseSocketDeclarationBuilder>> builders_;

 public:
  NodeDeclarationBuilder(NodeDeclaration &declaration);

  /**
   * All inputs support fields, and all outputs are fields if any of the inputs is a field.
   * Calling field status definitions on each socket is unnecessary. Must be called before adding
   * any sockets.
   */
  void is_function_node(bool value = true)
  {
    BLI_assert_msg(declaration_.inputs().is_empty() && declaration_.outputs().is_empty(),
                   "is_function_node() must be called before any socket is created");
    declaration_.is_function_node_ = value;
  }

  template<typename DeclType>
  typename DeclType::Builder &add_input(StringRef name, StringRef identifier = "");
  template<typename DeclType>
  typename DeclType::Builder &add_output(StringRef name, StringRef identifier = "");

 private:
  template<typename DeclType>
  typename DeclType::Builder &add_socket(StringRef name,
                                         StringRef identifier,
                                         eNodeSocketInOut in_out);
};

/* -------------------------------------------------------------------- */
/** \name #OutputFieldDependency Inline Methods
 * \{ */

inline OutputFieldDependency OutputFieldDependency::ForFieldSource()
{
  OutputFieldDependency field_dependency;
  field_dependency.type_ = OutputSocketFieldType::FieldSource;
  return field_dependency;
}

inline OutputFieldDependency OutputFieldDependency::ForDataSource()
{
  OutputFieldDependency field_dependency;
  field_dependency.type_ = OutputSocketFieldType::None;
  return field_dependency;
}

inline OutputFieldDependency OutputFieldDependency::ForDependentField()
{
  OutputFieldDependency field_dependency;
  field_dependency.type_ = OutputSocketFieldType::DependentField;
  return field_dependency;
}

inline OutputFieldDependency OutputFieldDependency::ForPartiallyDependentField(Vector<int> indices)
{
  OutputFieldDependency field_dependency;
  if (indices.is_empty()) {
    field_dependency.type_ = OutputSocketFieldType::None;
  }
  else {
    field_dependency.type_ = OutputSocketFieldType::PartiallyDependent;
    field_dependency.linked_input_indices_ = std::move(indices);
  }
  return field_dependency;
}

inline OutputSocketFieldType OutputFieldDependency::field_type() const
{
  return type_;
}

inline Span<int> OutputFieldDependency::linked_input_indices() const
{
  return linked_input_indices_;
}

inline bool operator==(const OutputFieldDependency &a, const OutputFieldDependency &b)
{
  return a.type_ == b.type_ && a.linked_input_indices_ == b.linked_input_indices_;
}

inline bool operator!=(const OutputFieldDependency &a, const OutputFieldDependency &b)
{
  return !(a == b);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #FieldInferencingInterface Inline Methods
 * \{ */

inline bool operator==(const FieldInferencingInterface &a, const FieldInferencingInterface &b)
{
  return a.inputs == b.inputs && a.outputs == b.outputs;
}

inline bool operator!=(const FieldInferencingInterface &a, const FieldInferencingInterface &b)
{
  return !(a == b);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #SocketDeclaration Inline Methods
 * \{ */

inline StringRefNull SocketDeclaration::name() const
{
  return name_;
}

inline StringRefNull SocketDeclaration::identifier() const
{
  return identifier_;
}

inline eNodeSocketInOut SocketDeclaration::in_out() const
{
  return in_out_;
}

inline StringRefNull SocketDeclaration::description() const
{
  return description_;
}

inline bool SocketDeclaration::is_attribute_name() const
{
  return is_attribute_name_;
}

inline bool SocketDeclaration::is_default_link_socket() const
{
  return is_default_link_socket_;
}

inline InputSocketFieldType SocketDeclaration::input_field_type() const
{
  return input_field_type_;
}

inline const OutputFieldDependency &SocketDeclaration::output_field_dependency() const
{
  return output_field_dependency_;
}

inline void SocketDeclaration::make_available(bNode &node) const
{
  if (make_available_fn_) {
    make_available_fn_(node);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #NodeDeclarationBuilder Inline Methods
 * \{ */

inline NodeDeclarationBuilder::NodeDeclarationBuilder(NodeDeclaration &declaration)
    : declaration_(declaration)
{
}

template<typename DeclType>
inline typename DeclType::Builder &NodeDeclarationBuilder::add_input(StringRef name,
                                                                     StringRef identifier)
{
  return this->add_socket<DeclType>(name, identifier, SOCK_IN);
}

template<typename DeclType>
inline typename DeclType::Builder &NodeDeclarationBuilder::add_output(StringRef name,
                                                                      StringRef identifier)
{
  return this->add_socket<DeclType>(name, identifier, SOCK_OUT);
}

template<typename DeclType>
inline typename DeclType::Builder &NodeDeclarationBuilder::add_socket(StringRef name,
                                                                      StringRef identifier,
                                                                      eNodeSocketInOut in_out)
{
  static_assert(std::is_base_of_v<SocketDeclaration, DeclType>);
  using Builder = typename DeclType::Builder;

  Vector<SocketDeclarationPtr> &declarations = in_out == SOCK_IN ? declaration_.inputs_ :
                                                                   declaration_.outputs_;

  std::unique_ptr<DeclType> socket_decl = std::make_unique<DeclType>();
  std::unique_ptr<Builder> socket_decl_builder = std::make_unique<Builder>();
  socket_decl_builder->decl_ = &*socket_decl;
  socket_decl->name_ = name;
  socket_decl->identifier_ = identifier.is_empty() ? name : identifier;
  socket_decl->in_out_ = in_out;
  if (declaration_.is_function_node()) {
    socket_decl->input_field_type_ = InputSocketFieldType::IsSupported;
    socket_decl->output_field_dependency_ = OutputFieldDependency::ForDependentField();
  }
  declarations.append(std::move(socket_decl));
  Builder &socket_decl_builder_ref = *socket_decl_builder;
  builders_.append(std::move(socket_decl_builder));
  return socket_decl_builder_ref;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #NodeDeclaration Inline Methods
 * \{ */

inline Span<SocketDeclarationPtr> NodeDeclaration::inputs() const
{
  return inputs_;
}

inline Span<SocketDeclarationPtr> NodeDeclaration::outputs() const
{
  return outputs_;
}

inline Span<SocketDeclarationPtr> NodeDeclaration::sockets(eNodeSocketInOut in_out) const
{
  if (in_out == SOCK_IN) {
    return inputs_;
  }
  return outputs_;
}

/** \} */

}  // namespace blender::nodes
