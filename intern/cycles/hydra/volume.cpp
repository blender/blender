/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/volume.h"
#include "hydra/field.h"
#include "hydra/geometry.inl"
#include "scene/volume.h"

HDCYCLES_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(_tokens,
   (openvdbAsset)
);
// clang-format on

HdCyclesVolume::HdCyclesVolume(const SdfPath &rprimId
#if PXR_VERSION < 2102
                               ,
                               const SdfPath &instancerId
#endif
                               )
    : HdCyclesGeometry(rprimId
#if PXR_VERSION < 2102
                       ,
                       instancerId
#endif
      )
{
}

HdCyclesVolume::~HdCyclesVolume() {}

HdDirtyBits HdCyclesVolume::GetInitialDirtyBitsMask() const
{
  HdDirtyBits bits = HdCyclesGeometry::GetInitialDirtyBitsMask();
  bits |= HdChangeTracker::DirtyVolumeField;
  return bits;
}

void HdCyclesVolume::Populate(HdSceneDelegate *sceneDelegate, HdDirtyBits dirtyBits, bool &rebuild)
{
  Scene *const scene = (Scene *)_geom->get_owner();

  if (dirtyBits & HdChangeTracker::DirtyVolumeField) {
    for (const HdVolumeFieldDescriptor &field : sceneDelegate->GetVolumeFieldDescriptors(GetId()))
    {
      if (const auto openvdbAsset = static_cast<HdCyclesField *>(
              sceneDelegate->GetRenderIndex().GetBprim(_tokens->openvdbAsset, field.fieldId)))
      {
        const ustring name(field.fieldName.GetString());

        AttributeStandard std = ATTR_STD_NONE;
        if (name == Attribute::standard_name(ATTR_STD_VOLUME_DENSITY)) {
          std = ATTR_STD_VOLUME_DENSITY;
        }
        else if (name == Attribute::standard_name(ATTR_STD_VOLUME_COLOR)) {
          std = ATTR_STD_VOLUME_COLOR;
        }
        else if (name == Attribute::standard_name(ATTR_STD_VOLUME_FLAME)) {
          std = ATTR_STD_VOLUME_FLAME;
        }
        else if (name == Attribute::standard_name(ATTR_STD_VOLUME_HEAT)) {
          std = ATTR_STD_VOLUME_HEAT;
        }
        else if (name == Attribute::standard_name(ATTR_STD_VOLUME_TEMPERATURE)) {
          std = ATTR_STD_VOLUME_TEMPERATURE;
        }
        else if (name == Attribute::standard_name(ATTR_STD_VOLUME_VELOCITY)) {
          std = ATTR_STD_VOLUME_VELOCITY;
        }

        // Skip attributes that are not needed
        if ((std != ATTR_STD_NONE && _geom->need_attribute(scene, std)) ||
            _geom->need_attribute(scene, name))
        {
          Attribute *const attr = (std != ATTR_STD_NONE) ?
                                      _geom->attributes.add(std) :
                                      _geom->attributes.add(
                                          name, TypeDesc::TypeFloat, ATTR_ELEMENT_VOXEL);
          attr->data_voxel() = openvdbAsset->GetImageHandle();
        }
      }
    }

    rebuild = true;
  }
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
