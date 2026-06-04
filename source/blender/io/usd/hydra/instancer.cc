/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "instancer.hh"

#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>

#include "BLI_assert.h"

#include "util.hh"

namespace blender::io::hydra {

pxr::HdContainerDataSourceHandle build_instanced_by_data_source(const pxr::SdfPath &instancer_path,
                                                                const pxr::SdfPath &prototype_root)
{
  return pxr::HdInstancedBySchema::Builder()
      .SetPaths(
          pxr::HdRetainedTypedSampledDataSource<pxr::VtArray<pxr::SdfPath>>::New({instancer_path}))
      .SetPrototypeRoots(
          pxr::HdRetainedTypedSampledDataSource<pxr::VtArray<pxr::SdfPath>>::New({prototype_root}))
      .Build();
}

pxr::HdContainerDataSourceHandle build_instancer_prim_data_source(
    Span<pxr::SdfPath> prototypes,
    Span<pxr::VtIntArray> instance_indices,
    const pxr::VtMatrix4dArray &transforms)
{
  pxr::VtArray<pxr::SdfPath> proto_array(prototypes.begin(), prototypes.end());

  /* Per-prototype subset of top-level instance indices. */
  BLI_assert(!instance_indices.is_empty());
  Vector<pxr::HdDataSourceBaseHandle> idx_values;
  idx_values.reserve(instance_indices.size());
  for (const pxr::VtIntArray &indices : instance_indices) {
    idx_values.append(pxr::HdRetainedTypedSampledDataSource<pxr::VtIntArray>::New(indices));
  }
  pxr::HdVectorDataSourceHandle indices_ds = pxr::HdRetainedSmallVectorDataSource::New(
      idx_values.size(), idx_values.data());

  pxr::HdContainerDataSourceHandle topology =
      pxr::HdInstancerTopologySchema::Builder()
          .SetPrototypes(
              pxr::HdRetainedTypedSampledDataSource<pxr::VtArray<pxr::SdfPath>>::New(proto_array))
          .SetInstanceIndices(indices_ds)
          .Build();

  /* Instance transforms primvar. */
  pxr::HdContainerDataSourceHandle xforms_primvar =
      pxr::HdPrimvarSchema::Builder()
          .SetPrimvarValue(
              pxr::HdRetainedTypedSampledDataSource<pxr::VtMatrix4dArray>::New(transforms))
          .SetInterpolation(pxr::HdPrimvarSchema::BuildInterpolationDataSource(
              pxr::HdPrimvarSchemaTokens->instance))
          .SetRole(pxr::HdPrimvarSchema::BuildRoleDataSource(pxr::HdPrimvarRoleTokens->none))
          .Build();
  pxr::HdContainerDataSourceHandle primvars = pxr::HdRetainedContainerDataSource::New(
      pxr::HdInstancerTokens->instanceTransforms, xforms_primvar);

  pxr::HdContainerDataSourceHandle xform =
      pxr::HdXformSchema::Builder()
          .SetMatrix(
              pxr::HdRetainedTypedSampledDataSource<pxr::GfMatrix4d>::New(pxr::GfMatrix4d(1.0)))
          .SetResetXformStack(pxr::HdRetainedTypedSampledDataSource<bool>::New(true))
          .Build();

  HdContainerBuilder b;
  b.add(pxr::HdInstancerTopologySchema::GetSchemaToken(), topology);
  b.add(pxr::HdPrimvarsSchema::GetSchemaToken(), primvars);
  b.add(pxr::HdXformSchema::GetSchemaToken(), xform);
  return b.build();
}

}  // namespace blender::io::hydra
