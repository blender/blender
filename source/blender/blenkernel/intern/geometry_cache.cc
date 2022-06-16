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
  MEM_delete(last_.geometry_set);
  last_ = KeyValuePair{};
}

void GeometryCache::append(Timestamp timestamp, const GeometrySet &geometry_set)
{
  BLI_assert(timestamp != TimestampInvalid);

  MEM_delete(last_.geometry_set);
  last_ = KeyValuePair{timestamp, MEM_new<GeometrySet>("geometry set", geometry_set)};
}

GeometrySet *GeometryCache::get_exact(Timestamp timestamp) const
{
  BLI_assert(timestamp != TimestampInvalid);

  if (last_.timestamp == timestamp) {
    return last_.geometry_set;
  }
  return nullptr;
}

GeometrySet *GeometryCache::get_before(Timestamp timestamp) const
{
  BLI_assert(timestamp != TimestampInvalid);

  if (last_.timestamp < timestamp) {
    return last_.geometry_set;
  }
  return nullptr;
}

/** \} */
