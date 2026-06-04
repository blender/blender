/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "populate_context.hh"

#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>

#include "BLI_hash.hh"
#include "BLI_string.h"

#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "BKE_material.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "instancer.hh"
#include "material.hh"
#include "util.hh"

namespace blender::io::hydra {

/* -------------------------------------------------------------------- */
/** \name BObjectInfo / EmittedGeometryKey
 * \{ */

bool BObjectInfo::is_real_object_data() const
{
  return real_object && static_cast<const ID *>(real_object->data) == object_data;
}

uint64_t EmittedGeometryKey::hash() const
{
  return get_default_hash(id, type);
}

static bool object_disables_geometry_sharing(Scene *scene, eEvaluationMode mode, Object *ob)
{
  /* Metaballs are merged together. */
  if (ob->type == OB_MBALL) {
    return true;
  }
  /* Modifiers can create different geometry per object. */
  const int settings = (mode == DAG_EVAL_VIEWPORT) ? eModifierMode_Realtime : eModifierMode_Render;
  if ((BKE_object_is_modified(scene, ob) & settings) != 0) {
    return true;
  }
  /* Object-level material links are difficult to share. */
  const int mat_count = BKE_object_material_count_eval(ob);
  for (int i = 0; i < mat_count; i++) {
    if (i < ob->totcol && ob->matbits && ob->matbits[i] != 0) {
      return true;
    }
  }
  return false;
}

EmittedGeometryKey PopulateContext::emitted_geometry_key(const BObjectInfo &info,
                                                         const pxr::TfToken &type) const
{
  Scene *scene_ptr = scene;
  const eEvaluationMode mode = DEG_get_mode(depsgraph);
  const bool can_share = info.is_real_object_data() &&
                         !object_disables_geometry_sharing(scene_ptr, mode, info.real_object);
  const ID *key_id = can_share ? info.object_data : &info.real_object->id;
  return EmittedGeometryKey{key_id, type};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Path naming scheme for each prim
 * \{ */

pxr::SdfPath PopulateContext::object_prim_id(const Object *object) const
{
  char name[32];
  SNPRINTF(name, "O_%p", &object->id);
  return root_path.AppendElementString(name);
}

pxr::SdfPath PopulateContext::submesh_prim_id(const Object *object, const int submesh_index) const
{
  char name[16];
  SNPRINTF(name, "SM_%04d", submesh_index);
  return object_prim_id(object).AppendElementString(name);
}

pxr::SdfPath PopulateContext::hair_prim_id(const Object *object, const ParticleSystem *psys) const
{
  char name[64];
  SNPRINTF(name, "O_%p_PS_%p", &object->id, psys);
  return root_path.AppendElementString(name);
}

pxr::SdfPath PopulateContext::material_prim_id(const Material *material) const
{
  char name[32];
  SNPRINTF(name, "M_%p", &material->id);
  return root_path.AppendElementString(name);
}

pxr::SdfPath PopulateContext::instancer_prim_id() const
{
  return root_path.AppendElementString("Instancer");
}

pxr::SdfPath PopulateContext::instancer_proto_root(const Object *source) const
{
  char name[32];
  SNPRINTF(name, "O_%p", &source->id);
  return instancer_prim_id().AppendElementString(name);
}

pxr::SdfPath PopulateContext::instancer_proto_submesh(const Object *source,
                                                      const int submesh_index) const
{
  char name[16];
  SNPRINTF(name, "SM_%04d", submesh_index);
  return instancer_proto_root(source).AppendElementString(name);
}

pxr::SdfPath PopulateContext::instance_clone_prim_id(const Object *source,
                                                     const int instance_index) const
{
  char name[16];
  SNPRINTF(name, "NM_%08d", instance_index);
  return instancer_proto_root(source).AppendElementString(name);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Emit
 * \{ */

void PopulateContext::emit_prim(const pxr::SdfPath &path,
                                const pxr::TfToken &type,
                                const pxr::HdContainerDataSourceHandle &ds)
{
  new_paths.add(path);
  to_add.push_back({path, type, ds});
  if (emitted_paths.contains(path)) {
    to_dirty.push_back({path, pxr::HdDataSourceLocator()});
  }
}

void PopulateContext::emit_object_prim(EmittedObject &emitted,
                                       const pxr::SdfPath &path,
                                       const pxr::TfToken &type,
                                       const pxr::HdContainerDataSourceHandle &ds,
                                       const pxr::GfMatrix4d &geometry_xform)
{
  EmittedObjectPrim entry;
  entry.path = path;
  entry.type = type;
  entry.base_data_source = ds;
  entry.geometry_xform = geometry_xform;
  emitted.prims.append(entry);
  emit_prim(path, type, ds);
}

pxr::HdContainerDataSourceHandle EmittedObjectPrim::compose() const
{
  if (xform_overlay) {
    return pxr::HdOverlayContainerDataSource::New(xform_overlay, base_data_source);
  }
  return base_data_source;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Materials
 * \{ */

const EmittedMaterial *PopulateContext::get_or_create_material(const Material *material)
{
  if (!material) {
    return nullptr;
  }
  used_emitted_materials.add(material);
  if (const EmittedMaterial *existing = emitted_materials.lookup_ptr(material)) {
    return existing;
  }
  new_emitted_materials.append(material);
  emitted_materials.add_new(material, build_emitted_material(*this, material));
  return emitted_materials.lookup_ptr(material);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Instances and Prototypes
 * \{ */

pxr::HdContainerDataSourceHandle PopulateContext::proto_instanced_by(const Object *source) const
{
  return pxr::HdRetainedContainerDataSource::New(
      pxr::HdInstancedBySchema::GetSchemaToken(),
      build_instanced_by_data_source(instancer_prim_id(), instancer_proto_root(source)));
}

void PopulateContext::add_instance(const Object *source, const float dupli_mat[4][4])
{
  const int instance_idx = int(instance_transforms.size());
  instance_transforms.push_back(gf_matrix_from_transform(dupli_mat));
  const IndexRange range = proto_range_by_source.lookup(source);
  for (const int proto_idx : range) {
    per_proto_indices[proto_idx].push_back(instance_idx);
  }
}

pxr::HdContainerDataSourceHandle compose_gprim_data_source(
    const EmittedGeometryPrim &gprim,
    const pxr::GfMatrix4d &transform,
    const bool visible,
    const pxr::HdContainerDataSourceHandle &instanced_by_overlay)
{
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
  b.reserve(5);
  b.add(gprim.schema_token, gprim.geometry);
  b.add(pxr::HdPrimvarsSchema::GetSchemaToken(), gprim.primvars);
  b.add(pxr::HdXformSchema::GetSchemaToken(), xform);
  b.add(pxr::HdVisibilitySchema::GetSchemaToken(), visibility);
  if (gprim.bindings) {
    b.add(pxr::HdMaterialBindingsSchema::GetSchemaToken(), gprim.bindings);
  }
  pxr::HdContainerDataSourceHandle prim_ds = b.build();

  if (instanced_by_overlay) {
    return pxr::HdOverlayContainerDataSource::New(prim_ds, instanced_by_overlay);
  }
  return prim_ds;
}

/** \} */

}  // namespace blender::io::hydra
