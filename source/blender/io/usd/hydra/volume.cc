/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "volume.hh"

#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/volumeFieldBindingSchema.h>
#include <pxr/imaging/hd/volumeFieldSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usdImaging/usdVolImaging/tokens.h>

#include "BLI_index_range.hh"

#include "BKE_material.hh"
#include "BKE_volume.hh"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_volume_types.h"

#include "populate_context.hh"
#include "util.hh"
#include "volume_modifier.hh"

namespace blender::io::hydra {

/* Bprim data source for one VDB field. */
static pxr::HdContainerDataSourceHandle build_volume_field_data_source(
    const std::string &filepath, const pxr::TfToken &field_name)
{
  return pxr::HdRetainedContainerDataSource::New(
      pxr::HdVolumeFieldSchema::GetSchemaToken(),
      pxr::HdVolumeFieldSchema::Builder()
          .SetFilePath(pxr::HdRetainedTypedSampledDataSource<pxr::SdfAssetPath>::New(
              pxr::SdfAssetPath(filepath, filepath)))
          .SetFieldName(pxr::HdRetainedTypedSampledDataSource<pxr::TfToken>::New(field_name))
          .SetFieldIndex(pxr::HdRetainedTypedSampledDataSource<int>::New(0))
          .Build());
}

/* Volume Rprim data source. */
static pxr::HdContainerDataSourceHandle build_volume_prim_data_source(
    const Span<VolumeFieldDescriptor> fields,
    const pxr::GfMatrix4d &transform,
    const bool visible,
    const pxr::SdfPath &material_path)
{
  HdContainerBuilder field_bindings;
  field_bindings.reserve(fields.size());
  for (const VolumeFieldDescriptor &f : fields) {
    field_bindings.add(f.name, f.field_path);
  }
  pxr::HdContainerDataSourceHandle bindings = pxr::HdVolumeFieldBindingSchema::BuildRetained(
      field_bindings.names.size(), field_bindings.names.data(), field_bindings.values.data());

  pxr::HdContainerDataSourceHandle xform =
      pxr::HdXformSchema::Builder()
          .SetMatrix(pxr::HdRetainedTypedSampledDataSource<pxr::GfMatrix4d>::New(transform))
          .SetResetXformStack(pxr::HdRetainedTypedSampledDataSource<bool>::New(false))
          .Build();
  pxr::HdContainerDataSourceHandle visibility =
      pxr::HdVisibilitySchema::Builder()
          .SetVisibility(pxr::HdRetainedTypedSampledDataSource<bool>::New(visible))
          .Build();

  HdContainerBuilder b;
  b.add(pxr::HdVolumeFieldBindingSchema::GetSchemaToken(), bindings);
  b.add(pxr::HdXformSchema::GetSchemaToken(), xform);
  b.add(pxr::HdVisibilitySchema::GetSchemaToken(), visibility);

  if (!material_path.IsEmpty()) {
    pxr::HdContainerDataSourceHandle binding =
        pxr::HdMaterialBindingSchema::Builder()
            .SetPath(pxr::HdRetainedTypedSampledDataSource<pxr::SdfPath>::New(material_path))
            .Build();
    pxr::HdContainerDataSourceHandle bindings_ds = pxr::HdRetainedContainerDataSource::New(
        pxr::HdMaterialBindingsSchemaTokens->allPurpose, binding);
    b.add(pxr::HdMaterialBindingsSchema::GetSchemaToken(), bindings_ds);
  }
  return b.build();
}

/* Resolve the VDB file path and per-grid fields for a volume datablock. */
static std::string build_volume_fields_from_datablock(const Object *object,
                                                      const pxr::SdfPath &volume_path,
                                                      Main *bmain,
                                                      pxr::GfMatrix4d *r_transform,
                                                      Vector<VolumeFieldDescriptor> *r_fields)
{
  Volume *volume = id_cast<Volume *>(object->data);
  if (!BKE_volume_load(volume, bmain)) {
    return {};
  }
  std::string filepath = BKE_volume_grids_frame_filepath(volume);
  if (volume->runtime->grids) {
    const int num_grids = BKE_volume_num_grids(volume);
    r_fields->reserve(num_grids);
    for (const int i : IndexRange(num_grids)) {
      const bke::VolumeGridData *grid = BKE_volume_grid_get(volume, i);
      const std::string grid_name = bke::volume_grid::get_name(*grid);
      VolumeFieldDescriptor f;
      f.name = pxr::TfToken(grid_name);
      f.field_path = volume_path.AppendElementString("VF_" + grid_name);
      r_fields->append(f);
    }
  }
  *r_transform = gf_matrix_from_transform(object->object_to_world().ptr());
  BKE_volume_unload(volume);
  return filepath;
}

bool emit_volume_object(PopulateContext &ctx, const Object *object, EmittedObject &emitted)
{
  const FluidModifierData *fmd = fluid_gas_domain_modifier(object, ctx.depsgraph);
  const bool is_volume_data = (object->type == OB_VOLUME);
  if (!is_volume_data && !fmd) {
    return false;
  }

  Vector<VolumeFieldDescriptor> fields;
  std::string filepath;
  pxr::GfMatrix4d geometry_xform(1.0);
  const pxr::SdfPath volume_path = ctx.object_prim_id(object);

  if (is_volume_data) {
    pxr::GfMatrix4d ignored(1.0);
    filepath = build_volume_fields_from_datablock(
        object, volume_path, ctx.bmain, &ignored, &fields);
  }
  else {
    filepath = build_volume_fields_from_modifier(
        object, fmd, ctx.scene->r.cfra, volume_path, &geometry_xform, &fields);
  }

  /* Volume datablock with no grids: nothing to publish. */
  if (fields.is_empty()) {
    return true;
  }

  /* TODO: support multi-material volumes. */
  const Material *material = (BKE_object_material_count_eval(object) > 0) ?
                                 BKE_object_material_get_eval(const_cast<Object *>(object), 1) :
                                 nullptr;
  const EmittedMaterial *mat_entry = ctx.get_or_create_material(material);
  const pxr::SdfPath material_path = mat_entry ? mat_entry->path : pxr::SdfPath();

  const pxr::GfMatrix4d transform = geometry_xform *
                                    gf_matrix_from_transform(object->object_to_world().ptr());
  pxr::HdContainerDataSourceHandle prim_ds = build_volume_prim_data_source(
      fields.as_span(), transform, true, material_path);
  ctx.emit_object_prim(
      emitted, volume_path, pxr::HdPrimTypeTokens->volume, prim_ds, geometry_xform);

  for (const VolumeFieldDescriptor &f : fields) {
    pxr::HdContainerDataSourceHandle field_ds = build_volume_field_data_source(filepath, f.name);
    ctx.emit_object_prim(emitted, f.field_path, pxr::UsdVolImagingTokens->openvdbAsset, field_ds);
  }
  return true;
}

void emit_volume_dupli(PopulateContext &ctx, const Object *source, const float dupli_mat[4][4])
{
  const int idx = ctx.nonmesh_instance_count.lookup_default(source, 0);
  ctx.nonmesh_instance_count.add_overwrite(source, idx + 1);
  const pxr::SdfPath volume_path = ctx.instance_clone_prim_id(source, idx);
  Vector<VolumeFieldDescriptor> fields;
  pxr::GfMatrix4d unused(1.0);
  const std::string filepath = build_volume_fields_from_datablock(
      source, volume_path, ctx.bmain, &unused, &fields);
  if (fields.is_empty()) {
    return;
  }
  const Material *material = (BKE_object_material_count_eval(source) > 0) ?
                                 BKE_object_material_get_eval(const_cast<Object *>(source), 1) :
                                 nullptr;
  const EmittedMaterial *mat_entry = ctx.get_or_create_material(material);
  const pxr::SdfPath material_path = mat_entry ? mat_entry->path : pxr::SdfPath();
  const pxr::GfMatrix4d transform = gf_matrix_from_transform(dupli_mat);
  pxr::HdContainerDataSourceHandle prim_ds = build_volume_prim_data_source(
      fields.as_span(), transform, true, material_path);
  ctx.emit_prim(volume_path, pxr::HdPrimTypeTokens->volume, prim_ds);
  for (const VolumeFieldDescriptor &f : fields) {
    pxr::HdContainerDataSourceHandle field_ds = build_volume_field_data_source(filepath, f.name);
    ctx.emit_prim(f.field_path, pxr::UsdVolImagingTokens->openvdbAsset, field_ds);
  }
}

}  // namespace blender::io::hydra
