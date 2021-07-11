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

/** \file
 * \ingroup bli
 *
 * A `blender::MultiValueMap<Key, Value>` is an unordered associative container that stores
 * key-value pairs. It is different from `blender::Map` in that it can store multiple values for
 * the same key. The list of values that corresponds to a specific key can contain duplicates
 * and their order is maintained.
 *
 * This data structure is different from a `std::multi_map`, because multi_map can store the same
 * key more than once and MultiValueMap can't.
 *
 * Currently, this class exists mainly for convenience. There are no performance benefits over
 * using Map<Key, Vector<Value>>. In the future, a better implementation for this data structure
 * can be developed.
 */

#include "BLI_map.hh"
#include "BLI_vector.hh"

namespace blender {

template<typename Key, typename Value> class MultiValueMap {
 public:
  using size_type = int64_t;

 private:
  using MapType = Map<Key, Vector<Value>>;
  MapType map_;

 public:
  /**
   * Add a new value for the given key. If the map contains the key already, the value will be
   * appended to the list of corresponding values.
   */
  void add(const Key &key, const Value &value)
  {
    this->add_as(key, value);
  }
  void add(const Key &key, Value &&value)
  {
    this->add_as(key, std::move(value));
  }
  void add(Key &&key, const Value &value)
  {
    this->add_as(std::move(key), value);
  }
  void add(Key &&key, Value &&value)
  {
    this->add_as(std::move(key), std::move(value));
  }
  template<typename ForwardKey, typename ForwardValue>
  void add_as(ForwardKey &&key, ForwardValue &&value)
  {
    Vector<Value> &vector = map_.lookup_or_add_default_as(std::forward<ForwardKey>(key));
    vector.append(std::forward<ForwardValue>(value));
  }

  void add_non_duplicates(const Key &key, const Value &value)
  {
    Vector<Value> &vector = map_.lookup_or_add_default_as(key);
    vector.append_non_duplicates(value);
  }

  /**
   * Add all given values to the key.
   */
  void add_multiple(const Key &key, Span<Value> values)
  {
    this->add_multiple_as(key, values);
  }
  void add_multiple(Key &&key, Span<Value> values)
  {
    this->add_multiple_as(std::move(key), values);
  }
  template<typename ForwardKey> void add_multiple_as(ForwardKey &&key, Span<Value> values)
  {
    Vector<Value> &vector = map_.lookup_or_add_default_as(std::forward<ForwardKey>(key));
    vector.extend(values);
  }

  /**
   * Get a span to all the values that are stored for the given key.
   */
  Span<Value> lookup(const Key &key) const
  {
    return this->lookup_as(key);
  }
  template<typename ForwardKey> Span<Value> lookup_as(const ForwardKey &key) const
  {
    const Vector<Value> *vector = map_.lookup_ptr_as(key);
    if (vector != nullptr) {
      return vector->as_span();
    }
    return {};
  }

  /**
   * Get a mutable span to all the values that are stored for the given key.
   */
  MutableSpan<Value> lookup(const Key &key)
  {
    return this->lookup_as(key);
  }
  template<typename ForwardKey> MutableSpan<Value> lookup_as(const ForwardKey &key)
  {
    Vector<Value> *vector = map_.lookup_ptr_as(key);
    if (vector != nullptr) {
      return vector->as_mutable_span();
    }
    return {};
  }

  /**
   * NOTE: This signature will change when the implementation changes.
   */
  typename MapType::ItemIterator items() const
  {
    return map_.items();
  }

  /**
   * NOTE: This signature will change when the implementation changes.
   */
  typename MapType::KeyIterator keys() const
  {
    return map_.keys();
  }

  /**
   * NOTE: This signature will change when the implementation changes.
   */
  typename MapType::ValueIterator values() const
  {
    return map_.values();
  }
};

}  // namespace blender
