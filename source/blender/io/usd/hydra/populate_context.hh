/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/usd/sdf/path.h>

#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "scene_index.hh"

namespace blender {

struct Depsgraph;
struct Main;
struct Material;
struct Object;
struct ParticleSystem;
struct Scene;
struct View3D;

struct ID;

namespace io::hydra {

/* Uniquely describes objects in a way that works with geometry set instances too. */
struct BObjectInfo {
  Object *iter_object;
  Object *real_object;
  ID *object_data;

  bool is_real_object_data() const;
};

/* Temporary state for one populate() update, to pass context and track what
 * was add and used. */
struct PopulateContext {
  PopulateContext(Depsgraph *depsgraph,
                  Main *bmain,
                  Scene *scene,
                  const View3D *view3d,
                  const pxr::SdfPath &root_path,
                  bool use_materialx,
                  Span<pxr::TfToken> material_render_contexts,
                  Span<pxr::TfToken> shader_source_types,
                  Map<const Material *, EmittedMaterial> &emitted_materials,
                  Map<EmittedGeometryKey, EmittedGeometry> &emitted_geometry,
                  Map<const Object *, Vector<const ID *>> &instance_geometries_by_object,
                  const Set<pxr::SdfPath> &current_paths)
      : depsgraph(depsgraph),
        bmain(bmain),
        scene(scene),
        view3d(view3d),
        root_path(root_path),
        use_materialx(use_materialx),
        material_render_contexts(material_render_contexts),
        shader_source_types(shader_source_types),
        emitted_materials(emitted_materials),
        emitted_geometry(emitted_geometry),
        emitted_paths(current_paths),
        instance_geometries_by_object(instance_geometries_by_object)
  {
  }

  /* Scene context. */
  Depsgraph *depsgraph;
  Main *bmain;
  Scene *scene;
  const View3D *view3d;
  pxr::SdfPath root_path;
  bool use_materialx;
  Span<pxr::TfToken> material_render_contexts;
  Span<pxr::TfToken> shader_source_types;

  /* Owned by HydraSceneIndex. */
  Map<const Material *, EmittedMaterial> &emitted_materials;
  Map<EmittedGeometryKey, EmittedGeometry> &emitted_geometry;
  const Set<pxr::SdfPath> &emitted_paths;
  Map<const Object *, Vector<const ID *>> &instance_geometries_by_object;

  /* Materials. */
  Set<const Material *> used_emitted_materials;
  Vector<const Material *> new_emitted_materials;

  /* Geometry. */
  Set<EmittedGeometryKey> used_emitted_geometry;
  Set<const Object *> used_instance_sources;

  /* Paths and prims. */
  Set<pxr::SdfPath> new_paths;
  pxr::HdRetainedSceneIndex::AddedPrimEntries to_add;
  pxr::HdSceneIndexObserver::DirtiedPrimEntries to_dirty;

  /* Objects. */
  Map<const Object *, EmittedObject> new_emitted_objects;

  /* Instancing and prototypes. */
  Vector<pxr::SdfPath> all_proto_paths;
  Vector<pxr::VtIntArray> per_proto_indices;
  pxr::VtMatrix4dArray instance_transforms;
  Map<const Object *, IndexRange> proto_range_by_source;
  Map<const Object *, int> nonmesh_instance_count;

  /* Path naming scheme for each prim. */
  pxr::SdfPath object_prim_id(const Object *object) const;
  pxr::SdfPath submesh_prim_id(const Object *object, int submesh_index) const;
  pxr::SdfPath hair_prim_id(const Object *object, const ParticleSystem *psys) const;
  pxr::SdfPath material_prim_id(const Material *material) const;
  pxr::SdfPath instancer_prim_id() const;
  pxr::SdfPath instancer_proto_root(const Object *source) const;
  pxr::SdfPath instancer_proto_submesh(const Object *source, int submesh_index) const;
  pxr::SdfPath instance_clone_prim_id(const Object *source, int instance_index) const;

  /* Look up or build the emitted material. */
  const EmittedMaterial *get_or_create_material(const Material *material);

  /* Unique geometry key for an emitted object. */
  EmittedGeometryKey emitted_geometry_key(const BObjectInfo &info, const pxr::TfToken &type) const;

  /* Emit a primitive. */
  void emit_prim(const pxr::SdfPath &path,
                 const pxr::TfToken &type,
                 const pxr::HdContainerDataSourceHandle &ds);

  /* Emit an object primitive. */
  void emit_object_prim(EmittedObject &emitted,
                        const pxr::SdfPath &path,
                        const pxr::TfToken &type,
                        const pxr::HdContainerDataSourceHandle &ds,
                        const pxr::GfMatrix4d &geometry_xform = pxr::GfMatrix4d(1.0));

  /* `instancedBy` data source binding `source` object prototype subtree to the instancer. */
  pxr::HdContainerDataSourceHandle proto_instanced_by(const Object *source) const;

  /* Add instance of a prototype for `source` object. */
  void add_instance(const Object *source, const float dupli_mat[4][4]);
};

/* Compose a geometry primitive with per-object transform and visibility. */
pxr::HdContainerDataSourceHandle compose_gprim_data_source(
    const EmittedGeometryPrim &gprim,
    const pxr::GfMatrix4d &transform,
    bool visible,
    const pxr::HdContainerDataSourceHandle &instanced_by_overlay = nullptr);

}  // namespace io::hydra
}  // namespace blender
