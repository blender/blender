/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_attribute_storage.hh"

#include "BLI_vector.hh"

struct CustomData;
struct Main;
struct Mesh;
struct MFace;
struct CustomDataLayer;

namespace blender::bke {

void mesh_uv_select_to_single_attribute(Mesh &mesh);

void mesh_custom_normals_to_generic(Mesh &mesh);

void mesh_sculpt_mask_to_generic(Mesh &mesh);

void mesh_freestyle_marks_to_generic(Mesh &mesh);
void mesh_freestyle_marks_to_legacy(AttributeStorage::BlendWriteData &attr_write_data,
                                    CustomData &edge_data,
                                    CustomData &face_data,
                                    Vector<CustomDataLayer, 16> &edge_layers,
                                    Vector<CustomDataLayer, 16> &face_layers);

}  // namespace blender::bke

void BKE_mesh_legacy_convert_uvs_to_generic(Mesh *mesh);

/**
 * Copy face sets to the generic data type from the legacy type.
 */
void BKE_mesh_legacy_face_set_to_generic(Mesh *mesh);

/**
 * Copy edge creases from edges to a separate layer.
 */
void BKE_mesh_legacy_edge_crease_to_layers(Mesh *mesh);
void BKE_mesh_legacy_crease_to_generic(Mesh *mesh);

/**
 * Copy bevel weights from vertices and edges to separate layers.
 */
void BKE_mesh_legacy_bevel_weight_to_layers(Mesh *mesh);
/**
 * Move bevel weight to generic float attribute type.
 */
void BKE_mesh_legacy_bevel_weight_to_generic(Mesh *mesh);

/**
 * Convert the old hide flags (#ME_HIDE) to the hidden element attribute for reading.
 * Only add the attributes when there are any elements in each domain hidden.
 */
void BKE_mesh_legacy_convert_flags_to_hide_layers(Mesh *mesh);

/**
 * Convert the old selection flags (#SELECT/#ME_FACE_SEL) to the selected element attribute for
 * reading. Only add the attributes when there are any elements in each domain selected.
 */
void BKE_mesh_legacy_convert_flags_to_selection_layers(Mesh *mesh);

/**
 * Move material indices from the #MPoly to a generic attributes.
 * Only add the attribute when the indices are not all zero.
 */
void BKE_mesh_legacy_convert_mpoly_to_material_indices(Mesh *mesh);

void BKE_mesh_legacy_attribute_flags_to_strings(Mesh *mesh);

void BKE_mesh_legacy_sharp_faces_from_flags(Mesh *mesh);

void BKE_mesh_legacy_sharp_edges_from_flags(Mesh *mesh);

void BKE_mesh_legacy_uv_seam_from_flags(Mesh *mesh);

void BKE_mesh_legacy_convert_verts_to_positions(Mesh *mesh);

void BKE_mesh_legacy_convert_edges_to_generic(Mesh *mesh);

void BKE_mesh_legacy_convert_polys_to_offsets(Mesh *mesh);

void BKE_mesh_legacy_convert_loops_to_corners(Mesh *mesh);

void BKE_mesh_legacy_face_map_to_generic(Main *bmain);

/**
 * Recreate #MFace Tessellation.
 */
void BKE_mesh_tessface_calc(Mesh *mesh);

void BKE_mesh_tessface_ensure(Mesh *mesh);

/**
 * Rotates the vertices of a face in case v[2] or v[3] (vertex index) is = 0.
 * this is necessary to make the `if #MFace.v4` check for quads work.
 */
int BKE_mesh_mface_index_validate(MFace *mface, CustomData *mfdata, int mfindex, int nr);

void BKE_mesh_convert_mfaces_to_mpolys(Mesh *mesh);

/**
 * The same as #BKE_mesh_convert_mfaces_to_mpolys
 * but oriented to be used in #do_versions from `readfile.cc`
 * the difference is how active/render/clone/stencil indices are handled here.
 *
 * normally they're being set from `pdata` which totally makes sense for meshes which are
 * already converted to #BMesh structures, but when loading older files indices shall be updated in
 * other way around, so newly added `pdata` and `ldata` would have this indices set based on
 * `pdata`  layer.
 *
 * this is normally only needed when reading older files,
 * in all other cases #BKE_mesh_convert_mfaces_to_mpolys shall be always used.
 */
void BKE_mesh_do_versions_convert_mfaces_to_mpolys(Mesh *mesh);

void BKE_mesh_calc_edges_legacy(Mesh *mesh);

void BKE_mesh_do_versions_cd_flag_init(Mesh *mesh);

void BKE_main_mesh_legacy_convert_auto_smooth(Main &bmain);

/**
 * Calculate/create edges from tessface data
 */
void BKE_mesh_calc_edges_tessface(Mesh *mesh);

/* Inlines */

/* NOTE(@sybren): Instead of -1 that function uses ORIGINDEX_NONE as defined in BKE_customdata.hh,
 * but I don't want to force every user of BKE_mesh.h to also include that file. */
inline int BKE_mesh_origindex_mface_mpoly(const int *index_mf_to_mpoly,
                                          const int *index_mp_to_orig,
                                          const int i)
{
  const int j = index_mf_to_mpoly[i];
  return (j != -1) ? (index_mp_to_orig ? index_mp_to_orig[j] : j) : -1;
}
