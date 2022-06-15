/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_cache.hh"
#include "BKE_geometry_set.hh"

/* -------------------------------------------------------------------- */
/** \name Geometry Cache
 * \{ */

GeometryCache::GeometryCache()
{
}

GeometryCache::~GeometryCache()
{
  clear();
}

void GeometryCache::clear()
{
  MEM_delete(first_);
  first_ = nullptr;
  MEM_delete(last_);
  last_ = nullptr;
}

void GeometryCache::append(const GeometrySet &geometry_set)
{
  MEM_delete(first_);
  first_ = last_;
  last_ = MEM_new<GeometrySet>("geometry set", geometry_set);
}

/** \} */
