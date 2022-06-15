/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct GeometrySet;

class GeometryCache {
 public:
  GeometryCache();
  ~GeometryCache();

  GeometryCache(GeometryCache &other) = default;
  GeometryCache(GeometryCache &&other) = default;

  GeometryCache &operator=(GeometryCache &other) = default;
  GeometryCache &operator=(GeometryCache &&other) = default;

  void clear();
  void append(const GeometrySet &geometry_set);
  GeometrySet *last() const
  {
    return last_;
  }

 private:
  /* Placeholders */
  GeometrySet *first_ = nullptr;
  GeometrySet *last_ = nullptr;
};
