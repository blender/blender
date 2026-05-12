/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <variant>

#include "BLI_set.hh"
#include "BLI_vector_set.hh"

#include "NOD_bundle_type_fwd.hh"
#include "NOD_geometry_nodes_bundle_signature.hh"
#include "NOD_node_declaration.hh"

namespace blender::nodes {

class FlatBundleType {
 public:
  struct Item {
    std::unique_ptr<SocketDeclaration> decl;

    UString name() const;
  };

 private:
  struct ItemNameGetter {
    UString operator()(const Item &item)
    {
      return item.decl->name;
    }
  };

  std::string name_;
  CustomIDVectorSet<Item, ItemNameGetter> items_;

 public:
  FlatBundleType(std::string name, Vector<std::unique_ptr<SocketDeclaration>> decls);

  StringRefNull name() const;

  Span<Item> items() const;
  const SocketDeclaration *find_decl(const UString name) const;

  BundleSignature to_bundle_signature() const;
};

class NestedBundleType {
 private:
  struct ItemNameGetter {
    std::string operator()(const FlatBundleTypePtr &item)
    {
      return item->name();
    }
  };

  std::string name_;
  CustomIDVectorSet<FlatBundleTypePtr, ItemNameGetter> items_;

 public:
  NestedBundleType(std::string name, Vector<FlatBundleTypePtr> bundle_types);

  StringRefNull name() const;
  Span<FlatBundleTypePtr> items() const;
};

class BundleType {
 public:
  std::variant<FlatBundleTypePtr, NestedBundleTypePtr> type;

  BundleType(FlatBundleTypePtr type) : type(std::move(type)) {}
  BundleType(NestedBundleTypePtr type) : type(std::move(type)) {}

  StringRefNull name() const;

  uint64_t hash() const;

  friend bool operator==(const BundleType &a, const BundleType &b) = default;
};

class FlatBundleTypeBuilder {
 private:
  std::string name_;
  Vector<std::unique_ptr<BaseSocketDeclarationBuilder>> builders_;
  Vector<std::unique_ptr<SocketDeclaration>> decls_;

 public:
  FlatBundleTypeBuilder(std::string name);

  template<typename DeclType> typename DeclType::Builder &add(UString name);

  FlatBundleTypePtr build();
};

class BundleTypeRegistry {
  Map<std::string, Set<BundleType>> types_;

 public:
  static void register_type(BundleType bundle_type);
  static FlatBundleTypePtr try_find_single_flat(StringRef name);
  static Vector<std::string> get_all_flat_type_names();
};

template<typename DeclType>
inline typename DeclType::Builder &FlatBundleTypeBuilder::add(UString name)
{
  static_assert(std::is_base_of_v<SocketDeclaration, DeclType>);
  using SocketBuilder = typename DeclType::Builder;

  auto decl_ptr = std::make_unique<DeclType>();
  DeclType &decl = *decl_ptr;

  auto decl_builder_ptr = std::make_unique<SocketBuilder>();
  SocketBuilder &decl_builder = *decl_builder_ptr;

  decl_builder.decl_ = &decl;
  decl_builder.decl_base_ = &decl;

  decl.name = name;
  decl.identifier = name;
  decl.in_out = SOCK_IN;
  decl.socket_type = DeclType::static_socket_type;

  decls_.append(std::move(decl_ptr));
  builders_.append(std::move(decl_builder_ptr));
  return decl_builder;
}

inline UString FlatBundleType::Item::name() const
{
  return this->decl->name;
}

inline StringRefNull FlatBundleType::name() const
{
  return name_;
}

inline Span<FlatBundleType::Item> FlatBundleType::items() const
{
  return items_;
}

inline StringRefNull NestedBundleType::name() const
{
  return name_;
}

inline Span<FlatBundleTypePtr> NestedBundleType::items() const
{
  return items_;
}

inline StringRefNull BundleType::name() const
{
  return std::visit([](auto &&type) { return type->name(); }, this->type);
}

inline uint64_t BundleType::hash() const
{
  return std::visit([](auto &&type) { return get_default_hash(type); }, this->type);
}

}  // namespace blender::nodes
