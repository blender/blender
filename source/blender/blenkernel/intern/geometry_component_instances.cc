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
  new_component->transforms_ = transforms_;
  new_component->instanced_data_ = instanced_data_;
  return new_component;
}

void InstancesComponent::clear()
{
  instanced_data_.clear();
  transforms_.clear();
}

void InstancesComponent::add_instance(Object *object, float4x4 transform, const int id)
{
  InstancedData data;
  data.type = INSTANCE_DATA_TYPE_OBJECT;
  data.data.object = object;
  this->add_instance(data, transform, id);
}

void InstancesComponent::add_instance(Collection *collection, float4x4 transform, const int id)
{
  InstancedData data;
  data.type = INSTANCE_DATA_TYPE_COLLECTION;
  data.data.collection = collection;
  this->add_instance(data, transform, id);
}

void InstancesComponent::add_instance(InstancedData data, float4x4 transform, const int id)
{
  instanced_data_.append(data);
  transforms_.append(transform);
  ids_.append(id);
}

Span<InstancedData> InstancesComponent::instanced_data() const
{
  return instanced_data_;
}

Span<float4x4> InstancesComponent::transforms() const
{
  return transforms_;
}

Span<int> InstancesComponent::ids() const
{
  return ids_;
}

MutableSpan<float4x4> InstancesComponent::transforms()
{
  return transforms_;
}

int InstancesComponent::instances_amount() const
{
  const int size = instanced_data_.size();
  BLI_assert(transforms_.size() == size);
  return size;
}

bool InstancesComponent::is_empty() const
{
  return transforms_.size() == 0;
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
  if (almost_unique_ids_.size() != ids_.size()) {
    almost_unique_ids_ = generate_unique_instance_ids(ids_);
  }
  return almost_unique_ids_;
}

/** \} */
