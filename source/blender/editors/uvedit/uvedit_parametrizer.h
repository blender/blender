/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup eduv
 */

#include "BLI_sys_types.h" /* for intptr_t support */

#ifdef __cplusplus
extern "C" {
#endif

typedef void ParamHandle;  /* handle to a set of charts */
typedef intptr_t ParamKey; /* (hash) key for identifying verts and faces */
typedef enum ParamBool {
  PARAM_TRUE = 1,
  PARAM_FALSE = 0,
} ParamBool;

/* Chart construction:
 * -------------------
 * - faces and seams may only be added between construct_{begin|end}
 * - the pointers to co and uv are stored, rather than being copied
 * - vertices are implicitly created
 * - in construct_end the mesh will be split up according to the seams
 * - the resulting charts must be:
 * - manifold, connected, open (at least one boundary loop)
 * - output will be written to the uv pointers
 */

ParamHandle *param_construct_begin(void);

void param_aspect_ratio(ParamHandle *handle, float aspx, float aspy);

void param_face_add(ParamHandle *handle,
                    ParamKey key,
                    int nverts,
                    ParamKey *vkeys,
                    float *co[4],
                    float *uv[4],
                    ParamBool *pin,
                    ParamBool *select);

void param_edge_set_seam(ParamHandle *handle, ParamKey *vkeys);

void param_construct_end(ParamHandle *handle,
                         ParamBool fill,
                         ParamBool topology_from_uvs,
                         int *count_fail);
void param_delete(ParamHandle *handle);

/* Least Squares Conformal Maps:
 * -----------------------------
 * - charts with less than two pinned vertices are assigned 2 pins
 * - lscm is divided in three steps:
 * - begin: compute matrix and its factorization (expensive)
 * - solve using pinned coordinates (cheap)
 * - end: clean up
 * - uv coordinates are allowed to change within begin/end, for
 *   quick re-solving
 */

void param_lscm_begin(ParamHandle *handle, ParamBool live, ParamBool abf);
void param_lscm_solve(ParamHandle *handle, int *count_changed, int *count_failed);
void param_lscm_end(ParamHandle *handle);

/* Stretch */

void param_stretch_begin(ParamHandle *handle);
void param_stretch_blend(ParamHandle *handle, float blend);
void param_stretch_iter(ParamHandle *handle);
void param_stretch_end(ParamHandle *handle);

/* Area Smooth */

void param_smooth_area(ParamHandle *handle);

/* Packing */

void param_pack(ParamHandle *handle, float margin, bool do_rotate, bool ignore_pinned);

/* Average area for all charts */

void param_average(ParamHandle *handle, bool ignore_pinned);

/* Simple x,y scale */

void param_scale(ParamHandle *handle, float x, float y);

/* Flushing */

void param_flush(ParamHandle *handle);
void param_flush_restore(ParamHandle *handle);

#ifdef __cplusplus
}
#endif
