/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif
#include "DNA_customdata_types.h"

/**
 * \see #BKE_mesh_calc_loop_tangent, same logic but used arrays instead of #BMesh data.
 *
 * \note This function is not so normal, its using #BMesh.ldata as input,
 * but output's to #Mesh.ldata.
 * This is done because #CD_TANGENT is cache data used only for drawing.
 */
void BKE_editmesh_loop_tangent_calc(BMEditMesh *em,
                                    bool calc_active_tangent,
                                    const char (*tangent_names)[MAX_CUSTOMDATA_LAYER_NAME],
                                    int tangent_names_len,
                                    const float (*face_normals)[3],
                                    const float (*loop_normals)[3],
                                    const float (*vert_orco)[3],
                                    CustomData *dm_loopdata_out,
                                    uint dm_loopdata_out_len,
                                    short *tangent_mask_curr_p);

#ifdef __cplusplus
}
#endif
