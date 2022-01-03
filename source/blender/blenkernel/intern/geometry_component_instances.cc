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
#include "BLI_index_mask.hh"
#include "BLI_map.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_collection_types.h"

#include "BKE_attribute_access.hh"
#include "BKE_attribute_math.hh"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"

#include "attribute_access_intern.hh"

#include "FN_cpp_type_make.hh"

using blender::float4x4;
using blender::IndexMask;
using blender::Map;
using blender::MutableSpan;
using blender::Set;
using blender::Span;
using blender::VectorSet;
using blender::fn::GSpan;

MAKE_CPP_TYPE(InstanceReference, InstanceReference, CPPTypeFlags::None)

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
  new_component->references_ = references_;
  new_component->attributes_ = attributes_;
  return new_component;
}

void InstancesComponent::reserve(int min_capacity)
{
  instance_reference_handles_.reserve(min_capacity);
  instance_transforms_.reserve(min_capacity);
  attributes_.reallocate(min_capacity);
}

void InstancesComponent::resize(int capacity)
{
  instance_reference_handles_.resize(capacity);
  instance_transforms_.resize(capacity);
  attributes_.reallocate(capacity);
}

void InstancesComponent::clear()
{
  instance_reference_handles_.clear();
  instance_transforms_.clear();
  attributes_.clear();
  references_.clear();
}

void InstancesComponent::add_instance(const int instance_handle, const float4x4 &transform)
{
  BLI_assert(instance_handle >= 0);
  BLI_assert(instance_handle < references_.size());
  instance_reference_handles_.append(instance_handle);
  instance_transforms_.append(transform);
  attributes_.reallocate(this->instances_amount());
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

GeometrySet &InstancesComponent::geometry_set_from_reference(const int reference_index)
{
  /* If this assert fails, it means #ensure_geometry_instances must be called first or that the
   * reference can't be converted to a geometry set. */
  BLI_assert(references_[reference_index].type() == InstanceReference::Type::GeometrySet);

  /* The const cast is okay because the instance's hash in the set
   * is not changed by adjusting the data inside the geometry set. */
  return const_cast<GeometrySet &>(references_[reference_index].geometry_set());
}

int InstancesComponent::add_reference(const InstanceReference &reference)
{
  return references_.index_of_or_add_as(reference);
}

blender::Span<InstanceReference> InstancesComponent::references() const
{
  return references_;
}

template<typename T>
static void copy_data_based_on_mask(Span<T> src, MutableSpan<T> dst, IndexMask mask)
{
  BLI_assert(src.data() != dst.data());
  using namespace blender;
  threading::parallel_for(mask.index_range(), 1024, [&](IndexRange range) {
    for (const int i : range) {
      dst[i] = src[mask[i]];
    }
  });
}

void InstancesComponent::remove_instances(const IndexMask mask)
{
  using namespace blender;
  if (mask.is_range() && mask.as_range().start() == 0) {
    /* Deleting from the end of the array can be much faster since no data has to be shifted. */
    this->resize(mask.size());
    this->remove_unused_references();
    return;
  }

  Vector<int> new_handles(mask.size());
  copy_data_based_on_mask<int>(this->instance_reference_handles(), new_handles, mask);
  instance_reference_handles_ = std::move(new_handles);
  Vector<float4x4> new_transforms(mask.size());
  copy_data_based_on_mask<float4x4>(this->instance_transforms(), new_transforms, mask);
  instance_transforms_ = std::move(new_transforms);

  const bke::CustomDataAttributes &src_attributes = attributes_;

  bke::CustomDataAttributes dst_attributes;
  dst_attributes.reallocate(mask.size());

  src_attributes.foreach_attribute(
      [&](const bke::AttributeIDRef &id, const AttributeMetaData &meta_data) {
        if (!id.should_be_kept()) {
          return true;
        }

        GSpan src = *src_attributes.get_for_read(id);
        dst_attributes.create(id, meta_data.data_type);
        fn::GMutableSpan dst = *dst_attributes.get_for_write(id);

        attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
          using T = decltype(dummy);
          copy_data_based_on_mask<T>(src.typed<T>(), dst.typed<T>(), mask);
        });
        return true;
      },
      ATTR_DOMAIN_INSTANCE);

  attributes_ = std::move(dst_attributes);
  this->remove_unused_references();
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
  std::optional<GSpan> instance_ids_gspan = attributes_.get_for_read("id");
  if (instance_ids_gspan) {
    Span<int> instance_ids = instance_ids_gspan->typed<int>();
    if (almost_unique_ids_.size() != instance_ids.size()) {
      almost_unique_ids_ = generate_unique_instance_ids(instance_ids);
    }
  }
  else {
    almost_unique_ids_.reinitialize(this->instances_amount());
    for (const int i : almost_unique_ids_.index_range()) {
      almost_unique_ids_[i] = i;
    }
  }
  return almost_unique_ids_;
}

int InstancesComponent::attribute_domain_size(const AttributeDomain domain) const
{
  if (domain != ATTR_DOMAIN_INSTANCE) {
    return 0;
  }
  return this->instances_amount();
}

blender::bke::CustomDataAttributes &InstancesComponent::attributes()
{
  return this->attributes_;
}

const blender::bke::CustomDataAttributes &InstancesComponent::attributes() const
{
  return this->attributes_;
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
            "position", ATTR_DOMAIN_INSTANCE, CD_PROP_FLOAT3, NonCreatable, Writable, NonDeletable)
  {
  }

  GVArray try_get_for_read(const GeometryComponent &component) const final
  {
    const InstancesComponent &instances_component = static_cast<const InstancesComponent &>(
        component);
    Span<float4x4> transforms = instances_component.instance_transforms();
    return VArray<float3>::ForDerivedSpan<float4x4, get_transform_position>(transforms);
  }

  WriteAttributeLookup try_get_for_write(GeometryComponent &component) const final
  {
    InstancesComponent &instances_component = static_cast<InstancesComponent &>(component);
    MutableSpan<float4x4> transforms = instances_component.instance_transforms();
    return {VMutableArray<float3>::ForDerivedSpan<float4x4,
                                                  get_transform_position,
                                                  set_transform_position>(transforms),
            domain_};
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

template<typename T>
static GVArray make_array_read_attribute(const void *data, const int domain_size)
{
  return VArray<T>::ForSpan(Span<T>((const T *)data, domain_size));
}

template<typename T>
static GVMutableArray make_array_write_attribute(void *data, const int domain_size)
{
  return VMutableArray<T>::ForSpan(MutableSpan<T>((T *)data, domain_size));
}

static ComponentAttributeProviders create_attribute_providers_for_instances()
{
  static InstancePositionAttributeProvider position;
  static CustomDataAccessInfo instance_custom_data_access = {
      [](GeometryComponent &component) -> CustomData * {
        InstancesComponent &inst = static_cast<InstancesComponent &>(component);
        return &inst.attributes().data;
      },
      [](const GeometryComponent &component) -> const CustomData * {
        const InstancesComponent &inst = static_cast<const InstancesComponent &>(component);
        return &inst.attributes().data;
      },
      nullptr};

  /**
   * IDs of the instances. They are used for consistency over multiple frames for things like
   * motion blur. Proper stable ID data that actually helps when rendering can only be generated
   * in some situations, so this vector is allowed to be empty, in which case the index of each
   * instance will be used for the final ID.
   */
  static BuiltinCustomDataLayerProvider id("id",
                                           ATTR_DOMAIN_INSTANCE,
                                           CD_PROP_INT32,
                                           CD_PROP_INT32,
                                           BuiltinAttributeProvider::Creatable,
                                           BuiltinAttributeProvider::Writable,
                                           BuiltinAttributeProvider::Deletable,
                                           instance_custom_data_access,
                                           make_array_read_attribute<int>,
                                           make_array_write_attribute<int>,
                                           nullptr);

  static CustomDataAttributeProvider instance_custom_data(ATTR_DOMAIN_INSTANCE,
                                                          instance_custom_data_access);

  return ComponentAttributeProviders({&position, &id}, {&instance_custom_data});
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
