/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <atomic>

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_vector.hh"

namespace blender::ed::spreadsheet {

/**
 * A generic cache for the spreadsheet. Different data sources can cache custom data using custom
 * keys.
 *
 * Elements are removed from the cache when they are not used during a redraw.
 */
class SpreadsheetCache {
 public:
  class Key {
   public:
    virtual ~Key() = default;

    mutable bool is_used = false;

    virtual uint64_t hash() const = 0;

    friend bool operator==(const Key &a, const Key &b)
    {
      return a.is_equal_to(b);
    }

   private:
    virtual bool is_equal_to(const Key &other) const = 0;
  };

  class Value {
   public:
    virtual ~Value() = default;
  };

 private:
  Vector<std::unique_ptr<Key>> keys_;
  Map<std::reference_wrapper<const Key>, std::unique_ptr<Value>> cache_map_;

 public:
  /* Adding or looking up a key tags it as being used, so that it won't be removed. */
  void add(std::unique_ptr<Key> key, std::unique_ptr<Value> value);
  Value *lookup(const Key &key);
  Value &lookup_or_add(std::unique_ptr<Key> key,
                       FunctionRef<std::unique_ptr<Value>()> create_value);

  void set_all_unused();
  void remove_all_unused();

  template<typename T> T &lookup_or_add(std::unique_ptr<Key> key)
  {
    return dynamic_cast<T &>(
        this->lookup_or_add(std::move(key), []() { return std::make_unique<T>(); }));
  }
};

}  // namespace blender::ed::spreadsheet
