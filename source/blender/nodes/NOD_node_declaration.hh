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

class SocketDeclaration {
 protected:
  std::string name_;
  std::string identifier_;

  friend NodeDeclarationBuilder;

 public:
  virtual ~SocketDeclaration() = default;

  virtual bNodeSocket &build(bNodeTree &ntree, bNode &node, eNodeSocketInOut in_out) const = 0;
  virtual bool matches(const bNodeSocket &socket) const = 0;
  virtual bNodeSocket &update_or_build(bNodeTree &ntree, bNode &node, bNodeSocket &socket) const;

  StringRefNull name() const;
  StringRefNull identifier() const;
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

 public:
  NodeDeclarationBuilder(NodeDeclaration &declaration);

  template<typename DeclType> DeclType &add_input(StringRef name, StringRef identifier = "");
  template<typename DeclType> DeclType &add_output(StringRef name, StringRef identifier = "");
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
inline DeclType &NodeDeclarationBuilder::add_input(StringRef name, StringRef identifier)
{
  static_assert(std::is_base_of_v<SocketDeclaration, DeclType>);
  std::unique_ptr<DeclType> socket_decl = std::make_unique<DeclType>();
  DeclType &ref = *socket_decl;
  ref.name_ = name;
  ref.identifier_ = identifier.is_empty() ? name : identifier;
  declaration_.inputs_.append(std::move(socket_decl));
  return ref;
}

template<typename DeclType>
inline DeclType &NodeDeclarationBuilder::add_output(StringRef name, StringRef identifier)
{
  static_assert(std::is_base_of_v<SocketDeclaration, DeclType>);
  std::unique_ptr<DeclType> socket_decl = std::make_unique<DeclType>();
  DeclType &ref = *socket_decl;
  ref.name_ = name;
  ref.identifier_ = identifier.is_empty() ? name : identifier;
  declaration_.outputs_.append(std::move(socket_decl));
  return ref;
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
