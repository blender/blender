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

#include "DNA_volume_types.h"

#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_volume.h"

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

VolumeComponent::VolumeComponent() : GeometryComponent(GEO_COMPONENT_TYPE_VOLUME)
{
}

VolumeComponent::~VolumeComponent()
{
  this->clear();
}

GeometryComponent *VolumeComponent::copy() const
{
  VolumeComponent *new_component = new VolumeComponent();
  if (volume_ != nullptr) {
    new_component->volume_ = BKE_volume_copy_for_eval(volume_, false);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return new_component;
}

void VolumeComponent::clear()
{
  BLI_assert(this->is_mutable());
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

/* Clear the component and replace it with the new volume. */
void VolumeComponent::replace(Volume *volume, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  volume_ = volume;
  ownership_ = ownership;
}

/* Return the volume and clear the component. The caller takes over responsibility for freeing the
 * volume (if the component was responsible before). */
Volume *VolumeComponent::release()
{
  BLI_assert(this->is_mutable());
  Volume *volume = volume_;
  volume_ = nullptr;
  return volume;
}

/* Get the volume from this component. This method can be used by multiple threads at the same
 * time. Therefore, the returned volume should not be modified. No ownership is transferred. */
const Volume *VolumeComponent::get_for_read() const
{
  return volume_;
}

/* Get the volume from this component. This method can only be used when the component is mutable,
 * i.e. it is not shared. The returned volume can be modified. No ownership is transferred. */
Volume *VolumeComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    volume_ = BKE_volume_copy_for_eval(volume_, false);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return volume_;
}

/** \} */
