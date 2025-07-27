/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_type_conversions.hh"
#include "NOD_geometry_nodes_bundle.hh"

namespace blender::bke {

Bundle::Bundle() = default;

Bundle::~Bundle()
{
  for (StoredItem &item : items_) {
    item.type->geometry_nodes_cpp_type->destruct(item.value);
  }
  for (void *buffer : buffers_) {
    MEM_freeN(buffer);
  }
}

Bundle::Bundle(const Bundle &other)
{
  for (const StoredItem &item : other.items_) {
    this->add_new(item.key, *item.type, item.value);
  }
}

Bundle::Bundle(Bundle &&other) noexcept
    : items_(std::move(other.items_)), buffers_(std::move(other.buffers_))
{
}

Bundle &Bundle::operator=(const Bundle &other)
{
  if (this == &other) {
    return *this;
  }
  this->~Bundle();
  new (this) Bundle(other);
  return *this;
}

Bundle &Bundle::operator=(Bundle &&other) noexcept
{
  if (this == &other) {
    return *this;
  }
  this->~Bundle();
  new (this) Bundle(std::move(other));
  return *this;
}

void Bundle::add_new(const StringRef key, const bNodeSocketType &type, const void *value)
{
  BLI_assert(!this->contains(key));
  BLI_assert(type.geometry_nodes_cpp_type);
  const CPPType &cpp_type = *type.geometry_nodes_cpp_type;
  void *buffer = MEM_mallocN_aligned(cpp_type.size, cpp_type.alignment, __func__);
  cpp_type.copy_construct(value, buffer);
  items_.append(StoredItem{std::move(key), &type, buffer});
  buffers_.append(buffer);
}

bool Bundle::add(const StringRef key, const bNodeSocketType &type, const void *value)
{
  if (this->contains(key)) {
    return false;
  }
  this->add_new(key, type, value);
  return true;
}

std::optional<Bundle::Item> Bundle::lookup(const StringRef key) const
{
  for (const StoredItem &item : items_) {
    if (item.key.matches(key)) {
      return Item{item.type, item.value};
    }
  }
  return std::nullopt;
}

bool Bundle::remove(const StringRef key)
{
  const int removed_num = items_.remove_if([&key](StoredItem &item) {
    if (item.key.matches(key)) {
      item.type->geometry_nodes_cpp_type->destruct(item.value);
      return true;
    }
    return false;
  });
  return removed_num >= 1;
}

bool Bundle::contains(const StringRef key) const
{
  for (const StoredItem &item : items_) {
    if (item.key.matches(key)) {
      return true;
    }
  }
  return false;
}

void Bundle::delete_self()
{
  MEM_delete(this);
}

}  // namespace blender::bke
