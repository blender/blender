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

#include "BKE_anonymous_attribute.hh"

using namespace blender::bke;

/**
 * A struct that identifies an attribute. It's lifetime is managed by an atomic reference count.
 *
 * Additionally, this struct can be strongly or weakly owned. The difference is that strong
 * ownership means that attributes with this id will be kept around. Weak ownership just makes sure
 * that the struct itself stays alive, but corresponding attributes might still be removed
 * automatically.
 */
struct AnonymousAttributeID {
  /**
   * Total number of references to this attribute id. Once this reaches zero, the struct can be
   * freed. This includes strong and weak references.
   */
  mutable std::atomic<int> refcount_tot = 0;

  /**
   * Number of strong references to this attribute id. When this is zero, the corresponding
   * attributes can be removed from geometries automatically.
   */
  mutable std::atomic<int> refcount_strong = 0;

  /**
   * Only used to identify this struct in a debugging session.
   */
  std::string debug_name;

  /**
   * Unique name of the this attribute id during the current session.
   */
  std::string internal_name;
};

/** Every time this function is called, it outputs a different name. */
static std::string get_new_internal_name()
{
  static std::atomic<int> index = 0;
  const int next_index = index.fetch_add(1);
  return "anonymous_attribute_" + std::to_string(next_index);
}

AnonymousAttributeID *BKE_anonymous_attribute_id_new_weak(const char *debug_name)
{
  AnonymousAttributeID *anonymous_id = new AnonymousAttributeID();
  anonymous_id->debug_name = debug_name;
  anonymous_id->internal_name = get_new_internal_name();
  anonymous_id->refcount_tot.store(1);
  return anonymous_id;
}

AnonymousAttributeID *BKE_anonymous_attribute_id_new_strong(const char *debug_name)
{
  AnonymousAttributeID *anonymous_id = new AnonymousAttributeID();
  anonymous_id->debug_name = debug_name;
  anonymous_id->internal_name = get_new_internal_name();
  anonymous_id->refcount_tot.store(1);
  anonymous_id->refcount_strong.store(1);
  return anonymous_id;
}

bool BKE_anonymous_attribute_id_has_strong_references(const AnonymousAttributeID *anonymous_id)
{
  return anonymous_id->refcount_strong.load() >= 1;
}

void BKE_anonymous_attribute_id_increment_weak(const AnonymousAttributeID *anonymous_id)
{
  anonymous_id->refcount_tot.fetch_add(1);
}

void BKE_anonymous_attribute_id_increment_strong(const AnonymousAttributeID *anonymous_id)
{
  anonymous_id->refcount_tot.fetch_add(1);
  anonymous_id->refcount_strong.fetch_add(1);
}

void BKE_anonymous_attribute_id_decrement_weak(const AnonymousAttributeID *anonymous_id)
{
  const int new_refcount = anonymous_id->refcount_tot.fetch_sub(1) - 1;
  if (new_refcount == 0) {
    BLI_assert(anonymous_id->refcount_strong == 0);
    delete anonymous_id;
  }
}

void BKE_anonymous_attribute_id_decrement_strong(const AnonymousAttributeID *anonymous_id)
{
  anonymous_id->refcount_strong.fetch_sub(1);
  BKE_anonymous_attribute_id_decrement_weak(anonymous_id);
}

const char *BKE_anonymous_attribute_id_debug_name(const AnonymousAttributeID *anonymous_id)
{
  return anonymous_id->debug_name.c_str();
}

const char *BKE_anonymous_attribute_id_internal_name(const AnonymousAttributeID *anonymous_id)
{
  return anonymous_id->internal_name.c_str();
}
