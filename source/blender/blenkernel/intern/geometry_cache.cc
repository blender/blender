/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_cache.h"
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
  MEM_delete(first_.geometry_set);
  first_ = KeyValuePair{};
  MEM_delete(last_.geometry_set);
  last_ = KeyValuePair{};
}

void GeometryCache::insert_and_continue_from(Timestamp timestamp, const GeometrySet &geometry_set)
{
  BLI_assert(timestamp != TimestampInvalid);

  if (!first_ || timestamp <= first_.timestamp) {
    MEM_delete(first_.geometry_set);
    first_ = KeyValuePair{timestamp, MEM_new<GeometrySet>("geometry set", geometry_set)};
    first_.geometry_set->ensure_owns_direct_data();
    MEM_delete(last_.geometry_set);
    last_ = KeyValuePair{};
  }
  else if (!last_ || timestamp <= last_.timestamp) {
    MEM_delete(last_.geometry_set);
    last_ = KeyValuePair{timestamp, MEM_new<GeometrySet>("geometry set", geometry_set)};
    last_.geometry_set->ensure_owns_direct_data();
  }
  else {
    MEM_delete(first_.geometry_set);
    first_ = last_;
    last_ = KeyValuePair{timestamp, MEM_new<GeometrySet>("geometry set", geometry_set)};
    last_.geometry_set->ensure_owns_direct_data();
  }
}

GeometrySet *GeometryCache::get_exact(Timestamp timestamp) const
{
  BLI_assert(timestamp != TimestampInvalid);

  if (last_ && last_.timestamp == timestamp) {
    return last_.geometry_set;
  }
  if (first_ && first_.timestamp == timestamp) {
    return first_.geometry_set;
  }
  return nullptr;
}

GeometrySet *GeometryCache::get_before(Timestamp timestamp) const
{
  BLI_assert(timestamp != TimestampInvalid);

  if (last_ && last_.timestamp < timestamp) {
    return last_.geometry_set;
  }
  if (first_ && first_.timestamp < timestamp) {
    return first_.geometry_set;
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name C API
 * \{ */

GeometryCache *BKE_geometry_cache_new()
{
  return MEM_new<GeometryCache>("geometry cache");
}

void BKE_geometry_cache_free(GeometryCache *cache)
{
  MEM_delete(cache);
}

void BKE_geometry_cache_insert_and_continue_from(GeometryCache *cache,
                               int cfra,
                               const GeometrySet *geometry_set)
{
  cache->insert_and_continue_from(GeometryCache::Timestamp{cfra}, *geometry_set);
}

/** \} */
