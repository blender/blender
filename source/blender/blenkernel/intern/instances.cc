/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_listbase.h"
#include "BLI_rand.hh"
#include "BLI_task.hh"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"

#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_instances.hh"

#include "attribute_storage_access.hh"

namespace blender::bke {

InstanceReference::InstanceReference(GeometrySet geometry_set)
    : type_(Type::GeometrySet),
      geometry_set_(std::make_unique<GeometrySet>(std::move(geometry_set)))
{
}

InstanceReference::InstanceReference(const InstanceReference &other)
    : type_(other.type_), data_(other.data_)
{
  if (other.geometry_set_) {
    geometry_set_ = std::make_unique<GeometrySet>(*other.geometry_set_);
  }
}

void InstanceReference::ensure_owns_direct_data()
{
  if (type_ != Type::GeometrySet) {
    return;
  }
  geometry_set_->ensure_owns_direct_data();
}

bool InstanceReference::owns_direct_data() const
{
  if (type_ != Type::GeometrySet) {
    /* The object and collection instances are not direct data. */
    return true;
  }
  return geometry_set_->owns_direct_data();
}

void InstanceReference::count_memory(MemoryCounter &memory) const
{
  switch (type_) {
    case Type::GeometrySet: {
      geometry_set_->count_memory(memory);
    }
    default: {
      break;
    }
  }
}

AttributeAccessor Instances::attributes() const
{
  return AttributeAccessor(this, instance_attribute_accessor_functions());
}

MutableAttributeAccessor Instances::attributes_for_write()
{
  return MutableAttributeAccessor(this, instance_attribute_accessor_functions());
}

static void convert_collection_to_instances(const Collection &collection,
                                            bke::Instances &instances)
{
  LISTBASE_FOREACH (CollectionChild *, collection_child, &collection.children) {
    float4x4 transform = float4x4::identity();
    transform.location() += float3(collection_child->collection->instance_offset);
    transform.location() -= float3(collection.instance_offset);
    const int handle = instances.add_reference(*collection_child->collection);
    instances.add_instance(handle, transform);
  }

  LISTBASE_FOREACH (CollectionObject *, collection_object, &collection.gobject) {
    float4x4 transform = float4x4::identity();
    transform.location() -= float3(collection.instance_offset);
    transform *= (collection_object->ob)->object_to_world();
    const int handle = instances.add_reference(*collection_object->ob);
    instances.add_instance(handle, transform);
  }
}

void InstanceReference::to_geometry_set(GeometrySet &r_geometry_set) const
{
  r_geometry_set.clear();
  switch (type_) {
    case Type::Object: {
      const Object &object = this->object();
      r_geometry_set = bke::object_get_evaluated_geometry_set(object);
      break;
    }
    case Type::Collection: {
      const Collection &collection = this->collection();
      std::unique_ptr<bke::Instances> instances_ptr = std::make_unique<bke::Instances>();
      convert_collection_to_instances(collection, *instances_ptr);
      r_geometry_set.replace_instances(instances_ptr.release());
      break;
    }
    case Type::GeometrySet: {
      r_geometry_set = this->geometry_set();
      break;
    }
    case Type::None: {
      break;
    }
  }
}

StringRefNull InstanceReference::name() const
{
  switch (type_) {
    case Type::Object:
      return this->object().id.name + 2;
    case Type::Collection:
      return this->collection().id.name + 2;
    case Type::GeometrySet:
      return this->geometry_set().name;
    case Type::None:
      break;
  }
  return "";
}

bool operator==(const InstanceReference &a, const InstanceReference &b)
{
  if (a.geometry_set_ && b.geometry_set_) {
    return *a.geometry_set_ == *b.geometry_set_;
  }
  return a.type_ == b.type_ && a.data_ == b.data_;
}

uint64_t InstanceReference::hash() const
{
  const uint64_t geometry_hash = geometry_set_ ? geometry_set_->hash() : 0;
  return get_default_hash(geometry_hash, type_, data_);
}

Instances::Instances() = default;

Instances::Instances(Instances &&other)
    : references_(std::move(other.references_)),
      instances_num_(other.instances_num_),
      attributes_(std::move(other.attributes_)),
      reference_user_counts_(std::move(other.reference_user_counts_)),
      unique_ids_cache_(std::move(other.unique_ids_cache_))
{
}

Instances::Instances(const Instances &other)
    : references_(other.references_),
      instances_num_(other.instances_num_),
      attributes_(other.attributes_),
      reference_user_counts_(other.reference_user_counts_),
      unique_ids_cache_(other.unique_ids_cache_)
{
}

Instances::~Instances() = default;

Instances &Instances::operator=(const Instances &other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) Instances(other);
  return *this;
}

Instances &Instances::operator=(Instances &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) Instances(std::move(other));
  return *this;
}

void Instances::resize(int capacity)
{
  const int old_size = this->instances_num();
  attributes_.resize(AttrDomain::Instance, capacity);
  instances_num_ = capacity;
  if (capacity > old_size) {
    fill_attribute_range_default(this->attributes_for_write(),
                                 AttrDomain::Instance,
                                 {},
                                 IndexRange::from_begin_end(old_size, capacity));
  }
}

void Instances::add_instance(const int instance_handle, const float4x4 &transform)
{
  BLI_assert(instance_handle >= 0);
  BLI_assert(instance_handle < references_.size());
  instances_num_++;
  attributes_.resize(AttrDomain::Instance, instances_num_);
  this->reference_handles_for_write().last() = instance_handle;
  this->transforms_for_write().last() = transform;
  this->tag_reference_handles_changed();
}

Span<int> Instances::reference_handles() const
{
  return get_span_attribute<int>(
             attributes_, AttrDomain::Instance, ".reference_index", instances_num_)
      .value_or(Span<int>());
}

MutableSpan<int> Instances::reference_handles_for_write()
{
  return get_mutable_attribute<int>(
      attributes_, AttrDomain::Instance, ".reference_index", instances_num_);
}

Span<float4x4> Instances::transforms() const
{
  return get_span_attribute<float4x4>(
             attributes_, AttrDomain::Instance, "instance_transform", instances_num_)
      .value_or(Span<float4x4>());
}

MutableSpan<float4x4> Instances::transforms_for_write()
{
  return get_mutable_attribute<float4x4>(
      attributes_, AttrDomain::Instance, "instance_transform", instances_num_);
}

GeometrySet &Instances::geometry_set_from_reference(const int reference_index)
{
  /* If this assert fails, it means #ensure_geometry_instances must be called first or that the
   * reference can't be converted to a geometry set. */
  BLI_assert(references_[reference_index].type() == InstanceReference::Type::GeometrySet);

  return references_[reference_index].geometry_set();
}

std::optional<int> Instances::find_reference_handle(const InstanceReference &query)
{
  for (const int i : references_.index_range()) {
    const InstanceReference &reference = references_[i];
    if (reference == query) {
      return i;
    }
  }
  return std::nullopt;
}

int Instances::add_reference(const InstanceReference &reference)
{
  if (std::optional<int> handle = this->find_reference_handle(reference)) {
    return *handle;
  }
  return this->add_new_reference(reference);
}

int Instances::add_new_reference(const InstanceReference &reference)
{
  this->tag_reference_handles_changed();
  return references_.append_and_get_index(reference);
}

Span<InstanceReference> Instances::references() const
{
  return references_;
}

MutableSpan<InstanceReference> Instances::references_for_write()
{
  return references_;
}

void Instances::remove(const IndexMask &mask, const AttributeFilter &attribute_filter)
{
  const std::optional<IndexRange> masked_range = mask.to_range();
  if (masked_range.has_value() && masked_range->start() == 0) {
    /* Deleting from the end of the array can be much faster since no data has to be shifted. */
    this->resize(mask.size());
    this->remove_unused_references();
    return;
  }

  Instances new_instances;
  new_instances.references_ = std::move(references_);
  new_instances.instances_num_ = mask.size();

  gather_attributes(this->attributes(),
                    AttrDomain::Instance,
                    AttrDomain::Instance,
                    attribute_filter,
                    mask,
                    new_instances.attributes_for_write());

  *this = std::move(new_instances);

  this->remove_unused_references();
}

void Instances::remove_unused_references()
{
  const int tot_instances = instances_num_;
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

  const Span<int> reference_handles = this->reference_handles();

  Array<bool> usage_by_handle(tot_references_before, false);
  Mutex mutex;

  /* Loop over all instances to see which references are used. */
  threading::parallel_for(IndexRange(tot_instances), 1000, [&](IndexRange range) {
    /* Use local counter to avoid lock contention. */
    Array<bool> local_usage_by_handle(tot_references_before, false);

    for (const int i : range) {
      const int handle = reference_handles[i];
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
  Vector<InstanceReference> new_references;
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
      new_references.append(reference);
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
  {
    const MutableSpan<int> reference_handles = this->reference_handles_for_write();
    threading::parallel_for(IndexRange(tot_instances), 1000, [&](IndexRange range) {
      for (const int i : range) {
        reference_handles[i] = handle_mapping[reference_handles[i]];
      }
    });
  }
}

int Instances::instances_num() const
{
  return this->instances_num_;
}

int Instances::references_num() const
{
  return references_.size();
}

bool Instances::owns_direct_data() const
{
  for (const InstanceReference &reference : references_) {
    if (!reference.owns_direct_data()) {
      return false;
    }
  }
  return true;
}

void Instances::ensure_owns_direct_data()
{
  for (const InstanceReference &const_reference : references_) {
    /* `const` cast is fine because we are not changing anything that would change the hash of the
     * reference. */
    InstanceReference &reference = const_cast<InstanceReference &>(const_reference);
    reference.ensure_owns_direct_data();
  }
}

void Instances::count_memory(MemoryCounter &memory) const
{
  attributes_.count_memory(memory);
  for (const InstanceReference &reference : references_) {
    reference.count_memory(memory);
  }
}

static Array<int> generate_unique_instance_ids(Span<int> original_ids)
{
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
        /* The likelihood of running into this case is very low even if there is a huge number of
         * instances. For correctness, it's still good to systematically find an unused id instead
         * of purely relying on randomness. */
        for (const int generated_id : IndexRange(INT32_MAX)) {
          if (used_unique_ids.add(generated_id)) {
            unique_ids[instance_index] = generated_id;
            break;
          }
        }
        break;
      }
    }
  }

  return unique_ids;
}

Span<int> Instances::reference_user_counts() const
{
  reference_user_counts_.ensure([&](Array<int> &r_data) {
    const int references_num = references_.size();
    r_data.reinitialize(references_num);
    r_data.fill(0);

    const Span<int> handles = this->reference_handles();
    for (const int handle : handles) {
      if (handle >= 0 && handle < references_num) {
        r_data[handle]++;
      }
    }
  });
  return reference_user_counts_.data();
}

Span<int> Instances::unique_ids() const
{
  unique_ids_cache_.ensure([&](Array<int> &r_data) {
    const VArraySpan<int> instance_ids = *this->attributes().lookup<int>("id");
    if (instance_ids.is_empty()) {
      r_data.reinitialize(instances_num_);
      array_utils::fill_index_range(r_data.as_mutable_span());
      return;
    }
    r_data = generate_unique_instance_ids(instance_ids);
  });
  return unique_ids_cache_.data();
}

static float3 get_transform_position(const float4x4 &transform)
{
  return transform.location();
}

static void set_transform_position(float4x4 &transform, const float3 position)
{
  transform.location() = position;
}

VArray<float3> instance_position_varray(const Instances &instances)
{
  return VArray<float3>::from_derived_span<float4x4, get_transform_position>(
      instances.transforms());
}

VMutableArray<float3> instance_position_varray_for_write(Instances &instances)
{
  MutableSpan<float4x4> transforms = instances.transforms_for_write();
  return VMutableArray<float3>::
      from_derived_span<float4x4, get_transform_position, set_transform_position>(transforms);
}

}  // namespace blender::bke
