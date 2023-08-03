/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_volume_types.h"

#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_volume.h"

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

VolumeComponent::VolumeComponent() : GeometryComponent(GeometryComponent::Type::Volume) {}

VolumeComponent::~VolumeComponent()
{
  this->clear();
}

GeometryComponent *VolumeComponent::copy() const
{
  VolumeComponent *new_component = new VolumeComponent();
  if (volume_ != nullptr) {
    new_component->volume_ = BKE_volume_copy_for_eval(volume_);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return new_component;
}

void VolumeComponent::clear()
{
  BLI_assert(this->is_mutable() || this->is_expired());
  if (volume_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      BKE_id_free(nullptr, volume_);
    }
    volume_ = nullptr;
  }
}

bool VolumeComponent::has_volume() const
{
  return volume_ != nullptr;
}

void VolumeComponent::replace(Volume *volume, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  volume_ = volume;
  ownership_ = ownership;
}

Volume *VolumeComponent::release()
{
  BLI_assert(this->is_mutable());
  Volume *volume = volume_;
  volume_ = nullptr;
  return volume;
}

const Volume *VolumeComponent::get() const
{
  return volume_;
}

Volume *VolumeComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    volume_ = BKE_volume_copy_for_eval(volume_);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return volume_;
}

bool VolumeComponent::owns_direct_data() const
{
  return ownership_ == GeometryOwnershipType::Owned;
}

void VolumeComponent::ensure_owns_direct_data()
{
  BLI_assert(this->is_mutable());
  if (ownership_ != GeometryOwnershipType::Owned) {
    volume_ = BKE_volume_copy_for_eval(volume_);
    ownership_ = GeometryOwnershipType::Owned;
  }
}

/** \} */

}  // namespace blender::bke
