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
 *
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#ifndef __BKE_SUBDIV_EVAL_H__
#define __BKE_SUBDIV_EVAL_H__

#include "BLI_sys_types.h"

struct Mesh;
struct Subdiv;

/* Returns true if evaluator is ready for use. */
bool BKE_subdiv_eval_begin(struct Subdiv *subdiv);
bool BKE_subdiv_eval_update_from_mesh(struct Subdiv *subdiv, const struct Mesh *mesh);

/* Makes sure displacement evaluator is initialized.
 *
 * NOTE: This function must be called once before evaluating displacement or
 * final surface position. */
void BKE_subdiv_eval_init_displacement(struct Subdiv *subdiv);

/* Single point queries. */

void BKE_subdiv_eval_limit_point(
    struct Subdiv *subdiv, const int ptex_face_index, const float u, const float v, float r_P[3]);
void BKE_subdiv_eval_limit_point_and_derivatives(struct Subdiv *subdiv,
                                                 const int ptex_face_index,
                                                 const float u,
                                                 const float v,
                                                 float r_P[3],
                                                 float r_dPdu[3],
                                                 float r_dPdv[3]);
void BKE_subdiv_eval_limit_point_and_normal(struct Subdiv *subdiv,
                                            const int ptex_face_index,
                                            const float u,
                                            const float v,
                                            float r_P[3],
                                            float r_N[3]);
void BKE_subdiv_eval_limit_point_and_short_normal(struct Subdiv *subdiv,
                                                  const int ptex_face_index,
                                                  const float u,
                                                  const float v,
                                                  float r_P[3],
                                                  short r_N[3]);

void BKE_subdiv_eval_face_varying(struct Subdiv *subdiv,
                                  const int face_varying_channel,
                                  const int ptex_face_index,
                                  const float u,
                                  const float v,
                                  float r_varying[2]);

/* NOTE: Expects derivatives to be correct.
 *
 * TODO(sergey): This is currently used together with
 * BKE_subdiv_eval_final_point() which can easily evaluate derivatives.
 * Would be nice to have displacement evaluation function which does not require
 * knowing derivatives ahead of a time. */
void BKE_subdiv_eval_displacement(struct Subdiv *subdiv,
                                  const int ptex_face_index,
                                  const float u,
                                  const float v,
                                  const float dPdu[3],
                                  const float dPdv[3],
                                  float r_D[3]);

void BKE_subdiv_eval_final_point(
    struct Subdiv *subdiv, const int ptex_face_index, const float u, const float v, float r_P[3]);

/* Patch queries at given resolution.
 *
 * Will evaluate patch at uniformly distributed (u, v) coordinates on a grid
 * of given resolution, producing resolution^2 evaluation points. The order
 * goes as u in rows, v in columns. */

void BKE_subdiv_eval_limit_patch_resolution_point(struct Subdiv *subdiv,
                                                  const int ptex_face_index,
                                                  const int resolution,
                                                  void *buffer,
                                                  const int offset,
                                                  const int stride);
void BKE_subdiv_eval_limit_patch_resolution_point_and_derivatives(struct Subdiv *subdiv,
                                                                  const int ptex_face_index,
                                                                  const int resolution,
                                                                  void *point_buffer,
                                                                  const int point_offset,
                                                                  const int point_stride,
                                                                  void *du_buffer,
                                                                  const int du_offset,
                                                                  const int du_stride,
                                                                  void *dv_buffer,
                                                                  const int dv_offset,
                                                                  const int dv_stride);
void BKE_subdiv_eval_limit_patch_resolution_point_and_normal(struct Subdiv *subdiv,
                                                             const int ptex_face_index,
                                                             const int resolution,
                                                             void *point_buffer,
                                                             const int point_offset,
                                                             const int point_stride,
                                                             void *normal_buffer,
                                                             const int normal_offset,
                                                             const int normal_stride);
void BKE_subdiv_eval_limit_patch_resolution_point_and_short_normal(struct Subdiv *subdiv,
                                                                   const int ptex_face_index,
                                                                   const int resolution,
                                                                   void *point_buffer,
                                                                   const int point_offset,
                                                                   const int point_stride,
                                                                   void *normal_buffer,
                                                                   const int normal_offset,
                                                                   const int normal_stride);

#endif /* __BKE_SUBDIV_EVAL_H__ */
