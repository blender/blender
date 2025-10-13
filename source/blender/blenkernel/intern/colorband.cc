/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_heap.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_key_types.h"
#include "DNA_texture_types.h"

#include "BKE_colorband.hh"
#include "BKE_idtype.hh"
#include "BKE_key.hh"

void BKE_colorband_init(ColorBand *coba, bool rangetype)
{
  int a;

  coba->data[0].pos = 0.0;
  coba->data[1].pos = 1.0;

  if (rangetype == 0) {
    coba->data[0].r = 0.0;
    coba->data[0].g = 0.0;
    coba->data[0].b = 0.0;
    coba->data[0].a = 0.0;

    coba->data[1].r = 1.0;
    coba->data[1].g = 1.0;
    coba->data[1].b = 1.0;
    coba->data[1].a = 1.0;
  }
  else {
    coba->data[0].r = 0.0;
    coba->data[0].g = 0.0;
    coba->data[0].b = 0.0;
    coba->data[0].a = 1.0;

    coba->data[1].r = 1.0;
    coba->data[1].g = 1.0;
    coba->data[1].b = 1.0;
    coba->data[1].a = 1.0;
  }

  for (a = 2; a < MAXCOLORBAND; a++) {
    coba->data[a].r = 0.5;
    coba->data[a].g = 0.5;
    coba->data[a].b = 0.5;
    coba->data[a].a = 1.0;
    coba->data[a].pos = 0.5;
  }

  coba->tot = 2;
  coba->cur = 0;
  coba->color_mode = COLBAND_BLEND_RGB;
  coba->ipotype = COLBAND_INTERP_LINEAR;
}

static void colorband_init_from_table_rgba_simple(ColorBand *coba,
                                                  const float (*array)[4],
                                                  const int array_len)
{
  /* No Re-sample, just de-duplicate. */
  const float eps = (1.0f / 255.0f) + 1e-6f;
  BLI_assert(array_len < MAXCOLORBAND);
  int stops = min_ii(MAXCOLORBAND, array_len);
  if (stops) {
    const float step_size = 1.0f / float(max_ii(stops - 1, 1));
    int i_curr = -1;
    for (int i_step = 0; i_step < stops; i_step++) {
      if ((i_curr != -1) && compare_v4v4(&coba->data[i_curr].r, array[i_step], eps)) {
        continue;
      }
      i_curr += 1;
      copy_v4_v4(&coba->data[i_curr].r, array[i_step]);
      coba->data[i_curr].pos = i_step * step_size;
      coba->data[i_curr].cur = i_curr;
    }
    coba->tot = i_curr + 1;
    coba->cur = 0;
  }
  else {
    /* coba is empty, set 1 black stop */
    zero_v3(&coba->data[0].r);
    coba->data[0].a = 1.0f;
    coba->cur = 0;
    coba->tot = 1;
  }
}

/* -------------------------------------------------------------------- */
/** \name Color Ramp Re-Sample
 *
 * Local functions for #BKE_colorband_init_from_table_rgba
 * \{ */

/**
 * Used for calculating which samples of a color-band to remove (when simplifying).
 */
struct ColorResampleElem {
  ColorResampleElem *next, *prev;
  HeapNode *node;
  float rgba[4];
  float pos;
};

/**
 * Measure the 'area' of each channel and combine to use as a cost for this samples removal.
 */
static float color_sample_remove_cost(const ColorResampleElem *c)
{
  if (c->next == nullptr || c->prev == nullptr) {
    return -1.0f;
  }
  float area = 0.0f;
#if 0
  float xy_prev[2], xy_curr[2], xy_next[2];
  xy_prev[0] = c->prev->pos;
  xy_curr[0] = c->pos;
  xy_next[0] = c->next->pos;
  for (int i = 0; i < 4; i++) {
    xy_prev[1] = c->prev->rgba[i];
    xy_curr[1] = c->rgba[i];
    xy_next[1] = c->next->rgba[i];
    area += fabsf(cross_tri_v2(xy_prev, xy_curr, xy_next));
  }
#else
  /* Above logic, optimized (p: previous, c: current, n: next). */
  const float xpc = c->prev->pos - c->pos;
  const float xnc = c->next->pos - c->pos;
  for (int i = 0; i < 4; i++) {
    const float ycn = c->rgba[i] - c->next->rgba[i];
    const float ypc = c->prev->rgba[i] - c->rgba[i];
    area += fabsf((xpc * ycn) + (ypc * xnc));
  }
#endif
  return area;
}

/* TODO(@ideasman42): create `BLI_math_filter` ? */
static float filter_gauss(float x)
{
  const float gaussfac = 1.6f;
  const float two_gaussfac2 = 2.0f * gaussfac * gaussfac;
  x *= 3.0f * gaussfac;
  return 1.0f / sqrtf(float(M_PI) * two_gaussfac2) * expf(-x * x / two_gaussfac2);
}

static void colorband_init_from_table_rgba_resample(ColorBand *coba,
                                                    const float (*array)[4],
                                                    const int array_len,
                                                    bool filter_samples)
{
  BLI_assert(array_len >= 2);
  const float eps_2x = ((1.0f / 255.0f) + 1e-6f);
  ColorResampleElem *c, *carr = MEM_malloc_arrayN<ColorResampleElem>(size_t(array_len), __func__);
  int carr_len = array_len;
  c = carr;
  {
    const float step_size = 1.0f / float(array_len - 1);
    for (int i = 0; i < array_len; i++, c++) {
      copy_v4_v4(carr[i].rgba, array[i]);
      c->next = c + 1;
      c->prev = c - 1;
      c->pos = i * step_size;
    }
  }
  carr[0].prev = nullptr;
  carr[array_len - 1].next = nullptr;

  /* -2 to remove endpoints. */
  Heap *heap = BLI_heap_new_ex(array_len - 2);
  c = carr;
  for (int i = 0; i < array_len; i++, c++) {
    float cost = color_sample_remove_cost(c);
    if (cost != -1.0f) {
      c->node = BLI_heap_insert(heap, cost, c);
    }
    else {
      c->node = nullptr;
    }
  }

  while ((carr_len > 1 && !BLI_heap_is_empty(heap)) &&
         ((carr_len >= MAXCOLORBAND) || (BLI_heap_top_value(heap) <= eps_2x)))
  {
    c = static_cast<ColorResampleElem *>(BLI_heap_pop_min(heap));
    ColorResampleElem *c_next = c->next, *c_prev = c->prev;
    c_prev->next = c_next;
    c_next->prev = c_prev;
    /* Clear data (not essential, avoid confusion). */
    c->prev = c->next = nullptr;
    c->node = nullptr;

    /* Update adjacent */
    for (int i = 0; i < 2; i++) {
      ColorResampleElem *c_other = i ? c_next : c_prev;
      if (c_other->node != nullptr) {
        const float cost = color_sample_remove_cost(c_other);
        if (cost != -1.0) {
          BLI_heap_node_value_update(heap, c_other->node, cost);
        }
        else {
          BLI_heap_remove(heap, c_other->node);
          c_other->node = nullptr;
        }
      }
    }
    carr_len -= 1;
  }
  BLI_heap_free(heap, nullptr);

  /* First member is never removed. */
  int i = 0;
  BLI_assert(carr_len < MAXCOLORBAND);
  if (filter_samples == false) {
    for (c = carr; c != nullptr; c = c->next, i++) {
      copy_v4_v4(&coba->data[i].r, c->rgba);
      coba->data[i].pos = c->pos;
      coba->data[i].cur = i;
    }
  }
  else {
    for (c = carr; c != nullptr; c = c->next, i++) {
      const int steps_prev = c->prev ? (c - c->prev) - 1 : 0;
      const int steps_next = c->next ? (c->next - c) - 1 : 0;
      if (steps_prev == 0 && steps_next == 0) {
        copy_v4_v4(&coba->data[i].r, c->rgba);
      }
      else {
        float rgba[4];
        float rgba_accum = 1;
        copy_v4_v4(rgba, c->rgba);

        if (steps_prev) {
          const float step_size = 1.0 / float(steps_prev + 1);
          int j = steps_prev;
          for (ColorResampleElem *c_other = c - 1; c_other != c->prev; c_other--, j--) {
            const float step_pos = float(j) * step_size;
            BLI_assert(step_pos > 0.0f && step_pos < 1.0f);
            const float f = filter_gauss(step_pos);
            madd_v4_v4fl(rgba, c_other->rgba, f);
            rgba_accum += f;
          }
        }
        if (steps_next) {
          const float step_size = 1.0 / float(steps_next + 1);
          int j = steps_next;
          for (ColorResampleElem *c_other = c + 1; c_other != c->next; c_other++, j--) {
            const float step_pos = float(j) * step_size;
            BLI_assert(step_pos > 0.0f && step_pos < 1.0f);
            const float f = filter_gauss(step_pos);
            madd_v4_v4fl(rgba, c_other->rgba, f);
            rgba_accum += f;
          }
        }

        mul_v4_v4fl(&coba->data[i].r, rgba, 1.0f / rgba_accum);
      }
      coba->data[i].pos = c->pos;
      coba->data[i].cur = i;
    }
  }
  BLI_assert(i == carr_len);
  coba->tot = i;
  coba->cur = 0;

  MEM_freeN(carr);
}

void BKE_colorband_init_from_table_rgba(ColorBand *coba,
                                        const float (*array)[4],
                                        const int array_len,
                                        bool filter_samples)
{
  /* NOTE: we could use MAXCOLORBAND here, but results of re-sampling are nicer,
   * avoid different behavior when limit is hit. */
  if (array_len < 2) {
    /* No Re-sample, just de-duplicate. */
    colorband_init_from_table_rgba_simple(coba, array, array_len);
  }
  else {
    /* Re-sample */
    colorband_init_from_table_rgba_resample(coba, array, array_len, filter_samples);
  }
}

/** \} */

ColorBand *BKE_colorband_add(bool rangetype)
{
  ColorBand *coba;

  coba = MEM_callocN<ColorBand>("colorband");
  BKE_colorband_init(coba, rangetype);

  return coba;
}

/* ------------------------------------------------------------------------- */

static float colorband_hue_interp(
    const int ipotype_hue, const float mfac, const float fac, float h1, float h2)
{
  float h_interp;
  int mode = 0;

#define HUE_INTERP(h_a, h_b) ((mfac * (h_a)) + (fac * (h_b)))
#define HUE_MOD(h) (((h) < 1.0f) ? (h) : (h) - 1.0f)

  h1 = HUE_MOD(h1);
  h2 = HUE_MOD(h2);

  BLI_assert(h1 >= 0.0f && h1 < 1.0f);
  BLI_assert(h2 >= 0.0f && h2 < 1.0f);

  switch (ipotype_hue) {
    case COLBAND_HUE_NEAR: {
      if ((h1 < h2) && (h2 - h1) > +0.5f) {
        mode = 1;
      }
      else if ((h1 > h2) && (h2 - h1) < -0.5f) {
        mode = 2;
      }
      else {
        mode = 0;
      }
      break;
    }
    case COLBAND_HUE_FAR: {
      /* Do full loop in Hue space in case both stops are the same... */
      if (h1 == h2) {
        mode = 1;
      }
      else if ((h1 < h2) && (h2 - h1) < +0.5f) {
        mode = 1;
      }
      else if ((h1 > h2) && (h2 - h1) > -0.5f) {
        mode = 2;
      }
      else {
        mode = 0;
      }
      break;
    }
    case COLBAND_HUE_CCW: {
      if (h1 > h2) {
        mode = 2;
      }
      else {
        mode = 0;
      }
      break;
    }
    case COLBAND_HUE_CW: {
      if (h1 < h2) {
        mode = 1;
      }
      else {
        mode = 0;
      }
      break;
    }
  }

  switch (mode) {
    case 0:
      h_interp = HUE_INTERP(h1, h2);
      break;
    case 1:
      h_interp = HUE_INTERP(h1 + 1.0f, h2);
      h_interp = HUE_MOD(h_interp);
      break;
    case 2:
      h_interp = HUE_INTERP(h1, h2 + 1.0f);
      h_interp = HUE_MOD(h_interp);
      break;
  }

  BLI_assert(h_interp >= 0.0f && h_interp < 1.0f);

#undef HUE_INTERP
#undef HUE_MOD

  return h_interp;
}

bool BKE_colorband_evaluate(const ColorBand *coba, float in, float out[4])
{
  const CBData *cbd1, *cbd2, *cbd0, *cbd3;
  float fac;
  int ipotype;
  int a;

  if (coba == nullptr || coba->tot == 0) {
    return false;
  }

  cbd1 = coba->data;

  /* NOTE: when ipotype >= COLBAND_INTERP_B_SPLINE,
   * we cannot do early-out with a constant color before first color stop and after last one,
   * because interpolation starts before and ends after those... */
  ipotype = (coba->color_mode == COLBAND_BLEND_RGB) ? coba->ipotype : int(COLBAND_INTERP_LINEAR);

  if (coba->tot == 1) {
    out[0] = cbd1->r;
    out[1] = cbd1->g;
    out[2] = cbd1->b;
    out[3] = cbd1->a;
  }
  else if ((in <= cbd1->pos) &&
           ELEM(ipotype, COLBAND_INTERP_LINEAR, COLBAND_INTERP_EASE, COLBAND_INTERP_CONSTANT))
  {
    /* We are before first color stop. */
    out[0] = cbd1->r;
    out[1] = cbd1->g;
    out[2] = cbd1->b;
    out[3] = cbd1->a;
  }
  else {
    CBData left, right;

    /* we're looking for first pos > in */
    for (a = 0; a < coba->tot; a++, cbd1++) {
      if (cbd1->pos > in) {
        break;
      }
    }

    if (a == coba->tot) {
      cbd2 = cbd1 - 1;
      right = *cbd2;
      right.pos = 1.0f;
      cbd1 = &right;
    }
    else if (a == 0) {
      left = *cbd1;
      left.pos = 0.0f;
      cbd2 = &left;
    }
    else {
      cbd2 = cbd1 - 1;
    }

    if ((a == coba->tot) &&
        ELEM(ipotype, COLBAND_INTERP_LINEAR, COLBAND_INTERP_EASE, COLBAND_INTERP_CONSTANT))
    {
      /* We are after last color stop. */
      out[0] = cbd2->r;
      out[1] = cbd2->g;
      out[2] = cbd2->b;
      out[3] = cbd2->a;
    }
    else if (ipotype == COLBAND_INTERP_CONSTANT) {
      /* constant */
      out[0] = cbd2->r;
      out[1] = cbd2->g;
      out[2] = cbd2->b;
      out[3] = cbd2->a;
    }
    else {
      if (cbd2->pos != cbd1->pos) {
        fac = (in - cbd1->pos) / (cbd2->pos - cbd1->pos);
      }
      else {
        /* was setting to 0.0 in 2.56 & previous, but this
         * is incorrect for the last element, see #26732. */
        fac = (a != coba->tot) ? 0.0f : 1.0f;
      }

      if (ELEM(ipotype, COLBAND_INTERP_B_SPLINE, COLBAND_INTERP_CARDINAL)) {
        /* Interpolate from right to left: `3 2 1 0`. */
        float t[4];

        if (a >= coba->tot - 1) {
          cbd0 = cbd1;
        }
        else {
          cbd0 = cbd1 + 1;
        }
        if (a < 2) {
          cbd3 = cbd2;
        }
        else {
          cbd3 = cbd2 - 1;
        }

        CLAMP(fac, 0.0f, 1.0f);

        if (ipotype == COLBAND_INTERP_CARDINAL) {
          key_curve_position_weights(fac, t, KEY_CARDINAL);
        }
        else {
          key_curve_position_weights(fac, t, KEY_BSPLINE);
        }

        out[0] = t[3] * cbd3->r + t[2] * cbd2->r + t[1] * cbd1->r + t[0] * cbd0->r;
        out[1] = t[3] * cbd3->g + t[2] * cbd2->g + t[1] * cbd1->g + t[0] * cbd0->g;
        out[2] = t[3] * cbd3->b + t[2] * cbd2->b + t[1] * cbd1->b + t[0] * cbd0->b;
        out[3] = t[3] * cbd3->a + t[2] * cbd2->a + t[1] * cbd1->a + t[0] * cbd0->a;
        clamp_v4(out, 0.0f, 1.0f);
      }
      else {
        if (ipotype == COLBAND_INTERP_EASE) {
          const float fac2 = fac * fac;
          fac = 3.0f * fac2 - 2.0f * fac2 * fac;
        }
        const float mfac = 1.0f - fac;

        if (UNLIKELY(coba->color_mode == COLBAND_BLEND_HSV)) {
          float col1[3], col2[3];

          rgb_to_hsv_v(&cbd1->r, col1);
          rgb_to_hsv_v(&cbd2->r, col2);

          out[0] = colorband_hue_interp(coba->ipotype_hue, mfac, fac, col1[0], col2[0]);
          out[1] = mfac * col1[1] + fac * col2[1];
          out[2] = mfac * col1[2] + fac * col2[2];
          out[3] = mfac * cbd1->a + fac * cbd2->a;

          hsv_to_rgb_v(out, out);
        }
        else if (UNLIKELY(coba->color_mode == COLBAND_BLEND_HSL)) {
          float col1[3], col2[3];

          rgb_to_hsl_v(&cbd1->r, col1);
          rgb_to_hsl_v(&cbd2->r, col2);

          out[0] = colorband_hue_interp(coba->ipotype_hue, mfac, fac, col1[0], col2[0]);
          out[1] = mfac * col1[1] + fac * col2[1];
          out[2] = mfac * col1[2] + fac * col2[2];
          out[3] = mfac * cbd1->a + fac * cbd2->a;

          hsl_to_rgb_v(out, out);
        }
        else {
          /* COLBAND_BLEND_RGB */
          out[0] = mfac * cbd1->r + fac * cbd2->r;
          out[1] = mfac * cbd1->g + fac * cbd2->g;
          out[2] = mfac * cbd1->b + fac * cbd2->b;
          out[3] = mfac * cbd1->a + fac * cbd2->a;
        }
      }
    }
  }

  return true; /* OK */
}

void BKE_colorband_evaluate_table_rgba(const ColorBand *coba, float **array, int *size)
{
  int a;

  *size = CM_TABLE + 1;
  *array = MEM_calloc_arrayN<float>(4 * size_t(*size), "ColorBand");

  for (a = 0; a < *size; a++) {
    BKE_colorband_evaluate(coba, float(a) / float(CM_TABLE), &(*array)[a * 4]);
  }
}

static int vergcband(const void *a1, const void *a2)
{
  const CBData *x1 = static_cast<const CBData *>(a1), *x2 = static_cast<const CBData *>(a2);

  if (x1->pos > x2->pos) {
    return 1;
  }
  if (x1->pos < x2->pos) {
    return -1;
  }
  return 0;
}

void BKE_colorband_update_sort(ColorBand *coba)
{
  int a;

  if (coba->tot < 2) {
    return;
  }

  for (a = 0; a < coba->tot; a++) {
    coba->data[a].cur = a;
  }

  qsort(coba->data, coba->tot, sizeof(CBData), vergcband);

  for (a = 0; a < coba->tot; a++) {
    if (coba->data[a].cur == coba->cur) {
      coba->cur = a;
      break;
    }
  }
}

CBData *BKE_colorband_element_add(ColorBand *coba, float position)
{
  if (coba->tot == MAXCOLORBAND) {
    return nullptr;
  }

  CBData *xnew;

  xnew = &coba->data[coba->tot];
  xnew->pos = position;

  if (coba->tot != 0) {
    BKE_colorband_evaluate(coba, position, &xnew->r);
  }
  else {
    zero_v4(&xnew->r);
  }

  coba->tot++;
  coba->cur = coba->tot - 1;

  BKE_colorband_update_sort(coba);

  return coba->data + coba->cur;
}

bool BKE_colorband_element_remove(ColorBand *coba, int index)
{
  if (coba->tot < 2) {
    return false;
  }

  if (index < 0 || index >= coba->tot) {
    return false;
  }

  coba->tot--;
  for (int a = index; a < coba->tot; a++) {
    coba->data[a] = coba->data[a + 1];
  }
  if (coba->cur) {
    coba->cur--;
  }
  return true;
}

void BKE_colorband_foreach_working_space_color(ColorBand *coba,
                                               const IDTypeForeachColorFunctionCallback &fn)
{
  for (int a = 0; a < coba->tot; a++) {
    fn.single(&coba->data[a].r);
  }
}
