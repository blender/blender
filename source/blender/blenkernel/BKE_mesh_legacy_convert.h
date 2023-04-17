/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_utildefines.h"

#ifdef __cplusplus
#  include "BLI_resource_scope.hh"
#  include "BLI_span.hh"
#  include "BLI_vector.hh"

#  include "DNA_customdata_types.h"
#  include "DNA_meshdata_types.h"
#endif

struct CustomData;
struct Mesh;
struct MFace;

#ifdef __cplusplus

void BKE_mesh_legacy_convert_uvs_to_struct(
    Mesh *mesh,
    blender::ResourceScope &temp_mloopuv_for_convert,
    blender::Vector<CustomDataLayer, 16> &loop_layers_to_write);
void BKE_mesh_legacy_convert_uvs_to_generic(Mesh *mesh);

/**
 * Move face sets to the legacy type from a generic type.
 */
void BKE_mesh_legacy_face_set_from_generic(
    blender::MutableSpan<CustomDataLayer> poly_layers_to_write);
/**
 * Copy face sets to the generic data type from the legacy type.
 */
void BKE_mesh_legacy_face_set_to_generic(struct Mesh *mesh);

/**
 * Copy edge creases from a separate layer into edges.
 */
void BKE_mesh_legacy_edge_crease_from_layers(struct Mesh *mesh);
/**
 * Copy edge creases from edges to a separate layer.
 */
void BKE_mesh_legacy_edge_crease_to_layers(struct Mesh *mesh);

/**
 * Copy bevel weights from separate layers into vertices and edges.
 */
void BKE_mesh_legacy_bevel_weight_from_layers(struct Mesh *mesh);
/**
 * Copy bevel weights from vertices and edges to separate layers.
 */
void BKE_mesh_legacy_bevel_weight_to_layers(struct Mesh *mesh);

/**
 * Convert the hidden element attributes to the old flag format for writing.
 */
void BKE_mesh_legacy_convert_hide_layers_to_flags(struct Mesh *mesh,
                                                  blender::MutableSpan<MPoly> legacy_polys);
/**
 * Convert the old hide flags (#ME_HIDE) to the hidden element attribute for reading.
 * Only add the attributes when there are any elements in each domain hidden.
 */
void BKE_mesh_legacy_convert_flags_to_hide_layers(struct Mesh *mesh);

/**
 * Convert the selected element attributes to the old flag format for writing.
 */
void BKE_mesh_legacy_convert_selection_layers_to_flags(struct Mesh *mesh,
                                                       blender::MutableSpan<MPoly> legacy_polys);
/**
 * Convert the old selection flags (#SELECT/#ME_FACE_SEL) to the selected element attribute for
 * reading. Only add the attributes when there are any elements in each domain selected.
 */
void BKE_mesh_legacy_convert_flags_to_selection_layers(struct Mesh *mesh);

/**
 * Move material indices from a generic attribute to #MPoly.
 */
void BKE_mesh_legacy_convert_material_indices_to_mpoly(struct Mesh *mesh,
                                                       blender::MutableSpan<MPoly> legacy_polys);
/**
 * Move material indices from the #MPoly struct to a generic attributes.
 * Only add the attribute when the indices are not all zero.
 */
void BKE_mesh_legacy_convert_mpoly_to_material_indices(struct Mesh *mesh);

/** Convert from runtime loose edge cache to legacy edge flag. */
void BKE_mesh_legacy_convert_loose_edges_to_flag(struct Mesh *mesh);

void BKE_mesh_legacy_attribute_flags_to_strings(struct Mesh *mesh);
void BKE_mesh_legacy_attribute_strings_to_flags(struct Mesh *mesh);

void BKE_mesh_legacy_sharp_faces_to_flags(struct Mesh *mesh,
                                          blender::MutableSpan<MPoly> legacy_polys);
void BKE_mesh_legacy_sharp_faces_from_flags(struct Mesh *mesh);

void BKE_mesh_legacy_sharp_edges_to_flags(struct Mesh *mesh);
void BKE_mesh_legacy_sharp_edges_from_flags(struct Mesh *mesh);

void BKE_mesh_legacy_uv_seam_to_flags(struct Mesh *mesh);
void BKE_mesh_legacy_uv_seam_from_flags(struct Mesh *mesh);

struct MVert *BKE_mesh_legacy_convert_positions_to_verts(
    Mesh *mesh,
    blender::ResourceScope &temp_arrays_for_convert,
    blender::Vector<CustomDataLayer, 16> &vert_layers_to_write);

void BKE_mesh_legacy_convert_verts_to_positions(Mesh *mesh);

MEdge *BKE_mesh_legacy_convert_edges_to_medge(
    Mesh *mesh,
    blender::ResourceScope &temp_arrays_for_convert,
    blender::Vector<CustomDataLayer, 16> &edge_layers_to_write);

void BKE_mesh_legacy_convert_edges_to_generic(Mesh *mesh);

struct MLoop *BKE_mesh_legacy_convert_corners_to_loops(
    Mesh *mesh,
    blender::ResourceScope &temp_arrays_for_convert,
    blender::Vector<CustomDataLayer, 16> &loop_layers_to_write);

blender::MutableSpan<MPoly> BKE_mesh_legacy_convert_offsets_to_polys(
    const Mesh *mesh,
    blender::ResourceScope &temp_arrays_for_convert,
    blender::Vector<CustomDataLayer, 16> &poly_layers_to_write);

void BKE_mesh_legacy_convert_polys_to_offsets(Mesh *mesh);

void BKE_mesh_legacy_convert_loops_to_corners(struct Mesh *mesh);

#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Recreate #MFace Tessellation.
 *
 * \note This doesn't use multi-threading like #BKE_mesh_recalc_looptri since
 * it's not used in many places and #MFace should be phased out.
 */

void BKE_mesh_tessface_calc(struct Mesh *mesh);

void BKE_mesh_tessface_ensure(struct Mesh *mesh);

/**
 * Rotates the vertices of a face in case v[2] or v[3] (vertex index) is = 0.
 * this is necessary to make the if #MFace.v4 check for quads work.
 */
int BKE_mesh_mface_index_validate(struct MFace *mface,
                                  struct CustomData *mfdata,
                                  int mfindex,
                                  int nr);

void BKE_mesh_convert_mfaces_to_mpolys(struct Mesh *mesh);

/**
 * The same as #BKE_mesh_convert_mfaces_to_mpolys
 * but oriented to be used in #do_versions from `readfile.c`
 * the difference is how active/render/clone/stencil indices are handled here.
 *
 * normally they're being set from `pdata` which totally makes sense for meshes which are already
 * converted to #BMesh structures, but when loading older files indices shall be updated in other
 * way around, so newly added `pdata` and `ldata` would have this indices set
 * based on `fdata`  layer.
 *
 * this is normally only needed when reading older files,
 * in all other cases #BKE_mesh_convert_mfaces_to_mpolys shall be always used.
 */
void BKE_mesh_do_versions_convert_mfaces_to_mpolys(struct Mesh *mesh);

void BKE_mesh_calc_edges_legacy(struct Mesh *me);

void BKE_mesh_do_versions_cd_flag_init(struct Mesh *mesh);

/* Inlines */

/* NOTE(@sybren): Instead of -1 that function uses ORIGINDEX_NONE as defined in BKE_customdata.h,
 * but I don't want to force every user of BKE_mesh.h to also include that file. */
BLI_INLINE int BKE_mesh_origindex_mface_mpoly(const int *index_mf_to_mpoly,
                                              const int *index_mp_to_orig,
                                              const int i)
{
  const int j = index_mf_to_mpoly[i];
  return (j != -1) ? (index_mp_to_orig ? index_mp_to_orig[j] : j) : -1;
}

#ifdef __cplusplus
}
#endif
