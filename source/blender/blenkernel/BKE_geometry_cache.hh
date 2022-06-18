/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <limits>

/** \file
 * \ingroup bke
 */

struct GeometrySet;

struct GeometryCache {
 public:
  using Timestamp = int;

  static const int TimestampInvalid = INT_MIN;

  GeometryCache();
  ~GeometryCache();

  GeometryCache(GeometryCache &other) = default;
  GeometryCache(GeometryCache &&other) = default;

  GeometryCache &operator=(GeometryCache &other) = default;
  GeometryCache &operator=(GeometryCache &&other) = default;

  void clear();
  void insert_and_continue_from(Timestamp timestamp, const GeometrySet &geometry_set);
  GeometrySet *get_exact(Timestamp timestamp) const;
  GeometrySet *get_before(Timestamp timestamp) const;

 private:
  struct KeyValuePair {
    Timestamp timestamp = TimestampInvalid;
    GeometrySet *geometry_set = nullptr;

    bool is_valid() const
    {
      return timestamp != TimestampInvalid;
    }

    operator bool() const
    {
      return is_valid();
    }
  };
  /* Placeholder, just two frames cached for now */
  KeyValuePair first_ = {TimestampInvalid, nullptr};
  KeyValuePair last_ = {TimestampInvalid, nullptr};
};
