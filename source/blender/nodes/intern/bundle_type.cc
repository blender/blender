/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_bundle_type.hh"
#include "NOD_geometry_nodes_bundle.hh"

namespace blender::nodes {

FlatBundleType::FlatBundleType(std::string name, Vector<std::unique_ptr<SocketDeclaration>> decls)
    : name_(std::move(name))
{
  for (std::unique_ptr<SocketDeclaration> &decl : decls) {
    items_.add_new(Item{std::move(decl)});
  }
}

const SocketDeclaration *FlatBundleType::find_decl(const UString name) const
{
  const Item *item = items_.lookup_key_ptr_as(name);
  if (!item) {
    return nullptr;
  }
  return item->decl.get();
}

FlatBundleTypeBuilder::FlatBundleTypeBuilder(std::string name) : name_(std::move(name)) {}

FlatBundleTypePtr FlatBundleTypeBuilder::build()
{
  return std::make_shared<const FlatBundleType>(std::move(name_), std::move(decls_));
}

BundleSignature FlatBundleType::to_bundle_signature() const
{
  BundleSignature signature;
  signature.add(Bundle::type_item_name.string(), SOCK_STRING);
  for (const Item &item : items_) {
    signature.add(item.name().ref(), item.decl->socket_type);
  }
  return signature;
}

NestedBundleType::NestedBundleType(std::string name, Vector<FlatBundleTypePtr> bundle_types)
    : name_(std::move(name))
{
  for (FlatBundleTypePtr &bundle_type : bundle_types) {
    items_.add_new(std::move(bundle_type));
  }
}

static BundleTypeRegistry &get_bundle_type_registry()
{
  static BundleTypeRegistry singleton;
  return singleton;
}

FlatBundleTypePtr BundleTypeRegistry::try_find_single_flat(const StringRef name)
{
  const BundleTypeRegistry &registry = get_bundle_type_registry();
  const Set<BundleType> *types_with_name = registry.types_.lookup_ptr(name);
  if (!types_with_name) {
    return nullptr;
  }
  if (types_with_name->size() != 1) {
    return nullptr;
  }
  const BundleType &bundle_type = *types_with_name->begin();
  if (const auto *flat_bundle_type = std::get_if<FlatBundleTypePtr>(&bundle_type.type)) {
    return *flat_bundle_type;
  }
  return nullptr;
}

Vector<std::string> BundleTypeRegistry::get_all_flat_type_names()
{
  const BundleTypeRegistry &registry = get_bundle_type_registry();
  Vector<std::string> names;
  for (const auto &[name, types] : registry.types_.items()) {
    if (std::any_of(types.begin(), types.end(), [](const BundleType &type) {
          return std::holds_alternative<FlatBundleTypePtr>(type.type);
        }))
    {
      names.append(name);
    }
  }
  return names;
}

void BundleTypeRegistry::register_type(BundleType bundle_type)
{
  BundleTypeRegistry &registry = get_bundle_type_registry();
  registry.types_.lookup_or_add_default(bundle_type.name()).add(bundle_type);
}

}  // namespace blender::nodes
