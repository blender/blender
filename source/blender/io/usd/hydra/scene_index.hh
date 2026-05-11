/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include <pxr/base/tf/declarePtrs.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_OPEN_SCOPE
class HdRenderDelegate;
PXR_NAMESPACE_CLOSE_SCOPE

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "CLG_log.h"

#include "util.hh"
#include "world.hh"

namespace blender {

struct Depsgraph;
struct ID;
struct Main;
struct Material;
struct Object;
struct Scene;
struct View3D;

namespace io::hydra {

extern CLG_LogRef *LOG_HYDRA_SCENE_INDEX;

/* Distinguishes different geometry types emitted from the same ID. */
struct EmittedGeometryKey {
  const ID *id;
  pxr::TfToken type;

  uint64_t hash() const;
  friend bool operator==(const EmittedGeometryKey &a, const EmittedGeometryKey &b)
  {
    return a.id == b.id && a.type == b.type;
  }
};

/* Prim emitted by geometry. */
struct EmittedGeometryPrim {
  pxr::TfToken schema_token;
  pxr::HdContainerDataSourceHandle geometry;
  pxr::HdContainerDataSourceHandle primvars;
  pxr::HdContainerDataSourceHandle bindings;
};

/* Emitted geometry, potentially with multiple prims for e.g. submeshes. */
struct EmittedGeometry {
  Vector<EmittedGeometryPrim> prims;
  Vector<const Material *> materials;
};

/* Prim emitted by an object. */
struct EmittedObjectPrim {
  pxr::SdfPath path;
  pxr::TfToken type;
  /* Geometry and material data source. */
  pxr::HdContainerDataSourceHandle base_data_source;
  /* Transform overlay that handles the object transform. */
  pxr::HdContainerDataSourceHandle xform_overlay;
  /* Transforms that comes from the geometry. Stored separately so
   * we can efficiently update just the object transform. */
  pxr::GfMatrix4d geometry_xform = pxr::GfMatrix4d(1.0);

  pxr::HdContainerDataSourceHandle compose() const;
};

/* Emitted object, potentially producing multiple prims. Also tracks
 * materials and geometry keys so we know what's in use for cleanup. */
struct EmittedObject {
  Vector<EmittedObjectPrim> prims;
  Vector<const Material *> materials;
  Vector<EmittedGeometryKey> geometry_keys;
};

/* Emitted material. */
struct EmittedMaterial {
  pxr::SdfPath path;
  pxr::HdContainerDataSourceHandle data_source;
  bool double_sided = true;
};

/* #HydraSceneIndex holds the Hydra scene built from the evaluated
 * Blender scene. Note this is not a subclass of pxr::HdRetainedSceneIndex
 * as builds with assert have an incompatible ABI on Windows and this
 * causes a crash when the destructor destroys STL containers. */
class HydraSceneIndex {
 public:
  HydraSceneIndex(const pxr::SdfPath &root_path,
                  pxr::HdRenderDelegate *render_delegate,
                  bool use_materialx);
  ~HydraSceneIndex();

  pxr::HdRetainedSceneIndexRefPtr retained() const
  {
    return retained_;
  }

  /* Walks the depsgraph and publishes the scene to Hydra as a tree of
   * #HdSceneIndexPrim with typed data sources. */
  void populate(Depsgraph *depsgraph, View3D *view3d);
  void clear();

 private:
  pxr::HdRetainedSceneIndexRefPtr retained_;

  pxr::SdfPath root_path_;
  pxr::HdRenderDelegate *render_delegate_ = nullptr;
  bool use_materialx_ = true;
  std::vector<pxr::TfToken> material_render_contexts_;
  std::vector<pxr::TfToken> shader_source_types_;

  /* Prim paths currently published, for diffing on each populate(). */
  Set<pxr::SdfPath> emitted_paths_;

  /* Emitted objects. */
  Map<const Object *, EmittedObject> emitted_objects_;

  /* Cache of material data sources, keyed by Blender material. */
  Map<const Material *, EmittedMaterial> emitted_materials_;

  /* Cache of geometry data sources shared between objects, composed with
   * per-Object xform and visibility at emit time. */
  Map<EmittedGeometryKey, EmittedGeometry> emitted_geometry_;

  /* Instance geometry for each object, to detect geometry set updates. */
  Map<const Object *, Vector<const ID *>> instance_geometries_by_object_;

  EmittedWorld emitted_world_;

  bool object_is_supported(const Object *object) const;
  bool object_is_visible(Depsgraph *depsgraph,
                         const View3D *view3d,
                         const Object *object,
                         int mode) const;
};

}  // namespace io::hydra
}  // namespace blender
