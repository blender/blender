/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Wire Frame.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \param defgrp_index: Vertex group index, -1 for no vertex groups.
 *
 * \note All edge tags must be cleared.
 * \note Behavior matches `MOD_solidify.cc`.
 */
void BM_mesh_wireframe(BMesh *bm,
                       float offset,
                       float offset_fac,
                       float offset_fac_vg,
                       bool use_replace,
                       bool use_boundary,
                       bool use_even_offset,
                       bool use_relative_offset,
                       bool use_crease,
                       float crease_weight,
                       int defgrp_index,
                       bool defgrp_invert,
                       short mat_offset,
                       short mat_max,
                       bool use_tag);

#ifdef __cplusplus
}
#endif
