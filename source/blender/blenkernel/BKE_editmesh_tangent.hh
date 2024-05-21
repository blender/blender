/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "DNA_customdata_types.h"

struct BMEditMesh;

/**
 * \see #BKE_mesh_calc_loop_tangent, same logic but used arrays instead of #BMesh data.
 *
 * \note This function is not so normal, its using #BMesh.ldata as input,
 * but output's to #Mesh.corner_data.
 * This is done because #CD_TANGENT is cache data used only for drawing.
 */
void BKE_editmesh_loop_tangent_calc(BMEditMesh *em,
                                    bool calc_active_tangent,
                                    const char (*tangent_names)[MAX_CUSTOMDATA_LAYER_NAME],
                                    int tangent_names_len,
                                    blender::Span<blender::float3> face_normals,
                                    blender::Span<blender::float3> corner_normals,
                                    blender::Span<blender::float3> vert_orco,
                                    CustomData *dm_loopdata_out,
                                    uint dm_loopdata_out_len,
                                    short *tangent_mask_curr_p);
