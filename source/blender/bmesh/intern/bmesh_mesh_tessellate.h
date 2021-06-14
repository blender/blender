/*
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
 */

#pragma once

/** \file
 * \ingroup bmesh
 */

struct BMPartialUpdate;

struct BMeshCalcTessellation_Params {
  /**
   * When calculating normals as well as tessellation, calculate normals after tessellation
   * for improved performance. See #BMeshCalcTessellation_Params
   */
  bool face_normals;
};

void BM_mesh_calc_tessellation_ex(BMesh *bm,
                                  BMLoop *(*looptris)[3],
                                  const struct BMeshCalcTessellation_Params *params);
void BM_mesh_calc_tessellation(BMesh *bm, BMLoop *(*looptris)[3]);

void BM_mesh_calc_tessellation_beauty(BMesh *bm, BMLoop *(*looptris)[3]);

void BM_mesh_calc_tessellation_with_partial_ex(BMesh *bm,
                                               BMLoop *(*looptris)[3],
                                               const struct BMPartialUpdate *bmpinfo,
                                               const struct BMeshCalcTessellation_Params *params);
void BM_mesh_calc_tessellation_with_partial(BMesh *bm,
                                            BMLoop *(*looptris)[3],
                                            const struct BMPartialUpdate *bmpinfo);
