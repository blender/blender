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

#include "BLI_float4x4.hh"
#include "BLI_map.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "DNA_collection_types.h"

#include "BKE_geometry_set.hh"

using blender::float4x4;
using blender::Map;
using blender::MutableSpan;
using blender::Set;
using blender::Span;

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

InstancesComponent::InstancesComponent() : GeometryComponent(GEO_COMPONENT_TYPE_INSTANCES)
{
}

GeometryComponent *InstancesComponent::copy() const
{
  InstancesComponent *new_component = new InstancesComponent();
  new_component->instance_reference_handles_ = instance_reference_handles_;
  new_component->instance_transforms_ = instance_transforms_;
  new_component->instance_ids_ = instance_ids_;
  new_component->references_ = references_;
  return new_component;
}

void InstancesComponent::reserve(int min_capacity)
{
  instance_reference_handles_.reserve(min_capacity);
  instance_transforms_.reserve(min_capacity);
  instance_ids_.reserve(min_capacity);
}

/**
 * Resize the transform, handles, and ID vectors to the specified capacity.
 *
 * \note This function should be used carefully, only when it's guaranteed
 * that the data will be filled.
 */
void InstancesComponent::resize(int capacity)
{
  instance_reference_handles_.resize(capacity);
  instance_transforms_.resize(capacity);
  instance_ids_.resize(capacity);
}

void InstancesComponent::clear()
{
  instance_reference_handles_.clear();
  instance_transforms_.clear();
  instance_ids_.clear();

  references_.clear();
}

void InstancesComponent::add_instance(const int instance_handle,
                                      const float4x4 &transform,
                                      const int id)
{
  BLI_assert(instance_handle >= 0);
  BLI_assert(instance_handle < references_.size());
  instance_reference_handles_.append(instance_handle);
  instance_transforms_.append(transform);
  instance_ids_.append(id);
}

blender::Span<int> InstancesComponent::instance_reference_handles() const
{
  return instance_reference_handles_;
}

blender::MutableSpan<int> InstancesComponent::instance_reference_handles()
{
  return instance_reference_handles_;
}

blender::MutableSpan<blender::float4x4> InstancesComponent::instance_transforms()
{
  return instance_transforms_;
}
blender::Span<blender::float4x4> InstancesComponent::instance_transforms() const
{
  return instance_transforms_;
}

blender::MutableSpan<int> InstancesComponent::instance_ids()
{
  return instance_ids_;
}
blender::Span<int> InstancesComponent::instance_ids() const
{
  return instance_ids_;
}

/**
 * Returns a handle for the given reference.
 * If the reference exists already, the handle of the existing reference is returned.
 * Otherwise a new handle is added.
 */
int InstancesComponent::add_reference(InstanceReference reference)
{
  return references_.index_of_or_add_as(reference);
}

blender::Span<InstanceReference> InstancesComponent::references() const
{
  return references_;
}

int InstancesComponent::instances_amount() const
{
  return instance_transforms_.size();
}

bool InstancesComponent::is_empty() const
{
  return this->instance_reference_handles_.size() == 0;
}

bool InstancesComponent::owns_direct_data() const
{
  /* The object and collection instances are not direct data. Instance transforms are direct data
   * and are always owned. Therefore, instance components always own all their direct data. */
  return true;
}

void InstancesComponent::ensure_owns_direct_data()
{
  BLI_assert(this->is_mutable());
}

static blender::Array<int> generate_unique_instance_ids(Span<int> original_ids)
{
  using namespace blender;
  Array<int> unique_ids(original_ids.size());

  Set<int> used_unique_ids;
  used_unique_ids.reserve(original_ids.size());
  Vector<int> instances_with_id_collision;
  for (const int instance_index : original_ids.index_range()) {
    const int original_id = original_ids[instance_index];
    if (used_unique_ids.add(original_id)) {
      /* The original id has not been used by another instance yet. */
      unique_ids[instance_index] = original_id;
    }
    else {
      /* The original id of this instance collided with a previous instance, it needs to be looked
       * at again in a second pass. Don't generate a new random id here, because this might collide
       * with other existing ids. */
      instances_with_id_collision.append(instance_index);
    }
  }

  Map<int, RandomNumberGenerator> generator_by_original_id;
  for (const int instance_index : instances_with_id_collision) {
    const int original_id = original_ids[instance_index];
    RandomNumberGenerator &rng = generator_by_original_id.lookup_or_add_cb(original_id, [&]() {
      RandomNumberGenerator rng;
      rng.seed_random(original_id);
      return rng;
    });

    const int max_iteration = 100;
    for (int iteration = 0;; iteration++) {
      /* Try generating random numbers until an unused one has been found. */
      const int random_id = rng.get_int32();
      if (used_unique_ids.add(random_id)) {
        /* This random id is not used by another instance. */
        unique_ids[instance_index] = random_id;
        break;
      }
      if (iteration == max_iteration) {
        /* It seems to be very unlikely that we ever run into this case (assuming there are less
         * than 2^30 instances). However, if that happens, it's better to use an id that is not
         * unique than to be stuck in an infinite loop. */
        unique_ids[instance_index] = original_id;
        break;
      }
    }
  }

  return unique_ids;
}

blender::Span<int> InstancesComponent::almost_unique_ids() const
{
  std::lock_guard lock(almost_unique_ids_mutex_);
  if (almost_unique_ids_.size() != instance_ids_.size()) {
    almost_unique_ids_ = generate_unique_instance_ids(instance_ids_);
  }
  return almost_unique_ids_;
}

/** \} */
