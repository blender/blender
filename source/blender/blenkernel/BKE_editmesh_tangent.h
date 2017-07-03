/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_EDITMESH_TANGENT_H__
#define __BKE_EDITMESH_TANGENT_H__

/** \file BKE_editmesh_tangent.h
 *  \ingroup bke
 */

void BKE_editmesh_loop_tangent_calc(
        BMEditMesh *em, bool calc_active_tangent,
        const char (*tangent_names)[MAX_NAME], int tangent_names_len,
        const float (*poly_normals)[3],
        const float (*loop_normals)[3],
        const float (*vert_orco)[3],
        CustomData *dm_loopdata_out,
        const uint dm_loopdata_out_len,
        short *tangent_mask_curr_p);

#endif /* __BKE_EDITMESH_TANGENT_H__ */
