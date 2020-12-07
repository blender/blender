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

#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"

#include "FN_generic_pointer.hh"

namespace blender::fn {

/**
 * This is a map that stores key-value-pairs. What makes it special is that the type of values does
 * not have to be known at compile time. There just has to be a corresponding CPPType.
 */
template<typename Key> class GValueMap {
 private:
  /* Used to allocate values owned by this container. */
  LinearAllocator<> &allocator_;
  Map<Key, GMutablePointer> values_;

 public:
  GValueMap(LinearAllocator<> &allocator) : allocator_(allocator)
  {
  }

  ~GValueMap()
  {
    /* Destruct all values that are still in the map. */
    for (GMutablePointer value : values_.values()) {
      value.destruct();
    }
  }

  /* Add a value to the container. The container becomes responsible for destructing the value that
   * is passed in. The caller remains responsible for freeing the value after it has been
   * destructed. */
  template<typename ForwardKey> void add_new_direct(ForwardKey &&key, GMutablePointer value)
  {
    values_.add_new_as(std::forward<ForwardKey>(key), value);
  }

  /* Add a value to the container that is move constructed from the given value. The caller remains
   * responsible for destructing and freeing the given value. */
  template<typename ForwardKey> void add_new_by_move(ForwardKey &&key, GMutablePointer value)
  {
    const CPPType &type = *value.type();
    void *buffer = allocator_.allocate(type.size(), type.alignment());
    type.move_to_uninitialized(value.get(), buffer);
    values_.add_new_as(std::forward<ForwardKey>(key), GMutablePointer{type, buffer});
  }

  /* Add a value to the container that is copy constructed from the given value. The caller remains
   * responsible for destructing and freeing the given value. */
  template<typename ForwardKey> void add_new_by_copy(ForwardKey &&key, GMutablePointer value)
  {
    const CPPType &type = *value.type();
    void *buffer = allocator_.allocate(type.size(), type.alignment());
    type.copy_to_uninitialized(value.get(), buffer);
    values_.add_new_as(std::forward<ForwardKey>(key), GMutablePointer{type, buffer});
  }

  /* Add a value to the container. */
  template<typename ForwardKey, typename T> void add_new(ForwardKey &&key, T &&value)
  {
    if constexpr (std::is_rvalue_reference_v<T>) {
      this->add_new_by_move(std::forward<ForwardKey>(key), &value);
    }
    else {
      this->add_new_by_copy(std::forward<ForwardKey>(key), &value);
    }
  }

  /* Remove the value for the given name from the container and remove it. The caller is
   * responsible for freeing it. The lifetime of the referenced memory might be bound to lifetime
   * of the container. */
  template<typename ForwardKey> GMutablePointer extract(const ForwardKey &key)
  {
    return values_.pop_as(key);
  }

  /* Remove the value for the given name from the container and remove it. */
  template<typename T, typename ForwardKey> T extract(const ForwardKey &key)
  {
    GMutablePointer value = values_.pop_as(key);
    const CPPType &type = *value.type();
    BLI_assert(type.is<T>());
    T return_value;
    type.relocate_to_initialized(value.get(), &return_value);
    return return_value;
  }

  template<typename T, typename ForwardKey> T lookup(const ForwardKey &key) const
  {
    GMutablePointer value = values_.lookup_as(key);
    const CPPType &type = *value.type();
    BLI_assert(type.is<T>());
    T return_value;
    type.copy_to_initialized(value.get(), &return_value);
    return return_value;
  }

  template<typename ForwardKey> bool contains(const ForwardKey &key) const
  {
    return values_.contains_as(key);
  }
};

}  // namespace blender::fn
