/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_sys_types.h" /* for intptr_t support */

/** \file
 * \ingroup geo
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ParamHandle ParamHandle; /* Handle to an array of charts. */
typedef intptr_t ParamKey;              /* Key (hash) for identifying verts and faces. */
typedef enum ParamBool {
  PARAM_TRUE = 1,
  PARAM_FALSE = 0,
} ParamBool;

/* -------------------------------------------------------------------- */
/** \name Chart Construction:
 *
 * Faces and seams may only be added between #GEO_uv_parametrizer_construct_begin and
 * #GEO_uv_parametrizer_construct_end.
 *
 * The pointers to `co` and `uv` are stored, rather than being copied. Vertices are implicitly
 * created.
 *
 * In #GEO_uv_parametrizer_construct_end the mesh will be split up according to the seams. The
 * resulting charts must be manifold, connected and open (at least one boundary loop). The output
 * will be written to the `uv` pointers.
 *
 * \{ */

ParamHandle *GEO_uv_parametrizer_construct_begin(void);

void GEO_uv_parametrizer_aspect_ratio(ParamHandle *handle, float aspx, float aspy);

void GEO_uv_parametrizer_face_add(ParamHandle *handle,
                                  ParamKey key,
                                  int nverts,
                                  ParamKey *vkeys,
                                  float *co[4],
                                  float *uv[4],
                                  ParamBool *pin,
                                  ParamBool *select);

void GEO_uv_parametrizer_edge_set_seam(ParamHandle *handle, ParamKey *vkeys);

void GEO_uv_parametrizer_construct_end(ParamHandle *handle,
                                       ParamBool fill,
                                       ParamBool topology_from_uvs,
                                       int *count_fail);
void GEO_uv_parametrizer_delete(ParamHandle *handle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Least Squares Conformal Maps:
 *
 * Charts with less than two pinned vertices are assigned two pins. LSCM is divided to three steps:
 *
 * 1. Begin: compute matrix and its factorization (expensive).
 * 2. Solve using pinned coordinates (cheap).
 * 3. End: clean up.
 *
 * UV coordinates are allowed to change within begin/end, for quick re-solving.
 *
 * \{ */

void GEO_uv_parametrizer_lscm_begin(ParamHandle *handle, ParamBool live, ParamBool abf);
void GEO_uv_parametrizer_lscm_solve(ParamHandle *handle, int *count_changed, int *count_failed);
void GEO_uv_parametrizer_lscm_end(ParamHandle *handle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Stretch
 * \{ */

void GEO_uv_parametrizer_stretch_begin(ParamHandle *handle);
void GEO_uv_parametrizer_stretch_blend(ParamHandle *handle, float blend);
void GEO_uv_parametrizer_stretch_iter(ParamHandle *handle);
void GEO_uv_parametrizer_stretch_end(ParamHandle *handle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Area Smooth
 * \{ */

void GEO_uv_parametrizer_smooth_area(ParamHandle *handle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Packing
 * \{ */

void GEO_uv_parametrizer_pack(ParamHandle *handle,
                              float margin,
                              bool do_rotate,
                              bool ignore_pinned);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Average area for all charts
 * \{ */

void GEO_uv_parametrizer_average(ParamHandle *handle, bool ignore_pinned);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Simple x,y scale
 * \{ */

void GEO_uv_parametrizer_scale(ParamHandle *handle, float x, float y);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flushing
 * \{ */

void GEO_uv_parametrizer_flush(ParamHandle *handle);
void GEO_uv_parametrizer_flush_restore(ParamHandle *handle);

/** \} */

#ifdef __cplusplus
}
#endif
