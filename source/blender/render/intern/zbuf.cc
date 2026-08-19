/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

/*---------------------------------------------------------------------------*/
/* Common includes                                                           */
/*---------------------------------------------------------------------------*/

#include <algorithm>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_math_base.hh"
#include "BLI_math_base_c.hh"

/* own includes */
#include "zbuf.h"

namespace blender {

/* could enable at some point but for now there are far too many conversions */
#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

/* ****************** Spans ******************************* */

void zbuf_alloc_span(ZSpan *zspan, int rectx, int recty)
{
  memset(zspan, 0, sizeof(ZSpan));

  zspan->rectx = rectx;
  zspan->recty = recty;

  zspan->span1 = MEM_new_array_uninitialized<float>(recty, "zspan");
  zspan->span2 = MEM_new_array_uninitialized<float>(recty, "zspan");
}

void zbuf_free_span(ZSpan *zspan)
{
  if (zspan) {
    MEM_SAFE_DELETE(zspan->span1);
    MEM_SAFE_DELETE(zspan->span2);
  }
}

/* reset range for clipping */
static void zbuf_init_span(ZSpan *zspan)
{
  zspan->miny1 = zspan->miny2 = zspan->recty + 1;
  zspan->maxy1 = zspan->maxy2 = -1;
  zspan->minp1 = zspan->maxp1 = zspan->minp2 = zspan->maxp2 = nullptr;
}

static void zbuf_add_to_span(ZSpan *zspan, const float v1[2], const float v2[2])
{
  const float *minv, *maxv;
  float *span;
  float xx1, dx0, xs0;
  int y, my0, my2;

  if (v1[1] < v2[1]) {
    minv = v1;
    maxv = v2;
  }
  else {
    minv = v2;
    maxv = v1;
  }

  my0 = ceil(minv[1]);
  my2 = floor(maxv[1]);

  if (my2 < 0 || my0 >= zspan->recty) {
    return;
  }

  /* clip top */
  if (my2 >= zspan->recty) {
    my2 = zspan->recty - 1;
  }
  /* clip bottom */
  my0 = std::max(my0, 0);

  if (my0 > my2) {
    return;
  }
  /* if (my0>my2) should still fill in, that way we get spans that skip nicely */

  xx1 = maxv[1] - minv[1];
  if (xx1 > FLT_EPSILON) {
    dx0 = (minv[0] - maxv[0]) / xx1;
    xs0 = dx0 * (minv[1] - my2) + minv[0];
  }
  else {
    dx0 = 0.0f;
    xs0 = min_ff(minv[0], maxv[0]);
  }

  /* empty span */
  if (zspan->maxp1 == nullptr) {
    span = zspan->span1;
  }
  else { /* does it complete left span? */
    if (maxv == zspan->minp1 || minv == zspan->maxp1) {
      span = zspan->span1;
    }
    else {
      span = zspan->span2;
    }
  }

  if (span == zspan->span1) {
    //      printf("left span my0 %d my2 %d\n", my0, my2);
    if (zspan->minp1 == nullptr || zspan->minp1[1] > minv[1]) {
      zspan->minp1 = minv;
    }
    if (zspan->maxp1 == nullptr || zspan->maxp1[1] < maxv[1]) {
      zspan->maxp1 = maxv;
    }
    zspan->miny1 = std::min(my0, zspan->miny1);
    zspan->maxy1 = std::max(my2, zspan->maxy1);
  }
  else {
    //      printf("right span my0 %d my2 %d\n", my0, my2);
    if (zspan->minp2 == nullptr || zspan->minp2[1] > minv[1]) {
      zspan->minp2 = minv;
    }
    if (zspan->maxp2 == nullptr || zspan->maxp2[1] < maxv[1]) {
      zspan->maxp2 = maxv;
    }
    zspan->miny2 = std::min(my0, zspan->miny2);
    zspan->maxy2 = std::max(my2, zspan->maxy2);
  }

  for (y = my2; y >= my0; y--, xs0 += dx0) {
    /* xs0 is the X-coordinate! */
    span[y] = xs0;
  }
}

/*-----------------------------------------------------------*/
/* Functions                                                 */
/*-----------------------------------------------------------*/

/* NOTE(@ideasman42): workaround for pixel aligned UVs which are common and can screw
 * up our intersection tests where a pixel gets in between 2 faces or the middle of a
 * quad, camera aligned quads also have this problem but they are less common. Add a
 * small offset to the UVs, fixes bug #18685. */
constexpr float ZSPAN_X_EPSILON = 0.001f;
constexpr float ZSPAN_Y_EPSILON = 0.002f;
constexpr float ZSPAN_X_OFFSET = 0.5f + ZSPAN_X_EPSILON;
constexpr float ZSPAN_Y_OFFSET = 0.5f + ZSPAN_Y_EPSILON;

void zspan_rasterize_triangle(ZSpan *zspan,
                              void *handle,
                              const float *v1_in,
                              const float *v2_in,
                              const float *v3_in,
                              void (*func)(void *, int, int, float, float))
{
  float x0, y0, x1, y1, x2, y2, z0, z1, z2;
  float u, v, uxd, uyd, vxd, vyd, uy0, vy0, xx1;
  const float *span1, *span2;
  int i, j, x, y, sn1, sn2, rectx = zspan->rectx, my0, my2;

  /* init */
  zbuf_init_span(zspan);

  const float v1[2] = {v1_in[0] - ZSPAN_X_OFFSET, v1_in[1] - ZSPAN_Y_OFFSET};
  const float v2[2] = {v2_in[0] - ZSPAN_X_OFFSET, v2_in[1] - ZSPAN_Y_OFFSET};
  const float v3[2] = {v3_in[0] - ZSPAN_X_OFFSET, v3_in[1] - ZSPAN_Y_OFFSET};

  /* set spans */
  zbuf_add_to_span(zspan, v1, v2);
  zbuf_add_to_span(zspan, v2, v3);
  zbuf_add_to_span(zspan, v3, v1);

  /* clipped */
  if (zspan->minp2 == nullptr || zspan->maxp2 == nullptr) {
    return;
  }

  my0 = max_ii(zspan->miny1, zspan->miny2);
  my2 = min_ii(zspan->maxy1, zspan->maxy2);

  //  printf("my %d %d\n", my0, my2);
  if (my2 < my0) {
    return;
  }

  /* ZBUF DX DY, in floats still */
  x1 = v1[0] - v2[0];
  x2 = v2[0] - v3[0];
  y1 = v1[1] - v2[1];
  y2 = v2[1] - v3[1];

  z1 = 1.0f; /* (u1 - u2) */
  z2 = 0.0f; /* (u2 - u3) */

  x0 = y1 * z2 - z1 * y2;
  y0 = z1 * x2 - x1 * z2;
  z0 = x1 * y2 - y1 * x2;

  if (z0 == 0.0f) {
    return;
  }

  xx1 = (x0 * v1[0] + y0 * v1[1]) / z0 + 1.0f;
  uxd = -double(x0) / double(z0);
  uyd = -double(y0) / double(z0);
  uy0 = double(my2) * uyd + double(xx1);

  z1 = -1.0f; /* (v1 - v2) */
  z2 = 1.0f;  /* (v2 - v3) */

  x0 = y1 * z2 - z1 * y2;
  y0 = z1 * x2 - x1 * z2;

  xx1 = (x0 * v1[0] + y0 * v1[1]) / z0;
  vxd = -double(x0) / double(z0);
  vyd = -double(y0) / double(z0);
  vy0 = double(my2) * vyd + double(xx1);

  /* correct span */
  span1 = zspan->span1 + my2;
  span2 = zspan->span2 + my2;

  for (i = 0, y = my2; y >= my0; i++, y--, span1--, span2--) {

    sn1 = floor(min_ff(*span1, *span2));
    sn2 = floor(max_ff(*span1, *span2));
    sn1++;

    if (sn2 >= rectx) {
      sn2 = rectx - 1;
    }
    sn1 = std::max(sn1, 0);

    u = ((double(sn1) * uxd) + uy0) - (i * uyd);
    v = ((double(sn1) * vxd) + vy0) - (i * vyd);

    for (j = 0, x = sn1; x <= sn2; j++, x++) {
      func(handle, x, y, u + (j * uxd), v + (j * vxd));
    }
  }
}

static void zspan_rasterize_conservative_line(ZSpan *zspan,
                                              void *handle,
                                              const float *v1_in,
                                              const float *v2_in,
                                              const float *uv1,
                                              const float *uv2,
                                              void (*func)(void *, int, int, float, float))
{
  const float v1[2] = {v1_in[0] - ZSPAN_X_OFFSET, v1_in[1] - ZSPAN_Y_OFFSET};
  const float v2[2] = {v2_in[0] - ZSPAN_X_OFFSET, v2_in[1] - ZSPAN_Y_OFFSET};

  const float miny = std::min(v1[1], v2[1]);
  const float maxy = std::max(v1[1], v2[1]);

  int y0 = floor(miny + 0.5f);
  int y1 = floor(maxy + 0.5f);

  /* Clip and cull the line. */
  y0 = std::max(y0, 0);
  if (y1 >= zspan->recty) {
    y1 = zspan->recty - 1;
  }

  if (y0 > y1) {
    return;
  }

  /* Line direction divided by length for uv interpolation. */
  float dir[2] = {v2[0] - v1[0], v2[1] - v1[1]};
  float len_sqr = dir[0] * dir[0] + dir[1] * dir[1];

  if (len_sqr > FLT_EPSILON) {
    dir[0] /= len_sqr;
    dir[1] /= len_sqr;
  }

  /* X coordinates of the line endpoints. */
  const float x_miny = (v1[1] < v2[1]) ? v1[0] : v2[0];
  const float x_maxy = (v1[1] < v2[1]) ? v2[0] : v1[0];
  const float inv_dy = math::safe_divide(1.0f, maxy - miny);

  for (; y0 <= y1; y0++) {
    /* Compute line x range inside current scanline, interpolating between endpoints. */
    const float ta = (clamp_f(y0 - 0.5f, miny, maxy) - miny) * inv_dy;
    const float tb = (maxy - clamp_f(y0 + 0.5f, miny, maxy)) * inv_dy;
    const float xa = math::interpolate(x_miny, x_maxy, ta);
    const float xb = math::interpolate(x_miny, x_maxy, 1.0f - tb);

    const float minx = std::min(xa, xb);
    const float maxx = std::max(xa, xb);

    int x0 = floor(minx + 0.5f);
    int x1 = floor(maxx + 0.5f);

    /* Clip and cull scanline. */
    x0 = std::max(x0, 0);
    if (x1 >= zspan->rectx) {
      x1 = zspan->rectx - 1;
    }

    if (x0 > x1) {
      continue;
    }

    for (; x0 <= x1; x0++) {
      float w = (x0 - v1[0]) * dir[0] + (y0 - v1[1]) * dir[1];
      w = clamp_f(w, 0, 1);

      float u = (1 - w) * uv1[0] + w * uv2[0];
      float v = (1 - w) * uv1[1] + w * uv2[1];

      func(handle, x0, y0, u, v);
    }
  }
}

void zspan_rasterize_conservative_wireframe(ZSpan *zspan,
                                            void *handle,
                                            const float *v1,
                                            const float *v2,
                                            const float *v3,
                                            void (*func)(void *, int, int, float, float))
{
  float uv1[2] = {1, 0};
  float uv2[2] = {0, 1};
  float uv3[2] = {0, 0};

  zspan_rasterize_conservative_line(zspan, handle, v1, v2, uv1, uv2, func);
  zspan_rasterize_conservative_line(zspan, handle, v2, v3, uv2, uv3, func);
  zspan_rasterize_conservative_line(zspan, handle, v3, v1, uv3, uv1, func);
}

}  // namespace blender
