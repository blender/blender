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
  void append(Timestamp timestamp, const GeometrySet &geometry_set);
  GeometrySet *get_exact(Timestamp timestamp) const;
  GeometrySet *get_before(Timestamp timestamp) const;

 private:
  struct KeyValuePair {
    Timestamp timestamp = TimestampInvalid;
    GeometrySet *geometry_set = nullptr;
  };
  /* Placeholder, just one frame cached for now */
  KeyValuePair last_ = {TimestampInvalid, nullptr};
};
