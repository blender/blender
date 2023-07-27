/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 *
 * \note Some of this logic has been duplicated in `COM_VectorBlurOperation.cc`
 * changes here may also apply also apply to that file.
 */

/*---------------------------------------------------------------------------*/
/* Common includes                                                           */
/*---------------------------------------------------------------------------*/

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"

/* own includes */
#include "zbuf.h"

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

  zspan->span1 = static_cast<float *>(MEM_mallocN(recty * sizeof(float), "zspan"));
  zspan->span2 = static_cast<float *>(MEM_mallocN(recty * sizeof(float), "zspan"));
}

void zbuf_free_span(ZSpan *zspan)
{
  if (zspan) {
    MEM_SAFE_FREE(zspan->span1);
    MEM_SAFE_FREE(zspan->span2);
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
  if (my0 < 0) {
    my0 = 0;
  }

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
    if (my0 < zspan->miny1) {
      zspan->miny1 = my0;
    }
    if (my2 > zspan->maxy1) {
      zspan->maxy1 = my2;
    }
  }
  else {
    //      printf("right span my0 %d my2 %d\n", my0, my2);
    if (zspan->minp2 == nullptr || zspan->minp2[1] > minv[1]) {
      zspan->minp2 = minv;
    }
    if (zspan->maxp2 == nullptr || zspan->maxp2[1] < maxv[1]) {
      zspan->maxp2 = maxv;
    }
    if (my0 < zspan->miny2) {
      zspan->miny2 = my0;
    }
    if (my2 > zspan->maxy2) {
      zspan->maxy2 = my2;
    }
  }

  for (y = my2; y >= my0; y--, xs0 += dx0) {
    /* xs0 is the X-coordinate! */
    span[y] = xs0;
  }
}

/*-----------------------------------------------------------*/
/* Functions                                                 */
/*-----------------------------------------------------------*/

void zspan_scanconvert(ZSpan *zspan,
                       void *handle,
                       float *v1,
                       float *v2,
                       float *v3,
                       void (*func)(void *, int, int, float, float))
{
  float x0, y0, x1, y1, x2, y2, z0, z1, z2;
  float u, v, uxd, uyd, vxd, vyd, uy0, vy0, xx1;
  const float *span1, *span2;
  int i, j, x, y, sn1, sn2, rectx = zspan->rectx, my0, my2;

  /* init */
  zbuf_init_span(zspan);

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
    if (sn1 < 0) {
      sn1 = 0;
    }

    u = ((double(sn1) * uxd) + uy0) - (i * uyd);
    v = ((double(sn1) * vxd) + vy0) - (i * vyd);

    for (j = 0, x = sn1; x <= sn2; j++, x++) {
      func(handle, x, y, u + (j * uxd), v + (j * vxd));
    }
  }
}

/* end of zbuf.c */
