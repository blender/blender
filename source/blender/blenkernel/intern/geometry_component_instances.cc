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

#include <mutex>

#include "BLI_float4x4.hh"
#include "BLI_map.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_collection_types.h"

#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"

#include "attribute_access_intern.hh"

using blender::float4x4;
using blender::Map;
using blender::MutableSpan;
using blender::Set;
using blender::Span;
using blender::VectorSet;

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
  if (!instance_ids_.is_empty()) {
    this->instance_ids_ensure();
  }
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
  if (!instance_ids_.is_empty()) {
    this->instance_ids_ensure();
  }
}

void InstancesComponent::clear()
{
  instance_reference_handles_.clear();
  instance_transforms_.clear();
  instance_ids_.clear();

  references_.clear();
}

void InstancesComponent::add_instance(const int instance_handle, const float4x4 &transform)
{
  BLI_assert(instance_handle >= 0);
  BLI_assert(instance_handle < references_.size());
  instance_reference_handles_.append(instance_handle);
  instance_transforms_.append(transform);
  if (!instance_ids_.is_empty()) {
    this->instance_ids_ensure();
  }
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
 * Make sure the ID storage size matches the number of instances. By directly resizing the
 * component's vectors internally, it is possible to be in a situation where the IDs are not
 * empty but they do not have the correct size; this function resolves that.
 */
blender::MutableSpan<int> InstancesComponent::instance_ids_ensure()
{
  instance_ids_.append_n_times(0, this->instances_amount() - instance_ids_.size());
  return instance_ids_;
}

void InstancesComponent::instance_ids_clear()
{
  instance_ids_.clear_and_make_inline();
}

/**
 * With write access to the instances component, the data in the instanced geometry sets can be
 * changed. This is a function on the component rather than each reference to ensure `const`
 * correctness for that reason.
 */
GeometrySet &InstancesComponent::geometry_set_from_reference(const int reference_index)
{
  /* If this assert fails, it means #ensure_geometry_instances must be called first or that the
   * reference can't be converted to a geometry set. */
  BLI_assert(references_[reference_index].type() == InstanceReference::Type::GeometrySet);

  /* The const cast is okay because the instance's hash in the set
   * is not changed by adjusting the data inside the geometry set. */
  return const_cast<GeometrySet &>(references_[reference_index].geometry_set());
}

/**
 * Returns a handle for the given reference.
 * If the reference exists already, the handle of the existing reference is returned.
 * Otherwise a new handle is added.
 */
int InstancesComponent::add_reference(const InstanceReference &reference)
{
  return references_.index_of_or_add_as(reference);
}

blender::Span<InstanceReference> InstancesComponent::references() const
{
  return references_;
}

void InstancesComponent::remove_unused_references()
{
  using namespace blender;
  using namespace blender::bke;

  const int tot_instances = this->instances_amount();
  const int tot_references_before = references_.size();

  if (tot_instances == 0) {
    /* If there are no instances, no reference is needed. */
    references_.clear();
    return;
  }
  if (tot_references_before == 1) {
    /* There is only one reference and at least one instance. So the only existing reference is
     * used. Nothing to do here. */
    return;
  }

  Array<bool> usage_by_handle(tot_references_before, false);
  std::mutex mutex;

  /* Loop over all instances to see which references are used. */
  threading::parallel_for(IndexRange(tot_instances), 1000, [&](IndexRange range) {
    /* Use local counter to avoid lock contention. */
    Array<bool> local_usage_by_handle(tot_references_before, false);

    for (const int i : range) {
      const int handle = instance_reference_handles_[i];
      BLI_assert(handle >= 0 && handle < tot_references_before);
      local_usage_by_handle[handle] = true;
    }

    std::lock_guard lock{mutex};
    for (const int i : IndexRange(tot_references_before)) {
      usage_by_handle[i] |= local_usage_by_handle[i];
    }
  });

  if (!usage_by_handle.as_span().contains(false)) {
    /* All references are used. */
    return;
  }

  /* Create new references and a mapping for the handles. */
  Vector<int> handle_mapping;
  VectorSet<InstanceReference> new_references;
  int next_new_handle = 0;
  bool handles_have_to_be_updated = false;
  for (const int old_handle : IndexRange(tot_references_before)) {
    if (!usage_by_handle[old_handle]) {
      /* Add some dummy value. It won't be read again. */
      handle_mapping.append(-1);
    }
    else {
      const InstanceReference &reference = references_[old_handle];
      handle_mapping.append(next_new_handle);
      new_references.add_new(reference);
      if (old_handle != next_new_handle) {
        handles_have_to_be_updated = true;
      }
      next_new_handle++;
    }
  }
  references_ = new_references;

  if (!handles_have_to_be_updated) {
    /* All remaining handles are the same as before, so they don't have to be updated. This happens
     * when unused handles are only at the end. */
    return;
  }

  /* Update handles of instances. */
  threading::parallel_for(IndexRange(tot_instances), 1000, [&](IndexRange range) {
    for (const int i : range) {
      instance_reference_handles_[i] = handle_mapping[instance_reference_handles_[i]];
    }
  });
}

int InstancesComponent::instances_amount() const
{
  return instance_transforms_.size();
}

int InstancesComponent::references_amount() const
{
  return references_.size();
}

bool InstancesComponent::is_empty() const
{
  return this->instance_reference_handles_.size() == 0;
}

bool InstancesComponent::owns_direct_data() const
{
  for (const InstanceReference &reference : references_) {
    if (!reference.owns_direct_data()) {
      return false;
    }
  }
  return true;
}

void InstancesComponent::ensure_owns_direct_data()
{
  BLI_assert(this->is_mutable());
  for (const InstanceReference &const_reference : references_) {
    /* Const cast is fine because we are not changing anything that would change the hash of the
     * reference. */
    InstanceReference &reference = const_cast<InstanceReference &>(const_reference);
    reference.ensure_owns_direct_data();
  }
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
  if (instance_ids().is_empty()) {
    almost_unique_ids_.reinitialize(this->instances_amount());
    for (const int i : almost_unique_ids_.index_range()) {
      almost_unique_ids_[i] = i;
    }
  }
  else {
    if (almost_unique_ids_.size() != instance_ids_.size()) {
      almost_unique_ids_ = generate_unique_instance_ids(instance_ids_);
    }
  }
  return almost_unique_ids_;
}

int InstancesComponent::attribute_domain_size(const AttributeDomain domain) const
{
  if (domain != ATTR_DOMAIN_POINT) {
    return 0;
  }
  return this->instances_amount();
}

namespace blender::bke {

static float3 get_transform_position(const float4x4 &transform)
{
  return transform.translation();
}

static void set_transform_position(float4x4 &transform, const float3 position)
{
  copy_v3_v3(transform.values[3], position);
}

class InstancePositionAttributeProvider final : public BuiltinAttributeProvider {
 public:
  InstancePositionAttributeProvider()
      : BuiltinAttributeProvider(
            "position", ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, NonCreatable, Writable, NonDeletable)
  {
  }

  GVArrayPtr try_get_for_read(const GeometryComponent &component) const final
  {
    const InstancesComponent &instances_component = static_cast<const InstancesComponent &>(
        component);
    Span<float4x4> transforms = instances_component.instance_transforms();
    return std::make_unique<fn::GVArray_For_DerivedSpan<float4x4, float3, get_transform_position>>(
        transforms);
  }

  GVMutableArrayPtr try_get_for_write(GeometryComponent &component) const final
  {
    InstancesComponent &instances_component = static_cast<InstancesComponent &>(component);
    MutableSpan<float4x4> transforms = instances_component.instance_transforms();
    return std::make_unique<fn::GVMutableArray_For_DerivedSpan<float4x4,
                                                               float3,
                                                               get_transform_position,
                                                               set_transform_position>>(
        transforms);
  }

  bool try_delete(GeometryComponent &UNUSED(component)) const final
  {
    return false;
  }

  bool try_create(GeometryComponent &UNUSED(component),
                  const AttributeInit &UNUSED(initializer)) const final
  {
    return false;
  }

  bool exists(const GeometryComponent &UNUSED(component)) const final
  {
    return true;
  }
};

class InstanceIDAttributeProvider final : public BuiltinAttributeProvider {
 public:
  InstanceIDAttributeProvider()
      : BuiltinAttributeProvider(
            "id", ATTR_DOMAIN_POINT, CD_PROP_INT32, Creatable, Writable, Deletable)
  {
  }

  GVArrayPtr try_get_for_read(const GeometryComponent &component) const final
  {
    const InstancesComponent &instances = static_cast<const InstancesComponent &>(component);
    if (instances.instance_ids().is_empty()) {
      return {};
    }
    return std::make_unique<fn::GVArray_For_Span<int>>(instances.instance_ids());
  }

  GVMutableArrayPtr try_get_for_write(GeometryComponent &component) const final
  {
    InstancesComponent &instances = static_cast<InstancesComponent &>(component);
    if (instances.instance_ids().is_empty()) {
      return {};
    }
    return std::make_unique<fn::GVMutableArray_For_MutableSpan<int>>(instances.instance_ids());
  }

  bool try_delete(GeometryComponent &component) const final
  {
    InstancesComponent &instances = static_cast<InstancesComponent &>(component);
    if (instances.instance_ids().is_empty()) {
      return false;
    }
    instances.instance_ids_clear();
    return true;
  }

  bool try_create(GeometryComponent &component, const AttributeInit &initializer) const final
  {
    InstancesComponent &instances = static_cast<InstancesComponent &>(component);
    if (instances.instances_amount() == 0) {
      return false;
    }
    MutableSpan<int> ids = instances.instance_ids_ensure();
    switch (initializer.type) {
      case AttributeInit::Type::Default: {
        ids.fill(0);
        break;
      }
      case AttributeInit::Type::VArray: {
        const GVArray *varray = static_cast<const AttributeInitVArray &>(initializer).varray;
        varray->materialize_to_uninitialized(IndexRange(varray->size()), ids.data());
        break;
      }
      case AttributeInit::Type::MoveArray: {
        void *source_data = static_cast<const AttributeInitMove &>(initializer).data;
        ids.copy_from({static_cast<int *>(source_data), instances.instances_amount()});
        MEM_freeN(source_data);
        break;
      }
    }
    return true;
  }

  bool exists(const GeometryComponent &component) const final
  {
    const InstancesComponent &instances = static_cast<const InstancesComponent &>(component);
    return !instances.instance_ids().is_empty();
  }
};

static ComponentAttributeProviders create_attribute_providers_for_instances()
{
  static InstancePositionAttributeProvider position;
  static InstanceIDAttributeProvider id;

  return ComponentAttributeProviders({&position, &id}, {});
}
}  // namespace blender::bke

const blender::bke::ComponentAttributeProviders *InstancesComponent::get_attribute_providers()
    const
{
  static blender::bke::ComponentAttributeProviders providers =
      blender::bke::create_attribute_providers_for_instances();
  return &providers;
}

/** \} */
