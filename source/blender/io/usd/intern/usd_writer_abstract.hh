/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "IO_abstract_hierarchy_iterator.h"
#include "usd_exporter_context.hh"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/boundable.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdUtils/sparseValueWriter.h>

#include "WM_types.hh"

#include <string>

struct ID;
struct IDProperty;
struct Material;
struct ReportList;

namespace blender {
template<typename T> struct Bounds;
}

namespace blender::io::usd {

using blender::io::AbstractHierarchyWriter;
using blender::io::HierarchyContext;

class USDAbstractWriter : public AbstractHierarchyWriter {
 protected:
  const USDExporterContext usd_export_context_;
  pxr::UsdUtilsSparseValueWriter usd_value_writer_;

  bool frame_has_been_written_;
  bool is_animated_;

 public:
  USDAbstractWriter(const USDExporterContext &usd_export_context);

  void write(HierarchyContext &context) override;

  /**
   * Returns true if the data to be written is actually supported. This would, for example, allow a
   * hypothetical camera writer accept a perspective camera but reject an orthogonal one.
   *
   * Returning false from a transform writer will prevent the object and all its descendants from
   * being exported. Returning false from a data writer (object data, hair, or particles) will
   * only prevent that data from being written (and thus cause the object to be exported as an
   * Empty).
   */
  virtual bool is_supported(const HierarchyContext *context) const;

  const pxr::SdfPath &usd_path() const;

  /** Get the wmJobWorkerStatus-provided `reports` list pointer, to use with the BKE_report API. */
  ReportList *reports() const
  {
    return usd_export_context_.export_params.worker_status->reports;
  }

 protected:
  virtual void do_write(HierarchyContext &context) = 0;
  std::string get_export_file_path() const;
  pxr::UsdTimeCode get_export_time_code() const;

  /* Returns the parent path of exported materials. */
  pxr::SdfPath get_material_library_path() const;
  /* Returns the parent path of exported materials for instance prototypes. */
  pxr::SdfPath get_proto_material_root_path(const HierarchyContext &context) const;
  /* Ensure the USD material is created in the default material library folder. */
  pxr::UsdShadeMaterial ensure_usd_material_created(const HierarchyContext &context,
                                                    Material *material) const;
  /* Calls ensure_usd_material_created(). Additionally, if the context is an
   * instancing prototype, creates a reference to the library material under the
   * prototype root. */
  pxr::UsdShadeMaterial ensure_usd_material(const HierarchyContext &context,
                                            Material *material) const;

  void write_id_properties(const pxr::UsdPrim &prim,
                           const ID &id,
                           pxr::UsdTimeCode = pxr::UsdTimeCode::Default()) const;
  void write_user_properties(const pxr::UsdPrim &prim,
                             IDProperty *properties,
                             pxr::UsdTimeCode = pxr::UsdTimeCode::Default()) const;

  void write_visibility(const HierarchyContext &context,
                        const pxr::UsdTimeCode time,
                        const pxr::UsdGeomImageable &usd_geometry);

  /**
   * Turn `prim` into an instance referencing `context.original_export_path`.
   * Return true when the instancing was successful, false otherwise.
   *
   * Reference the original data instead of writing a copy.
   */
  virtual bool mark_as_instance(const HierarchyContext &context, const pxr::UsdPrim &prim);

  /**
   * Compute the bounds for a boundable prim, and author the result as the `extent` attribute.
   *
   * Although this method works for any boundable prim, it is preferred to use Blender's own
   * cached bounds when possible.
   *
   * This method does not author the `extentsHint` attribute, which is also important to provide.
   * Whereas the `extent` attribute can only be authored on prims inheriting from
   * `UsdGeomBoundable`, an `extentsHint` can be provided on any prim, including scopes.  This
   * `extentsHint` should be authored on every prim in a hierarchy being exported.
   *
   * Note that this hint is only useful when importing or inspecting layers, and should not be
   * taken into account when computing extents during export.
   *
   * TODO: also provide method for authoring extentsHint on every prim in a hierarchy.
   */
  void author_extent(const pxr::UsdGeomBoundable &boundable, const pxr::UsdTimeCode time);

  /**
   * Author the `extent` attribute for a boundable prim given the Blender `bounds`.
   */
  void author_extent(const pxr::UsdGeomBoundable &boundable,
                     const std::optional<Bounds<float3>> &bounds,
                     const pxr::UsdTimeCode time);

  void add_to_prim_map(const pxr::SdfPath &usd_path, const ID *id) const;
};

}  // namespace blender::io::usd
