/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

namespace blender {

struct Depsgraph;
struct Mesh;
struct Object;

namespace io {

/**
 * Conditionally coerce objects to mesh for export.
 * For use by formats that only support meshes.
 */
struct MeshCoerceForExport {
  /**
   * Mesh to read geometry from, referencing evaluated, pre-modified or `owned` data.
   *
   * The same as `owned` when a conversion was performed.
   */
  const Mesh *mesh = nullptr;
  /**
   * Mesh converted from a non-mesh, or null.
   */
  Mesh *owned = nullptr;

  ~MeshCoerceForExport();
};

/**
 * Return the mesh to export for `obj_eval`.
 *
 * The mesh may be converted on demand and stored in #MeshCoerceForExport::owned.
 *
 * \return The mesh to export.
 */
const Mesh *mesh_coerce_for_export_setup(MeshCoerceForExport &coerce,
                                         Depsgraph *depsgraph,
                                         Object *obj_eval,
                                         bool apply_modifiers);

}  // namespace io
}  // namespace blender
