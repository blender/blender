/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_rand.hh"
#include "BLI_task.hh"

#include "BKE_attribute_math.hh"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"

namespace blender::bke {

InstanceReference::InstanceReference(GeometrySet geometry_set)
    : type_(Type::GeometrySet),
      geometry_set_(std::make_unique<GeometrySet>(std::move(geometry_set)))
{
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

Instances::Instances(const Instances &other)
    : references_(other.references_),
      reference_handles_(other.reference_handles_),
      transforms_(other.transforms_),
      almost_unique_ids_(other.almost_unique_ids_),
      attributes_(other.attributes_)
{
}

void Instances::reserve(int min_capacity)
{
  reference_handles_.reserve(min_capacity);
  transforms_.reserve(min_capacity);
  attributes_.reallocate(min_capacity);
}

void Instances::resize(int capacity)
{
  reference_handles_.resize(capacity);
  transforms_.resize(capacity);
  attributes_.reallocate(capacity);
}

void Instances::add_instance(const int instance_handle, const float4x4 &transform)
{
  BLI_assert(instance_handle >= 0);
  BLI_assert(instance_handle < references_.size());
  reference_handles_.append(instance_handle);
  transforms_.append(transform);
  attributes_.reallocate(this->instances_num());
}

blender::Span<int> Instances::reference_handles() const
{
  return reference_handles_;
}

blender::MutableSpan<int> Instances::reference_handles()
{
  return reference_handles_;
}

blender::MutableSpan<blender::float4x4> Instances::transforms()
{
  return transforms_;
}
blender::Span<blender::float4x4> Instances::transforms() const
{
  return transforms_;
}

GeometrySet &Instances::geometry_set_from_reference(const int reference_index)
{
  /* If this assert fails, it means #ensure_geometry_instances must be called first or that the
   * reference can't be converted to a geometry set. */
  BLI_assert(references_[reference_index].type() == InstanceReference::Type::GeometrySet);

  /* The const cast is okay because the instance's hash in the set
   * is not changed by adjusting the data inside the geometry set. */
  return const_cast<GeometrySet &>(references_[reference_index].geometry_set());
}

int Instances::add_reference(const InstanceReference &reference)
{
  return references_.index_of_or_add_as(reference);
}

blender::Span<InstanceReference> Instances::references() const
{
  return references_;
}

void Instances::remove(const IndexMask mask,
                       const AnonymousAttributePropagationInfo &propagation_info)
{
  using namespace blender;
  if (mask.is_range() && mask.as_range().start() == 0) {
    /* Deleting from the end of the array can be much faster since no data has to be shifted. */
    this->resize(mask.size());
    this->remove_unused_references();
    return;
  }

  const Span<int> old_handles = this->reference_handles();
  Vector<int> new_handles(mask.size());
  array_utils::gather(old_handles, mask.indices(), new_handles.as_mutable_span());
  reference_handles_ = std::move(new_handles);

  const Span<float4x4> old_tansforms = this->transforms();
  Vector<float4x4> new_transforms(mask.size());
  array_utils::gather(old_tansforms, mask.indices(), new_transforms.as_mutable_span());
  transforms_ = std::move(new_transforms);

  const bke::CustomDataAttributes &src_attributes = attributes_;

  bke::CustomDataAttributes dst_attributes;
  dst_attributes.reallocate(mask.size());

  src_attributes.foreach_attribute(
      [&](const bke::AttributeIDRef &id, const bke::AttributeMetaData &meta_data) {
        if (id.is_anonymous() && !propagation_info.propagate(id.anonymous_id())) {
          return true;
        }

        GSpan src = *src_attributes.get_for_read(id);
        dst_attributes.create(id, meta_data.data_type);
        GMutableSpan dst = *dst_attributes.get_for_write(id);
        array_utils::gather(src, mask.indices(), dst);

        return true;
      },
      ATTR_DOMAIN_INSTANCE);

  attributes_ = std::move(dst_attributes);
  this->remove_unused_references();
}

void Instances::remove_unused_references()
{
  using namespace blender;
  using namespace blender::bke;

  const int tot_instances = this->instances_num();
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
      const int handle = reference_handles_[i];
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
      reference_handles_[i] = handle_mapping[reference_handles_[i]];
    }
  });
}

int Instances::instances_num() const
{
  return transforms_.size();
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

blender::Span<int> Instances::almost_unique_ids() const
{
  std::lock_guard lock(almost_unique_ids_mutex_);
  std::optional<GSpan> instance_ids_gspan = attributes_.get_for_read("id");
  if (instance_ids_gspan) {
    Span<int> instance_ids = instance_ids_gspan->typed<int>();
    if (almost_unique_ids_.size() != instance_ids.size()) {
      almost_unique_ids_ = generate_unique_instance_ids(instance_ids);
    }
  }
  else {
    almost_unique_ids_.reinitialize(this->instances_num());
    for (const int i : almost_unique_ids_.index_range()) {
      almost_unique_ids_[i] = i;
    }
  }
  return almost_unique_ids_;
}

}  // namespace blender::bke
