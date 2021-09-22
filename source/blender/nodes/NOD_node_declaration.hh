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

#include <type_traits>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_node_types.h"

namespace blender::nodes {

class NodeDeclarationBuilder;

/**
 * Describes a single input or output socket. This is subclassed for different socket types.
 */
class SocketDeclaration {
 protected:
  std::string name_;
  std::string identifier_;
  bool hide_label_ = false;
  bool hide_value_ = false;
  bool is_multi_input_ = false;
  bool no_mute_links_ = false;

  friend NodeDeclarationBuilder;
  template<typename SocketDecl> friend class SocketDeclarationBuilder;

 public:
  virtual ~SocketDeclaration() = default;

  virtual bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const = 0;
  virtual bool matches(const bNodeSocket &socket) const = 0;
  virtual bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const;

  StringRefNull name() const;
  StringRefNull identifier() const;

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

  Self &no_muted_links(bool value = true)
  {
    decl_->no_mute_links_ = value;
    return *(Self *)this;
  }
};

using SocketDeclarationPtr = std::unique_ptr<SocketDeclaration>;

class NodeDeclaration {
 private:
  Vector<SocketDeclarationPtr> inputs_;
  Vector<SocketDeclarationPtr> outputs_;

  friend NodeDeclarationBuilder;

 public:
  void build(bNodeTree &ntree, bNode &node) const;
  bool matches(const bNode &node) const;

  Span<SocketDeclarationPtr> inputs() const;
  Span<SocketDeclarationPtr> outputs() const;

  MEM_CXX_CLASS_ALLOC_FUNCS("NodeDeclaration")
};

class NodeDeclarationBuilder {
 private:
  NodeDeclaration &declaration_;
  Vector<std::unique_ptr<BaseSocketDeclarationBuilder>> builders_;

 public:
  NodeDeclarationBuilder(NodeDeclaration &declaration);

  template<typename DeclType>
  typename DeclType::Builder &add_input(StringRef name, StringRef identifier = "");
  template<typename DeclType>
  typename DeclType::Builder &add_output(StringRef name, StringRef identifier = "");

 private:
  template<typename DeclType>
  typename DeclType::Builder &add_socket(StringRef name,
                                         StringRef identifier,
                                         Vector<SocketDeclarationPtr> &r_decls);
};

/* --------------------------------------------------------------------
 * SocketDeclaration inline methods.
 */

inline StringRefNull SocketDeclaration::name() const
{
  return name_;
}

inline StringRefNull SocketDeclaration::identifier() const
{
  return identifier_;
}

/* --------------------------------------------------------------------
 * NodeDeclarationBuilder inline methods.
 */

inline NodeDeclarationBuilder::NodeDeclarationBuilder(NodeDeclaration &declaration)
    : declaration_(declaration)
{
}

template<typename DeclType>
inline typename DeclType::Builder &NodeDeclarationBuilder::add_input(StringRef name,
                                                                     StringRef identifier)
{
  return this->add_socket<DeclType>(name, identifier, declaration_.inputs_);
}

template<typename DeclType>
inline typename DeclType::Builder &NodeDeclarationBuilder::add_output(StringRef name,
                                                                      StringRef identifier)
{
  return this->add_socket<DeclType>(name, identifier, declaration_.outputs_);
}

template<typename DeclType>
inline typename DeclType::Builder &NodeDeclarationBuilder::add_socket(
    StringRef name, StringRef identifier, Vector<SocketDeclarationPtr> &r_decls)
{
  static_assert(std::is_base_of_v<SocketDeclaration, DeclType>);
  using Builder = typename DeclType::Builder;
  std::unique_ptr<DeclType> socket_decl = std::make_unique<DeclType>();
  std::unique_ptr<Builder> socket_decl_builder = std::make_unique<Builder>();
  socket_decl_builder->decl_ = &*socket_decl;
  socket_decl->name_ = name;
  socket_decl->identifier_ = identifier.is_empty() ? name : identifier;
  r_decls.append(std::move(socket_decl));
  Builder &socket_decl_builder_ref = *socket_decl_builder;
  builders_.append(std::move(socket_decl_builder));
  return socket_decl_builder_ref;
}

/* --------------------------------------------------------------------
 * NodeDeclaration inline methods.
 */

inline Span<SocketDeclarationPtr> NodeDeclaration::inputs() const
{
  return inputs_;
}

inline Span<SocketDeclarationPtr> NodeDeclaration::outputs() const
{
  return outputs_;
}

}  // namespace blender::nodes
