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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#include "draw_manager.h"

#include "BLI_math_bits.h"
#include "BLI_memblock.h"

#include "BKE_global.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "intern/gpu_shader_private.h"

#ifdef USE_GPU_SELECT
#  include "GPU_select.h"
#endif

#ifdef USE_GPU_SELECT
void DRW_select_load_id(uint id)
{
  BLI_assert(G.f & G_FLAG_PICKSEL);
  DST.select_id = id;
}
#endif

#define DEBUG_UBO_BINDING

/* -------------------------------------------------------------------- */
/** \name Draw State (DRW_state)
 * \{ */

void drw_state_set(DRWState state)
{
  if (DST.state == state) {
    return;
  }

#define CHANGED_TO(f) \
  ((DST.state_lock & (f)) ? \
       0 : \
       (((DST.state & (f)) ? ((state & (f)) ? 0 : -1) : ((state & (f)) ? 1 : 0))))

#define CHANGED_ANY(f) (((DST.state & (f)) != (state & (f))) && ((DST.state_lock & (f)) == 0))

#define CHANGED_ANY_STORE_VAR(f, enabled) \
  (((DST.state & (f)) != (enabled = (state & (f)))) && (((DST.state_lock & (f)) == 0)))

  /* Depth Write */
  {
    int test;
    if ((test = CHANGED_TO(DRW_STATE_WRITE_DEPTH))) {
      if (test == 1) {
        glDepthMask(GL_TRUE);
      }
      else {
        glDepthMask(GL_FALSE);
      }
    }
  }

  /* Color Write */
  {
    int test;
    if ((test = CHANGED_TO(DRW_STATE_WRITE_COLOR))) {
      if (test == 1) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }
      else {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      }
    }
  }

  /* Raster Discard */
  {
    if (CHANGED_ANY(DRW_STATE_RASTERIZER_ENABLED)) {
      if ((state & DRW_STATE_RASTERIZER_ENABLED) != 0) {
        glDisable(GL_RASTERIZER_DISCARD);
      }
      else {
        glEnable(GL_RASTERIZER_DISCARD);
      }
    }
  }

  /* Cull */
  {
    DRWState test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_CULL_BACK | DRW_STATE_CULL_FRONT, test)) {
      if (test) {
        glEnable(GL_CULL_FACE);

        if ((state & DRW_STATE_CULL_BACK) != 0) {
          glCullFace(GL_BACK);
        }
        else if ((state & DRW_STATE_CULL_FRONT) != 0) {
          glCullFace(GL_FRONT);
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        glDisable(GL_CULL_FACE);
      }
    }
  }

  /* Depth Test */
  {
    DRWState test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_DEPTH_LESS | DRW_STATE_DEPTH_LESS_EQUAL |
                                  DRW_STATE_DEPTH_EQUAL | DRW_STATE_DEPTH_GREATER |
                                  DRW_STATE_DEPTH_GREATER_EQUAL | DRW_STATE_DEPTH_ALWAYS,
                              test)) {
      if (test) {
        glEnable(GL_DEPTH_TEST);

        if (state & DRW_STATE_DEPTH_LESS) {
          glDepthFunc(GL_LESS);
        }
        else if (state & DRW_STATE_DEPTH_LESS_EQUAL) {
          glDepthFunc(GL_LEQUAL);
        }
        else if (state & DRW_STATE_DEPTH_EQUAL) {
          glDepthFunc(GL_EQUAL);
        }
        else if (state & DRW_STATE_DEPTH_GREATER) {
          glDepthFunc(GL_GREATER);
        }
        else if (state & DRW_STATE_DEPTH_GREATER_EQUAL) {
          glDepthFunc(GL_GEQUAL);
        }
        else if (state & DRW_STATE_DEPTH_ALWAYS) {
          glDepthFunc(GL_ALWAYS);
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        glDisable(GL_DEPTH_TEST);
      }
    }
  }

  /* Wire Width */
  {
    int test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_WIRE | DRW_STATE_WIRE_WIDE | DRW_STATE_WIRE_SMOOTH,
                              test)) {
      if (test & DRW_STATE_WIRE_WIDE) {
        GPU_line_width(3.0f);
      }
      else if (test & DRW_STATE_WIRE_SMOOTH) {
        GPU_line_width(2.0f);
        GPU_line_smooth(true);
      }
      else if (test & DRW_STATE_WIRE) {
        GPU_line_width(1.0f);
      }
      else {
        GPU_line_width(1.0f);
        GPU_line_smooth(false);
      }
    }
  }

  /* Points Size */
  {
    int test;
    if ((test = CHANGED_TO(DRW_STATE_POINT))) {
      if (test == 1) {
        GPU_enable_program_point_size();
        glPointSize(5.0f);
      }
      else {
        GPU_disable_program_point_size();
      }
    }
  }

  /* Blending (all buffer) */
  {
    int test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_BLEND | DRW_STATE_BLEND_PREMUL | DRW_STATE_ADDITIVE |
                                  DRW_STATE_MULTIPLY | DRW_STATE_ADDITIVE_FULL |
                                  DRW_STATE_BLEND_OIT | DRW_STATE_BLEND_PREMUL_UNDER,
                              test)) {
      if (test) {
        glEnable(GL_BLEND);

        if ((state & DRW_STATE_BLEND) != 0) {
          glBlendFuncSeparate(GL_SRC_ALPHA,
                              GL_ONE_MINUS_SRC_ALPHA, /* RGB */
                              GL_ONE,
                              GL_ONE_MINUS_SRC_ALPHA); /* Alpha */
        }
        else if ((state & DRW_STATE_BLEND_PREMUL_UNDER) != 0) {
          glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
        }
        else if ((state & DRW_STATE_BLEND_PREMUL) != 0) {
          glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
        else if ((state & DRW_STATE_MULTIPLY) != 0) {
          glBlendFunc(GL_DST_COLOR, GL_ZERO);
        }
        else if ((state & DRW_STATE_BLEND_OIT) != 0) {
          glBlendFuncSeparate(GL_ONE,
                              GL_ONE, /* RGB */
                              GL_ZERO,
                              GL_ONE_MINUS_SRC_ALPHA); /* Alpha */
        }
        else if ((state & DRW_STATE_ADDITIVE) != 0) {
          /* Do not let alpha accumulate but premult the source RGB by it. */
          glBlendFuncSeparate(GL_SRC_ALPHA,
                              GL_ONE, /* RGB */
                              GL_ZERO,
                              GL_ONE); /* Alpha */
        }
        else if ((state & DRW_STATE_ADDITIVE_FULL) != 0) {
          /* Let alpha accumulate. */
          glBlendFunc(GL_ONE, GL_ONE);
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        glDisable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE); /* Don't multiply incoming color by alpha. */
      }
    }
  }

  /* Clip Planes */
  {
    int test;
    if ((test = CHANGED_TO(DRW_STATE_CLIP_PLANES))) {
      if (test == 1) {
        for (int i = 0; i < DST.clip_planes_len; ++i) {
          glEnable(GL_CLIP_DISTANCE0 + i);
        }
      }
      else {
        for (int i = 0; i < MAX_CLIP_PLANES; ++i) {
          glDisable(GL_CLIP_DISTANCE0 + i);
        }
      }
    }
  }

  /* Stencil */
  {
    DRWState test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_WRITE_STENCIL | DRW_STATE_WRITE_STENCIL_SHADOW_PASS |
                                  DRW_STATE_WRITE_STENCIL_SHADOW_FAIL | DRW_STATE_STENCIL_EQUAL |
                                  DRW_STATE_STENCIL_NEQUAL,
                              test)) {
      if (test) {
        glEnable(GL_STENCIL_TEST);
        /* Stencil Write */
        if ((state & DRW_STATE_WRITE_STENCIL) != 0) {
          glStencilMask(0xFF);
          glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        }
        else if ((state & DRW_STATE_WRITE_STENCIL_SHADOW_PASS) != 0) {
          glStencilMask(0xFF);
          glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
          glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
        }
        else if ((state & DRW_STATE_WRITE_STENCIL_SHADOW_FAIL) != 0) {
          glStencilMask(0xFF);
          glStencilOpSeparate(GL_BACK, GL_KEEP, GL_DECR_WRAP, GL_KEEP);
          glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
        }
        /* Stencil Test */
        else if ((state & (DRW_STATE_STENCIL_EQUAL | DRW_STATE_STENCIL_NEQUAL)) != 0) {
          glStencilMask(0x00); /* disable write */
          DST.stencil_mask = STENCIL_UNDEFINED;
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        /* disable write & test */
        DST.stencil_mask = 0;
        glStencilMask(0x00);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glDisable(GL_STENCIL_TEST);
      }
    }
  }

  /* Provoking Vertex */
  {
    int test;
    if ((test = CHANGED_TO(DRW_STATE_FIRST_VERTEX_CONVENTION))) {
      if (test == 1) {
        glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
      }
      else {
        glProvokingVertex(GL_LAST_VERTEX_CONVENTION);
      }
    }
  }

  /* Polygon Offset */
  {
    int test;
    if (CHANGED_ANY_STORE_VAR(DRW_STATE_OFFSET_POSITIVE | DRW_STATE_OFFSET_NEGATIVE, test)) {
      if (test) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glEnable(GL_POLYGON_OFFSET_POINT);
        /* Stencil Write */
        if ((state & DRW_STATE_OFFSET_POSITIVE) != 0) {
          glPolygonOffset(1.0f, 1.0f);
        }
        else if ((state & DRW_STATE_OFFSET_NEGATIVE) != 0) {
          glPolygonOffset(-1.0f, -1.0f);
        }
        else {
          BLI_assert(0);
        }
      }
      else {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
        glDisable(GL_POLYGON_OFFSET_POINT);
      }
    }
  }

#undef CHANGED_TO
#undef CHANGED_ANY
#undef CHANGED_ANY_STORE_VAR

  DST.state = state;
}

static void drw_stencil_set(uint mask)
{
  if (DST.stencil_mask != mask) {
    DST.stencil_mask = mask;
    /* Stencil Write */
    if ((DST.state & DRW_STATE_WRITE_STENCIL) != 0) {
      glStencilFunc(GL_ALWAYS, mask, 0xFF);
    }
    /* Stencil Test */
    else if ((DST.state & DRW_STATE_STENCIL_EQUAL) != 0) {
      glStencilFunc(GL_EQUAL, mask, 0xFF);
    }
    else if ((DST.state & DRW_STATE_STENCIL_NEQUAL) != 0) {
      glStencilFunc(GL_NOTEQUAL, mask, 0xFF);
    }
  }
}

/* Reset state to not interfer with other UI drawcall */
void DRW_state_reset_ex(DRWState state)
{
  DST.state = ~state;
  drw_state_set(state);
}

/**
 * Use with care, intended so selection code can override passes depth settings,
 * which is important for selection to work properly.
 *
 * Should be set in main draw loop, cleared afterwards
 */
void DRW_state_lock(DRWState state)
{
  DST.state_lock = state;
}

void DRW_state_reset(void)
{
  DRW_state_reset_ex(DRW_STATE_DEFAULT);

  /* Reset blending function */
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

/**
 * This only works if DRWPasses have been tagged with DRW_STATE_CLIP_PLANES,
 * and if the shaders have support for it (see usage of gl_ClipDistance).
 * Be sure to call DRW_state_clip_planes_reset() after you finish drawing.
 */
void DRW_state_clip_planes_len_set(uint plane_len)
{
  BLI_assert(plane_len <= MAX_CLIP_PLANES);
  DST.clip_planes_len = plane_len;
}

void DRW_state_clip_planes_reset(void)
{
  DST.clip_planes_len = 0;
}

void DRW_state_clip_planes_set_from_rv3d(RegionView3D *rv3d)
{
  int max_len = 6;
  int real_len = (rv3d->viewlock & RV3D_BOXCLIP) ? 4 : max_len;
  while (real_len < max_len) {
    /* Fill in dummy values that wont change results (6 is hard coded in shaders). */
    copy_v4_v4(rv3d->clip[real_len], rv3d->clip[3]);
    real_len++;
  }

  DRW_state_clip_planes_len_set(max_len);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clipping (DRW_clipping)
 * \{ */

/* Extract the 8 corners from a Projection Matrix.
 * Although less accurate, this solution can be simplified as follows:
 * BKE_boundbox_init_from_minmax(&bbox, (const float[3]){-1.0f, -1.0f, -1.0f}, (const
 * float[3]){1.0f, 1.0f, 1.0f}); for (int i = 0; i < 8; i++) {mul_project_m4_v3(projinv,
 * bbox.vec[i]);}
 */
static void draw_frustum_boundbox_calc(const float (*projmat)[4], BoundBox *r_bbox)
{
  float left, right, bottom, top, near, far;
  bool is_persp = projmat[3][3] == 0.0f;

  projmat_dimensions(projmat, &left, &right, &bottom, &top, &near, &far);

  if (is_persp) {
    left *= near;
    right *= near;
    bottom *= near;
    top *= near;
  }

  r_bbox->vec[0][2] = r_bbox->vec[3][2] = r_bbox->vec[7][2] = r_bbox->vec[4][2] = -near;
  r_bbox->vec[0][0] = r_bbox->vec[3][0] = left;
  r_bbox->vec[4][0] = r_bbox->vec[7][0] = right;
  r_bbox->vec[0][1] = r_bbox->vec[4][1] = bottom;
  r_bbox->vec[7][1] = r_bbox->vec[3][1] = top;

  /* Get the coordinates of the far plane. */
  if (is_persp) {
    float sca_far = far / near;
    left *= sca_far;
    right *= sca_far;
    bottom *= sca_far;
    top *= sca_far;
  }

  r_bbox->vec[1][2] = r_bbox->vec[2][2] = r_bbox->vec[6][2] = r_bbox->vec[5][2] = -far;
  r_bbox->vec[1][0] = r_bbox->vec[2][0] = left;
  r_bbox->vec[6][0] = r_bbox->vec[5][0] = right;
  r_bbox->vec[1][1] = r_bbox->vec[5][1] = bottom;
  r_bbox->vec[2][1] = r_bbox->vec[6][1] = top;
}

static void draw_clipping_setup_from_view(void)
{
  if (DST.clipping.updated) {
    return;
  }

  float(*viewinv)[4] = DST.view_data.matstate.mat[DRW_MAT_VIEWINV];
  float(*projmat)[4] = DST.view_data.matstate.mat[DRW_MAT_WIN];
  float(*projinv)[4] = DST.view_data.matstate.mat[DRW_MAT_WININV];
  BoundSphere *bsphere = &DST.clipping.frustum_bsphere;

  /* Extract Clipping Planes */
  BoundBox bbox;
#if 0 /* It has accuracy problems. */
  BKE_boundbox_init_from_minmax(
      &bbox, (const float[3]){-1.0f, -1.0f, -1.0f}, (const float[3]){1.0f, 1.0f, 1.0f});
  for (int i = 0; i < 8; i++) {
    mul_project_m4_v3(projinv, bbox.vec[i]);
  }
#else
  draw_frustum_boundbox_calc(projmat, &bbox);
#endif
  /* Transform into world space. */
  for (int i = 0; i < 8; i++) {
    mul_m4_v3(viewinv, bbox.vec[i]);
  }

  memcpy(&DST.clipping.frustum_corners, &bbox, sizeof(BoundBox));

  /* Compute clip planes using the world space frustum corners. */
  for (int p = 0; p < 6; p++) {
    int q, r, s;
    switch (p) {
      case 0:
        q = 1;
        r = 2;
        s = 3;
        break; /* -X */
      case 1:
        q = 0;
        r = 4;
        s = 5;
        break; /* -Y */
      case 2:
        q = 1;
        r = 5;
        s = 6;
        break; /* +Z (far) */
      case 3:
        q = 2;
        r = 6;
        s = 7;
        break; /* +Y */
      case 4:
        q = 0;
        r = 3;
        s = 7;
        break; /* -Z (near) */
      default:
        q = 4;
        r = 7;
        s = 6;
        break; /* +X */
    }
    if (DST.frontface == GL_CW) {
      SWAP(int, q, s);
    }

    normal_quad_v3(
        DST.clipping.frustum_planes[p], bbox.vec[p], bbox.vec[q], bbox.vec[r], bbox.vec[s]);
    /* Increase precision and use the mean of all 4 corners. */
    DST.clipping.frustum_planes[p][3] = -dot_v3v3(DST.clipping.frustum_planes[p], bbox.vec[p]);
    DST.clipping.frustum_planes[p][3] += -dot_v3v3(DST.clipping.frustum_planes[p], bbox.vec[q]);
    DST.clipping.frustum_planes[p][3] += -dot_v3v3(DST.clipping.frustum_planes[p], bbox.vec[r]);
    DST.clipping.frustum_planes[p][3] += -dot_v3v3(DST.clipping.frustum_planes[p], bbox.vec[s]);
    DST.clipping.frustum_planes[p][3] *= 0.25f;
  }

  /* Extract Bounding Sphere */
  if (projmat[3][3] != 0.0f) {
    /* Orthographic */
    /* The most extreme points on the near and far plane. (normalized device coords). */
    float *nearpoint = bbox.vec[0];
    float *farpoint = bbox.vec[6];

    /* just use median point */
    mid_v3_v3v3(bsphere->center, farpoint, nearpoint);
    bsphere->radius = len_v3v3(bsphere->center, farpoint);
  }
  else if (projmat[2][0] == 0.0f && projmat[2][1] == 0.0f) {
    /* Perspective with symmetrical frustum. */

    /* We obtain the center and radius of the circumscribed circle of the
     * isosceles trapezoid composed by the diagonals of the near and far clipping plane */

    /* center of each clipping plane */
    float mid_min[3], mid_max[3];
    mid_v3_v3v3(mid_min, bbox.vec[3], bbox.vec[4]);
    mid_v3_v3v3(mid_max, bbox.vec[2], bbox.vec[5]);

    /* square length of the diagonals of each clipping plane */
    float a_sq = len_squared_v3v3(bbox.vec[3], bbox.vec[4]);
    float b_sq = len_squared_v3v3(bbox.vec[2], bbox.vec[5]);

    /* distance squared between clipping planes */
    float h_sq = len_squared_v3v3(mid_min, mid_max);

    float fac = (4 * h_sq + b_sq - a_sq) / (8 * h_sq);

    /* The goal is to get the smallest sphere,
     * not the sphere that passes through each corner */
    CLAMP(fac, 0.0f, 1.0f);

    interp_v3_v3v3(bsphere->center, mid_min, mid_max, fac);

    /* distance from the center to one of the points of the far plane (1, 2, 5, 6) */
    bsphere->radius = len_v3v3(bsphere->center, bbox.vec[1]);
  }
  else {
    /* Perspective with asymmetrical frustum. */

    /* We put the sphere center on the line that goes from origin
     * to the center of the far clipping plane. */

    /* Detect which of the corner of the far clipping plane is the farthest to the origin */
    float nfar[4];               /* most extreme far point in NDC space */
    float farxy[2];              /* farpoint projection onto the near plane */
    float farpoint[3] = {0.0f};  /* most extreme far point in camera coordinate */
    float nearpoint[3];          /* most extreme near point in camera coordinate */
    float farcenter[3] = {0.0f}; /* center of far cliping plane in camera coordinate */
    float F = -1.0f, N;          /* square distance of far and near point to origin */
    float f, n; /* distance of far and near point to z axis. f is always > 0 but n can be < 0 */
    float e, s; /* far and near clipping distance (<0) */
    float c;    /* slope of center line = distance of far clipping center
                 * to z axis / far clipping distance. */
    float z;    /* projection of sphere center on z axis (<0) */

    /* Find farthest corner and center of far clip plane. */
    float corner[3] = {1.0f, 1.0f, 1.0f}; /* in clip space */
    for (int i = 0; i < 4; i++) {
      float point[3];
      mul_v3_project_m4_v3(point, projinv, corner);
      float len = len_squared_v3(point);
      if (len > F) {
        copy_v3_v3(nfar, corner);
        copy_v3_v3(farpoint, point);
        F = len;
      }
      add_v3_v3(farcenter, point);
      /* rotate by 90 degree to walk through the 4 points of the far clip plane */
      float tmp = corner[0];
      corner[0] = -corner[1];
      corner[1] = tmp;
    }

    /* the far center is the average of the far clipping points */
    mul_v3_fl(farcenter, 0.25f);
    /* the extreme near point is the opposite point on the near clipping plane */
    copy_v3_fl3(nfar, -nfar[0], -nfar[1], -1.0f);
    mul_v3_project_m4_v3(nearpoint, projinv, nfar);
    /* this is a frustum projection */
    N = len_squared_v3(nearpoint);
    e = farpoint[2];
    s = nearpoint[2];
    /* distance to view Z axis */
    f = len_v2(farpoint);
    /* get corresponding point on the near plane */
    mul_v2_v2fl(farxy, farpoint, s / e);
    /* this formula preserve the sign of n */
    sub_v2_v2(nearpoint, farxy);
    n = f * s / e - len_v2(nearpoint);
    c = len_v2(farcenter) / e;
    /* the big formula, it simplifies to (F-N)/(2(e-s)) for the symmetric case */
    z = (F - N) / (2.0f * (e - s + c * (f - n)));

    bsphere->center[0] = farcenter[0] * z / e;
    bsphere->center[1] = farcenter[1] * z / e;
    bsphere->center[2] = z;
    bsphere->radius = len_v3v3(bsphere->center, farpoint);

    /* Transform to world space. */
    mul_m4_v3(viewinv, bsphere->center);
  }

  DST.clipping.updated = true;
}

/* Return True if the given BoundSphere intersect the current view frustum */
bool DRW_culling_sphere_test(BoundSphere *bsphere)
{
  draw_clipping_setup_from_view();

  /* Bypass test if radius is negative. */
  if (bsphere->radius < 0.0f) {
    return true;
  }

  /* Do a rough test first: Sphere VS Sphere intersect. */
  BoundSphere *frustum_bsphere = &DST.clipping.frustum_bsphere;
  float center_dist_sq = len_squared_v3v3(bsphere->center, frustum_bsphere->center);
  float radius_sum = bsphere->radius + frustum_bsphere->radius;
  if (center_dist_sq > SQUARE(radius_sum)) {
    return false;
  }
  /* TODO we could test against the inscribed sphere of the frustum to early out positively. */

  /* Test against the 6 frustum planes. */
  /* TODO order planes with sides first then far then near clip. Should be better culling heuristic
   * when sculpting. */
  for (int p = 0; p < 6; p++) {
    float dist = plane_point_side_v3(DST.clipping.frustum_planes[p], bsphere->center);
    if (dist < -bsphere->radius) {
      return false;
    }
  }

  return true;
}

/* Return True if the given BoundBox intersect the current view frustum.
 * bbox must be in world space. */
bool DRW_culling_box_test(BoundBox *bbox)
{
  draw_clipping_setup_from_view();

  /* 6 view frustum planes */
  for (int p = 0; p < 6; p++) {
    /* 8 box vertices. */
    for (int v = 0; v < 8; v++) {
      float dist = plane_point_side_v3(DST.clipping.frustum_planes[p], bbox->vec[v]);
      if (dist > 0.0f) {
        /* At least one point in front of this plane.
         * Go to next plane. */
        break;
      }
      else if (v == 7) {
        /* 8 points behind this plane. */
        return false;
      }
    }
  }

  return true;
}

/* Return True if the current view frustum is inside or intersect the given plane */
bool DRW_culling_plane_test(float plane[4])
{
  draw_clipping_setup_from_view();

  /* Test against the 8 frustum corners. */
  for (int c = 0; c < 8; c++) {
    float dist = plane_point_side_v3(plane, DST.clipping.frustum_corners.vec[c]);
    if (dist < 0.0f) {
      return true;
    }
  }

  return false;
}

void DRW_culling_frustum_corners_get(BoundBox *corners)
{
  draw_clipping_setup_from_view();
  memcpy(corners, &DST.clipping.frustum_corners, sizeof(BoundBox));
}

/* See draw_clipping_setup_from_view() for the plane order. */
void DRW_culling_frustum_planes_get(float planes[6][4])
{
  draw_clipping_setup_from_view();
  memcpy(planes, &DST.clipping.frustum_planes, sizeof(DST.clipping.frustum_planes));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw (DRW_draw)
 * \{ */

static void draw_visibility_eval(DRWCallState *st)
{
  bool culled = st->flag & DRW_CALL_CULLED;

  if (st->cache_id != DST.state_cache_id) {
    /* Update culling result for this view. */
    culled = !DRW_culling_sphere_test(&st->bsphere);
  }

  if (st->visibility_cb) {
    culled = !st->visibility_cb(!culled, st->user_data);
  }

  SET_FLAG_FROM_TEST(st->flag, culled, DRW_CALL_CULLED);
}

static void draw_matrices_model_prepare(DRWCallState *st)
{
  if (st->cache_id == DST.state_cache_id) {
    /* Values are already updated for this view. */
    return;
  }
  else {
    st->cache_id = DST.state_cache_id;
  }

  /* No need to go further the call will not be used. */
  if ((st->flag & DRW_CALL_CULLED) != 0 && (st->flag & DRW_CALL_BYPASS_CULLING) == 0) {
    return;
  }
  /* Order matters */
  if (st->matflag & (DRW_CALL_MODELVIEW | DRW_CALL_MODELVIEWINVERSE)) {
    mul_m4_m4m4(st->modelview, DST.view_data.matstate.mat[DRW_MAT_VIEW], st->model);
  }
  if (st->matflag & DRW_CALL_MODELVIEWINVERSE) {
    invert_m4_m4(st->modelviewinverse, st->modelview);
  }
  if (st->matflag & DRW_CALL_MODELVIEWPROJECTION) {
    mul_m4_m4m4(st->modelviewprojection, DST.view_data.matstate.mat[DRW_MAT_PERS], st->model);
  }
}

static void draw_geometry_prepare(DRWShadingGroup *shgroup, DRWCall *call)
{
  /* step 1 : bind object dependent matrices */
  if (call != NULL) {
    DRWCallState *state = call->state;

    if (shgroup->model != -1) {
      GPU_shader_uniform_vector(shgroup->shader, shgroup->model, 16, 1, (float *)state->model);
    }
    if (shgroup->modelinverse != -1) {
      GPU_shader_uniform_vector(
          shgroup->shader, shgroup->modelinverse, 16, 1, (float *)state->modelinverse);
    }
    if (shgroup->modelview != -1) {
      GPU_shader_uniform_vector(
          shgroup->shader, shgroup->modelview, 16, 1, (float *)state->modelview);
    }
    if (shgroup->modelviewinverse != -1) {
      GPU_shader_uniform_vector(
          shgroup->shader, shgroup->modelviewinverse, 16, 1, (float *)state->modelviewinverse);
    }
    if (shgroup->modelviewprojection != -1) {
      GPU_shader_uniform_vector(shgroup->shader,
                                shgroup->modelviewprojection,
                                16,
                                1,
                                (float *)state->modelviewprojection);
    }
    if (shgroup->objectinfo != -1) {
      float objectinfo[4];
      objectinfo[0] = state->objectinfo[0];
      objectinfo[1] = call->single.ma_index; /* WATCH this is only valid for single drawcalls. */
      objectinfo[2] = state->objectinfo[1];
      objectinfo[3] = (state->flag & DRW_CALL_NEGSCALE) ? -1.0f : 1.0f;
      GPU_shader_uniform_vector(shgroup->shader, shgroup->objectinfo, 4, 1, (float *)objectinfo);
    }
    if (shgroup->orcotexfac != -1) {
      GPU_shader_uniform_vector(
          shgroup->shader, shgroup->orcotexfac, 3, 2, (float *)state->orcotexfac);
    }
  }
  else {
    /* For instancing and batching. */
    float unitmat[4][4];
    unit_m4(unitmat);

    if (shgroup->model != -1) {
      GPU_shader_uniform_vector(shgroup->shader, shgroup->model, 16, 1, (float *)unitmat);
    }
    if (shgroup->modelinverse != -1) {
      GPU_shader_uniform_vector(shgroup->shader, shgroup->modelinverse, 16, 1, (float *)unitmat);
    }
    if (shgroup->modelview != -1) {
      GPU_shader_uniform_vector(shgroup->shader,
                                shgroup->modelview,
                                16,
                                1,
                                (float *)DST.view_data.matstate.mat[DRW_MAT_VIEW]);
    }
    if (shgroup->modelviewinverse != -1) {
      GPU_shader_uniform_vector(shgroup->shader,
                                shgroup->modelviewinverse,
                                16,
                                1,
                                (float *)DST.view_data.matstate.mat[DRW_MAT_VIEWINV]);
    }
    if (shgroup->modelviewprojection != -1) {
      GPU_shader_uniform_vector(shgroup->shader,
                                shgroup->modelviewprojection,
                                16,
                                1,
                                (float *)DST.view_data.matstate.mat[DRW_MAT_PERS]);
    }
    if (shgroup->objectinfo != -1) {
      GPU_shader_uniform_vector(shgroup->shader, shgroup->objectinfo, 4, 1, (float *)unitmat);
    }
    if (shgroup->orcotexfac != -1) {
      GPU_shader_uniform_vector(
          shgroup->shader, shgroup->orcotexfac, 3, 2, (float *)shgroup->instance_orcofac);
    }
  }
}

static void draw_geometry_execute_ex(
    DRWShadingGroup *shgroup, GPUBatch *geom, uint start, uint count, bool draw_instance)
{
  /* Special case: empty drawcall, placement is done via shader, don't bind anything. */
  /* TODO use DRW_CALL_PROCEDURAL instead */
  if (geom == NULL) {
    BLI_assert(shgroup->type == DRW_SHG_TRIANGLE_BATCH); /* Add other type if needed. */
    /* Shader is already bound. */
    GPU_draw_primitive(GPU_PRIM_TRIS, count);
    return;
  }

  /* step 2 : bind vertex array & draw */
  GPU_batch_program_set_no_use(
      geom, GPU_shader_get_program(shgroup->shader), GPU_shader_get_interface(shgroup->shader));
  /* XXX hacking gawain. we don't want to call glUseProgram! (huge performance loss) */
  geom->program_in_use = true;

  GPU_batch_draw_range_ex(geom, start, count, draw_instance);

  geom->program_in_use = false; /* XXX hacking gawain */
}

static void draw_geometry_execute(DRWShadingGroup *shgroup, GPUBatch *geom)
{
  draw_geometry_execute_ex(shgroup, geom, 0, 0, false);
}

enum {
  BIND_NONE = 0,
  BIND_TEMP = 1,    /* Release slot after this shading group. */
  BIND_PERSIST = 2, /* Release slot only after the next shader change. */
};

static void set_bound_flags(uint64_t *slots, uint64_t *persist_slots, int slot_idx, char bind_type)
{
  uint64_t slot = 1lu << slot_idx;
  *slots |= slot;
  if (bind_type == BIND_PERSIST) {
    *persist_slots |= slot;
  }
}

static int get_empty_slot_index(uint64_t slots)
{
  uint64_t empty_slots = ~slots;
  /* Find first empty slot using bitscan. */
  if (empty_slots != 0) {
    if ((empty_slots & 0xFFFFFFFFlu) != 0) {
      return (int)bitscan_forward_uint(empty_slots);
    }
    else {
      return (int)bitscan_forward_uint(empty_slots >> 32) + 32;
    }
  }
  else {
    /* Greater than GPU_max_textures() */
    return 99999;
  }
}

static void bind_texture(GPUTexture *tex, char bind_type)
{
  int idx = GPU_texture_bound_number(tex);
  if (idx == -1) {
    /* Texture isn't bound yet. Find an empty slot and bind it. */
    idx = get_empty_slot_index(DST.RST.bound_tex_slots);

    if (idx < GPU_max_textures()) {
      GPUTexture **gpu_tex_slot = &DST.RST.bound_texs[idx];
      /* Unbind any previous texture. */
      if (*gpu_tex_slot != NULL) {
        GPU_texture_unbind(*gpu_tex_slot);
      }
      GPU_texture_bind(tex, idx);
      *gpu_tex_slot = tex;
    }
    else {
      printf("Not enough texture slots! Reduce number of textures used by your shader.\n");
      return;
    }
  }
  else {
    /* This texture slot was released but the tex
     * is still bound. Just flag the slot again. */
    BLI_assert(DST.RST.bound_texs[idx] == tex);
  }
  set_bound_flags(&DST.RST.bound_tex_slots, &DST.RST.bound_tex_slots_persist, idx, bind_type);
}

static void bind_ubo(GPUUniformBuffer *ubo, char bind_type)
{
  int idx = GPU_uniformbuffer_bindpoint(ubo);
  if (idx == -1) {
    /* UBO isn't bound yet. Find an empty slot and bind it. */
    idx = get_empty_slot_index(DST.RST.bound_ubo_slots);

    if (idx < GPU_max_ubo_binds()) {
      GPUUniformBuffer **gpu_ubo_slot = &DST.RST.bound_ubos[idx];
      /* Unbind any previous UBO. */
      if (*gpu_ubo_slot != NULL) {
        GPU_uniformbuffer_unbind(*gpu_ubo_slot);
      }
      GPU_uniformbuffer_bind(ubo, idx);
      *gpu_ubo_slot = ubo;
    }
    else {
      /* printf so user can report bad behavior */
      printf("Not enough ubo slots! This should not happen!\n");
      /* This is not depending on user input.
       * It is our responsibility to make sure there is enough slots. */
      BLI_assert(0);
      return;
    }
  }
  else {
    /* This UBO slot was released but the UBO is
     * still bound here. Just flag the slot again. */
    BLI_assert(DST.RST.bound_ubos[idx] == ubo);
  }
  set_bound_flags(&DST.RST.bound_ubo_slots, &DST.RST.bound_ubo_slots_persist, idx, bind_type);
}

#ifndef NDEBUG
/**
 * Opengl specification is strict on buffer binding.
 *
 * " If any active uniform block is not backed by a
 * sufficiently large buffer object, the results of shader
 * execution are undefined, and may result in GL interruption or
 * termination. " - Opengl 3.3 Core Specification
 *
 * For now we only check if the binding is correct. Not the size of
 * the bound ubo.
 *
 * See T55475.
 * */
static bool ubo_bindings_validate(DRWShadingGroup *shgroup)
{
  bool valid = true;
#  ifdef DEBUG_UBO_BINDING
  /* Check that all active uniform blocks have a non-zero buffer bound. */
  GLint program = 0;
  GLint active_blocks = 0;

  glGetIntegerv(GL_CURRENT_PROGRAM, &program);
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &active_blocks);

  for (uint i = 0; i < active_blocks; ++i) {
    int binding = 0;
    int buffer = 0;

    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_BINDING, &binding);
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, binding, &buffer);

    if (buffer == 0) {
      char blockname[64];
      glGetActiveUniformBlockName(program, i, sizeof(blockname), NULL, blockname);

      if (valid) {
        printf("Trying to draw with missing UBO binding.\n");
        valid = false;
      }
      printf("Pass : %s, Shader : %s, Block : %s\n",
             shgroup->pass_parent->name,
             shgroup->shader->name,
             blockname);
    }
  }
#  endif
  return valid;
}
#endif

static void release_texture_slots(bool with_persist)
{
  if (with_persist) {
    DST.RST.bound_tex_slots = 0;
    DST.RST.bound_tex_slots_persist = 0;
  }
  else {
    DST.RST.bound_tex_slots &= DST.RST.bound_tex_slots_persist;
  }
}

static void release_ubo_slots(bool with_persist)
{
  if (with_persist) {
    DST.RST.bound_ubo_slots = 0;
    DST.RST.bound_ubo_slots_persist = 0;
  }
  else {
    DST.RST.bound_ubo_slots &= DST.RST.bound_ubo_slots_persist;
  }
}

static void draw_shgroup(DRWShadingGroup *shgroup, DRWState pass_state)
{
  BLI_assert(shgroup->shader);

  GPUTexture *tex;
  GPUUniformBuffer *ubo;
  int val;
  float fval;
  const bool shader_changed = (DST.shader != shgroup->shader);
  bool use_tfeedback = false;

  if (shader_changed) {
    if (DST.shader) {
      GPU_shader_unbind();
    }
    GPU_shader_bind(shgroup->shader);
    DST.shader = shgroup->shader;
  }

  if ((pass_state & DRW_STATE_TRANS_FEEDBACK) != 0 &&
      (shgroup->type == DRW_SHG_FEEDBACK_TRANSFORM)) {
    use_tfeedback = GPU_shader_transform_feedback_enable(shgroup->shader,
                                                         shgroup->tfeedback_target->vbo_id);
  }

  release_ubo_slots(shader_changed);
  release_texture_slots(shader_changed);

  drw_state_set((pass_state & shgroup->state_extra_disable) | shgroup->state_extra);
  drw_stencil_set(shgroup->stencil_mask);

  /* Binding Uniform */
  for (DRWUniform *uni = shgroup->uniforms; uni; uni = uni->next) {
    if (uni->location == -2) {
      uni->location = GPU_shader_get_uniform_ensure(shgroup->shader,
                                                    DST.uniform_names.buffer + uni->name_ofs);
      if (uni->location == -1) {
        continue;
      }
    }
    switch (uni->type) {
      case DRW_UNIFORM_SHORT_TO_INT:
        val = (int)*((short *)uni->pvalue);
        GPU_shader_uniform_vector_int(
            shgroup->shader, uni->location, uni->length, uni->arraysize, &val);
        break;
      case DRW_UNIFORM_SHORT_TO_FLOAT:
        fval = (float)*((short *)uni->pvalue);
        GPU_shader_uniform_vector(
            shgroup->shader, uni->location, uni->length, uni->arraysize, (float *)&fval);
        break;
      case DRW_UNIFORM_BOOL_COPY:
      case DRW_UNIFORM_INT_COPY:
        GPU_shader_uniform_vector_int(
            shgroup->shader, uni->location, uni->length, uni->arraysize, &uni->ivalue);
        break;
      case DRW_UNIFORM_BOOL:
      case DRW_UNIFORM_INT:
        GPU_shader_uniform_vector_int(
            shgroup->shader, uni->location, uni->length, uni->arraysize, (int *)uni->pvalue);
        break;
      case DRW_UNIFORM_FLOAT_COPY:
        GPU_shader_uniform_vector(
            shgroup->shader, uni->location, uni->length, uni->arraysize, &uni->fvalue);
        break;
      case DRW_UNIFORM_FLOAT:
        GPU_shader_uniform_vector(
            shgroup->shader, uni->location, uni->length, uni->arraysize, (float *)uni->pvalue);
        break;
      case DRW_UNIFORM_TEXTURE:
        tex = (GPUTexture *)uni->pvalue;
        BLI_assert(tex);
        bind_texture(tex, BIND_TEMP);
        GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
        break;
      case DRW_UNIFORM_TEXTURE_PERSIST:
        tex = (GPUTexture *)uni->pvalue;
        BLI_assert(tex);
        bind_texture(tex, BIND_PERSIST);
        GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
        break;
      case DRW_UNIFORM_TEXTURE_REF:
        tex = *((GPUTexture **)uni->pvalue);
        BLI_assert(tex);
        bind_texture(tex, BIND_TEMP);
        GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
        break;
      case DRW_UNIFORM_BLOCK:
        ubo = (GPUUniformBuffer *)uni->pvalue;
        bind_ubo(ubo, BIND_TEMP);
        GPU_shader_uniform_buffer(shgroup->shader, uni->location, ubo);
        break;
      case DRW_UNIFORM_BLOCK_PERSIST:
        ubo = (GPUUniformBuffer *)uni->pvalue;
        bind_ubo(ubo, BIND_PERSIST);
        GPU_shader_uniform_buffer(shgroup->shader, uni->location, ubo);
        break;
    }
  }

#ifdef USE_GPU_SELECT
#  define GPU_SELECT_LOAD_IF_PICKSEL(_select_id) \
    if (G.f & G_FLAG_PICKSEL) { \
      GPU_select_load_id(_select_id); \
    } \
    ((void)0)

#  define GPU_SELECT_LOAD_IF_PICKSEL_CALL(_call) \
    if ((G.f & G_FLAG_PICKSEL) && (_call)) { \
      GPU_select_load_id((_call)->select_id); \
    } \
    ((void)0)

#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST(_shgroup, _start, _count) \
    _start = 0; \
    _count = _shgroup->instance_count; \
    int *select_id = NULL; \
    if (G.f & G_FLAG_PICKSEL) { \
      if (_shgroup->override_selectid == -1) { \
        /* Hack : get vbo data without actually drawing. */ \
        GPUVertBufRaw raw; \
        GPU_vertbuf_attr_get_raw_data(_shgroup->inst_selectid, 0, &raw); \
        select_id = GPU_vertbuf_raw_step(&raw); \
        switch (_shgroup->type) { \
          case DRW_SHG_TRIANGLE_BATCH: \
            _count = 3; \
            break; \
          case DRW_SHG_LINE_BATCH: \
            _count = 2; \
            break; \
          default: \
            _count = 1; \
            break; \
        } \
      } \
      else { \
        GPU_select_load_id(_shgroup->override_selectid); \
      } \
    } \
    while (_start < _shgroup->instance_count) { \
      if (select_id) { \
        GPU_select_load_id(select_id[_start]); \
      }

#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST_END(_start, _count) \
    _start += _count; \
    } \
    ((void)0)

#else
#  define GPU_SELECT_LOAD_IF_PICKSEL(select_id)
#  define GPU_SELECT_LOAD_IF_PICKSEL_CALL(call)
#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST_END(start, count) ((void)0)
#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST(_shgroup, _start, _count) \
    _start = 0; \
    _count = _shgroup->instance_count;

#endif

  BLI_assert(ubo_bindings_validate(shgroup));

  /* Rendering Calls */
  if (!ELEM(shgroup->type, DRW_SHG_NORMAL, DRW_SHG_FEEDBACK_TRANSFORM)) {
    /* Replacing multiple calls with only one */
    if (ELEM(shgroup->type, DRW_SHG_INSTANCE, DRW_SHG_INSTANCE_EXTERNAL)) {
      if (shgroup->type == DRW_SHG_INSTANCE_EXTERNAL) {
        if (shgroup->instance_geom != NULL) {
          GPU_SELECT_LOAD_IF_PICKSEL(shgroup->override_selectid);
          draw_geometry_prepare(shgroup, NULL);
          draw_geometry_execute_ex(shgroup, shgroup->instance_geom, 0, 0, true);
        }
      }
      else {
        if (shgroup->instance_count > 0) {
          uint count, start;
          draw_geometry_prepare(shgroup, NULL);
          GPU_SELECT_LOAD_IF_PICKSEL_LIST (shgroup, start, count) {
            draw_geometry_execute_ex(shgroup, shgroup->instance_geom, start, count, true);
          }
          GPU_SELECT_LOAD_IF_PICKSEL_LIST_END(start, count);
        }
      }
    }
    else { /* DRW_SHG_***_BATCH */
      /* Some dynamic batch can have no geom (no call to aggregate) */
      if (shgroup->instance_count > 0) {
        uint count, start;
        draw_geometry_prepare(shgroup, NULL);
        GPU_SELECT_LOAD_IF_PICKSEL_LIST (shgroup, start, count) {
          draw_geometry_execute_ex(shgroup, shgroup->batch_geom, start, count, false);
        }
        GPU_SELECT_LOAD_IF_PICKSEL_LIST_END(start, count);
      }
    }
  }
  else {
    bool prev_neg_scale = false;
    int callid = 0;
    for (DRWCall *call = shgroup->calls.first; call; call = call->next) {

      /* OPTI/IDEA(clem): Do this preparation in another thread. */
      draw_visibility_eval(call->state);
      draw_matrices_model_prepare(call->state);

      if ((call->state->flag & DRW_CALL_CULLED) != 0 &&
          (call->state->flag & DRW_CALL_BYPASS_CULLING) == 0) {
        continue;
      }

      /* XXX small exception/optimisation for outline rendering. */
      if (shgroup->callid != -1) {
        GPU_shader_uniform_vector_int(shgroup->shader, shgroup->callid, 1, 1, &callid);
        callid += 1;
      }

      /* Negative scale objects */
      bool neg_scale = call->state->flag & DRW_CALL_NEGSCALE;
      if (neg_scale != prev_neg_scale) {
        glFrontFace((neg_scale) ? DST.backface : DST.frontface);
        prev_neg_scale = neg_scale;
      }

      GPU_SELECT_LOAD_IF_PICKSEL_CALL(call);
      draw_geometry_prepare(shgroup, call);

      switch (call->type) {
        case DRW_CALL_SINGLE:
          draw_geometry_execute(shgroup, call->single.geometry);
          break;
        case DRW_CALL_RANGE:
          draw_geometry_execute_ex(
              shgroup, call->range.geometry, call->range.start, call->range.count, false);
          break;
        case DRW_CALL_INSTANCES:
          draw_geometry_execute_ex(
              shgroup, call->instances.geometry, 0, *call->instances.count, true);
          break;
        case DRW_CALL_PROCEDURAL:
          GPU_draw_primitive(call->procedural.prim_type, call->procedural.vert_count);
          break;
        default:
          BLI_assert(0);
      }
    }
    /* Reset state */
    glFrontFace(DST.frontface);
  }

  if (use_tfeedback) {
    GPU_shader_transform_feedback_disable(shgroup->shader);
  }
}

static void drw_update_view(void)
{
  if (DST.dirty_mat) {
    DST.state_cache_id++;
    DST.dirty_mat = false;

    DRW_uniformbuffer_update(G_draw.view_ubo, &DST.view_data);

    /* Catch integer wrap around. */
    if (UNLIKELY(DST.state_cache_id == 0)) {
      DST.state_cache_id = 1;
      /* We must reset all CallStates to ensure that not
       * a single one stayed with cache_id equal to 1. */
      BLI_memblock_iter iter;
      DRWCallState *state;
      BLI_memblock_iternew(DST.vmempool->states, &iter);
      while ((state = BLI_memblock_iterstep(&iter))) {
        state->cache_id = 0;
      }
    }

    /* TODO dispatch threads to compute matrices/culling */
  }

  draw_clipping_setup_from_view();
}

static void drw_draw_pass_ex(DRWPass *pass,
                             DRWShadingGroup *start_group,
                             DRWShadingGroup *end_group)
{
  if (start_group == NULL) {
    return;
  }

  DST.shader = NULL;

  BLI_assert(DST.buffer_finish_called &&
             "DRW_render_instance_buffer_finish had not been called before drawing");

  drw_update_view();

  /* GPU_framebuffer_clear calls can change the state outside the DRW module.
   * Force reset the affected states to avoid problems later. */
  drw_state_set(DST.state | DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR);

  drw_state_set(pass->state);

  DRW_stats_query_start(pass->name);

  for (DRWShadingGroup *shgroup = start_group; shgroup; shgroup = shgroup->next) {
    draw_shgroup(shgroup, pass->state);
    /* break if upper limit */
    if (shgroup == end_group) {
      break;
    }
  }

  /* Clear Bound textures */
  for (int i = 0; i < DST_MAX_SLOTS; i++) {
    if (DST.RST.bound_texs[i] != NULL) {
      GPU_texture_unbind(DST.RST.bound_texs[i]);
      DST.RST.bound_texs[i] = NULL;
    }
  }

  /* Clear Bound Ubos */
  for (int i = 0; i < DST_MAX_SLOTS; i++) {
    if (DST.RST.bound_ubos[i] != NULL) {
      GPU_uniformbuffer_unbind(DST.RST.bound_ubos[i]);
      DST.RST.bound_ubos[i] = NULL;
    }
  }

  if (DST.shader) {
    GPU_shader_unbind();
    DST.shader = NULL;
  }

  /* HACK: Rasterized discard can affect clear commands which are not
   * part of a DRWPass (as of now). So disable rasterized discard here
   * if it has been enabled. */
  if ((DST.state & DRW_STATE_RASTERIZER_ENABLED) == 0) {
    drw_state_set((DST.state & ~DRW_STATE_RASTERIZER_ENABLED) | DRW_STATE_DEFAULT);
  }

  DRW_stats_query_end();
}

void DRW_draw_pass(DRWPass *pass)
{
  drw_draw_pass_ex(pass, pass->shgroups.first, pass->shgroups.last);
}

/* Draw only a subset of shgroups. Used in special situations as grease pencil strokes */
void DRW_draw_pass_subset(DRWPass *pass, DRWShadingGroup *start_group, DRWShadingGroup *end_group)
{
  drw_draw_pass_ex(pass, start_group, end_group);
}

/** \} */
