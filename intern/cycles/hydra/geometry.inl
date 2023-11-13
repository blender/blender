/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/attribute.h"
#include "hydra/geometry.h"
#include "hydra/instancer.h"
#include "hydra/material.h"
#include "hydra/session.h"
#include "scene/geometry.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "util/hash.h"

#include <pxr/imaging/hd/sceneDelegate.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

extern Transform convert_transform(const GfMatrix4d &matrix);

template<typename Base, typename CyclesBase>
HdCyclesGeometry<Base, CyclesBase>::HdCyclesGeometry(const SdfPath &rprimId
#if PXR_VERSION < 2102
                                                     ,
                                                     const SdfPath &instancerId
#endif
                                                     )
    : Base(rprimId
#if PXR_VERSION < 2102

           ,
           instancerId
#endif
           ),
      _geomTransform(1.0)
{
}

template<typename Base, typename CyclesBase>
void HdCyclesGeometry<Base, CyclesBase>::_InitRepr(const TfToken &reprToken,
                                                   HdDirtyBits *dirtyBits)
{
  TF_UNUSED(reprToken);
  TF_UNUSED(dirtyBits);
}

template<typename Base, typename CyclesBase>
HdDirtyBits HdCyclesGeometry<Base, CyclesBase>::GetInitialDirtyBitsMask() const
{
  return HdChangeTracker::DirtyPrimID | HdChangeTracker::DirtyTransform |
         HdChangeTracker::DirtyMaterialId | HdChangeTracker::DirtyVisibility |
         HdChangeTracker::DirtyInstancer;
}

template<typename Base, typename CyclesBase>
HdDirtyBits HdCyclesGeometry<Base, CyclesBase>::_PropagateDirtyBits(HdDirtyBits bits) const
{
  return bits;
}

template<typename Base, typename CyclesBase>
void HdCyclesGeometry<Base, CyclesBase>::Sync(HdSceneDelegate *sceneDelegate,
                                              HdRenderParam *renderParam,
                                              HdDirtyBits *dirtyBits,
                                              const TfToken &reprToken)
{
  TF_UNUSED(reprToken);

  if (*dirtyBits == HdChangeTracker::Clean) {
    return;
  }

  Initialize(renderParam);

#if PXR_VERSION >= 2102
  Base::_UpdateInstancer(sceneDelegate, dirtyBits);
  HdInstancer::_SyncInstancerAndParents(sceneDelegate->GetRenderIndex(), Base::GetInstancerId());
#endif
  Base::_UpdateVisibility(sceneDelegate, dirtyBits);

  const SceneLock lock(renderParam);

  if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
#if HD_API_VERSION >= 37 && PXR_VERSION >= 2105
    Base::SetMaterialId(sceneDelegate->GetMaterialId(Base::GetId()));
#else
    Base::_SetMaterialId(sceneDelegate->GetRenderIndex().GetChangeTracker(),
                         sceneDelegate->GetMaterialId(Base::GetId()));
#endif

    const auto material = static_cast<const HdCyclesMaterial *>(
        sceneDelegate->GetRenderIndex().GetSprim(HdPrimTypeTokens->material,
                                                 Base::GetMaterialId()));

    array<Node *> usedShaders(1);
    if (material && material->GetCyclesShader()) {
      usedShaders[0] = material->GetCyclesShader();
    }
    else {
      usedShaders[0] = lock.scene->default_surface;
    }

    for (Node *shader : usedShaders) {
      static_cast<Shader *>(shader)->tag_used(lock.scene);
    }

    _geom->set_used_shaders(usedShaders);
  }

  const SdfPath &id = Base::GetId();

  if (HdChangeTracker::IsPrimIdDirty(*dirtyBits, id)) {
    // This needs to be corrected in the AOV
    _instances[0]->set_pass_id(Base::GetPrimId() + 1);
  }

  if (HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
    _geomTransform = sceneDelegate->GetTransform(id);
  }

  if (HdChangeTracker::IsTransformDirty(*dirtyBits, id) ||
      HdChangeTracker::IsInstancerDirty(*dirtyBits, id)) {
    const auto instancer = static_cast<HdCyclesInstancer *>(
        sceneDelegate->GetRenderIndex().GetInstancer(Base::GetInstancerId()));

    // Make sure the first object attribute is the instanceId
    assert(_instances[0]->attributes.size() >= 1 &&
           _instances[0]->attributes.front().name() == HdAovTokens->instanceId.GetString());

    VtMatrix4dArray transforms;
    if (instancer) {
      transforms = instancer->ComputeInstanceTransforms(id);
      _instances[0]->attributes.front() = ParamValue(HdAovTokens->instanceId.GetString(), +0.0f);
    }
    else {
      // Default to a single instance with an identity transform
      transforms.push_back(GfMatrix4d(1.0));
      _instances[0]->attributes.front() = ParamValue(HdAovTokens->instanceId.GetString(), -1.0f);
    }

    const size_t oldSize = _instances.size();
    const size_t newSize = transforms.size();

    // Resize instance list
    for (size_t i = newSize; i < oldSize; ++i) {
      lock.scene->delete_node(_instances[i]);
    }
    _instances.resize(newSize);
    for (size_t i = oldSize; i < newSize; ++i) {
      _instances[i] = lock.scene->create_node<Object>();
      InitializeInstance(static_cast<int>(i));
    }

    // Update transforms of all instances
    for (size_t i = 0; i < transforms.size(); ++i) {
      const float metersPerUnit =
          static_cast<HdCyclesSession *>(renderParam)->GetStageMetersPerUnit();

      const Transform tfm = transform_scale(make_float3(metersPerUnit)) *
                            convert_transform(_geomTransform * transforms[i]);
      _instances[i]->set_tfm(tfm);
    }
  }

  if (HdChangeTracker::IsVisibilityDirty(*dirtyBits, id)) {
    for (Object *instance : _instances) {
      instance->set_visibility(Base::IsVisible() ? ~0 : 0);
    }
  }

  // Must happen after material ID update, so that attribute decisions can be made
  // based on it (e.g. check whether an attribute is actually needed)
  bool rebuild = false;
  Populate(sceneDelegate, *dirtyBits, rebuild);

  if (_geom->is_modified() || rebuild) {
    _geom->tag_update(lock.scene, rebuild);
  }

  for (Object *instance : _instances) {
    instance->tag_update(lock.scene);
  }

  *dirtyBits = HdChangeTracker::Clean;
}

template<typename Base, typename CyclesBase>
void HdCyclesGeometry<Base, CyclesBase>::Finalize(HdRenderParam *renderParam)
{
  if (!_geom && _instances.empty()) {
    return;
  }

  const SceneLock lock(renderParam);
  const bool keep_nodes = static_cast<const HdCyclesSession *>(renderParam)->keep_nodes;

  if (!keep_nodes) {
    lock.scene->delete_node(_geom);
  }
  _geom = nullptr;

  if (!keep_nodes) {
    lock.scene->delete_nodes(set<Object *>(_instances.begin(), _instances.end()));
  }
  _instances.clear();
  _instances.shrink_to_fit();
}

template<typename Base, typename CyclesBase>
void HdCyclesGeometry<Base, CyclesBase>::Initialize(HdRenderParam *renderParam)
{
  if (_geom) {
    return;
  }

  const SceneLock lock(renderParam);

  // Create geometry
  _geom = lock.scene->create_node<CyclesBase>();
  _geom->name = Base::GetId().GetString();

  // Create default instance
  _instances.push_back(lock.scene->create_node<Object>());
  InitializeInstance(0);
}

template<typename Base, typename CyclesBase>
void HdCyclesGeometry<Base, CyclesBase>::InitializeInstance(int index)
{
  Object *instance = _instances[index];
  instance->set_geometry(_geom);

  instance->attributes.emplace_back(HdAovTokens->instanceId.GetString(),
                                    _instances.size() == 1 ? -1.0f : static_cast<float>(index));

  instance->set_color(make_float3(0.8f, 0.8f, 0.8f));
  instance->set_random_id(hash_uint2(hash_string(_geom->name.c_str()), index));
}

template<typename Base, typename CyclesBase>
HdInterpolation HdCyclesGeometry<Base, CyclesBase>::GetPrimvarInterpolation(
    HdSceneDelegate *sceneDelegate, const TfToken &name) const
{
  for (int i = 0; i < HdInterpolationCount; ++i) {
    for (const HdPrimvarDescriptor &desc :
         Base::GetPrimvarDescriptors(sceneDelegate, static_cast<HdInterpolation>(i))) {
      if (desc.name == name) {
        return static_cast<HdInterpolation>(i);
      }
    }
  }

  return HdInterpolationCount;
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
