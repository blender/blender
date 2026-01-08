/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blf
 *
 * Glyph conversion, from FreeType to curves.
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ft2build.h>

#include FT_OUTLINE_H

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_geom.h"

#include "BLF_api.hh"

#include "DNA_curve_types.h"

#include "blf_internal.hh"
#include "blf_internal_types.hh"

#include "BLI_math_vector.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/**
 * Convert a floating point value to a FreeType 16.16 fixed point value.
 */
static FT_Fixed to_16dot16(const double val)
{
  return FT_Fixed(lround(val * 65536.0));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Convert Glyph to Curves
 * \{ */

/**
 * from: http://www.freetype.org/freetype2/docs/glyphs/glyphs-6.html#section-1
 *
 * Vectorial representation of Freetype glyphs
 *
 * The source format of outlines is a collection of closed paths called "contours". Each contour is
 * made of a series of line segments and bezier arcs. Depending on the file format, these can be
 * second-order or third-order polynomials. The former are also called quadratic or conic arcs, and
 * they come from the TrueType format. The latter are called cubic arcs and mostly come from the
 * Type1 format.
 *
 * Each arc is described through a series of start, end and control points.
 * Each point of the outline has a specific tag which indicates whether it is
 * used to describe a line segment or an arc.
 * The following rules are applied to decompose the contour's points into segments and arcs :
 *
 * # two successive "on" points indicate a line segment joining them.
 *
 * # one conic "off" point midst two "on" points indicates a conic bezier arc,
 *   the "off" point being the control point, and the "on" ones the start and end points.
 *
 * # Two successive cubic "off" points midst two "on" points indicate a cubic bezier arc.
 *   There must be exactly two cubic control points and two on points for each cubic arc
 *   (using a single cubic "off" point between two "on" points is forbidden, for example).
 *
 * # finally, two successive conic "off" points forces the rasterizer to create
 *   (during the scan-line conversion process exclusively) a virtual "on" point midst them,
 *   at their exact middle.
 *   This greatly facilitates the definition of successive conic bezier arcs.
 *   Moreover, it's the way outlines are described in the TrueType specification.
 *
 * Note that it is possible to mix conic and cubic arcs in a single contour, even though no current
 * font driver produces such outlines.
 *
 * <pre>
 *                                   *            # on
 *                                                * off
 *                                __---__
 *   #-__                      _--       -_
 *       --__                _-            -
 *           --__           #               \
 *               --__                        #
 *                   -#
 *                            Two "on" points
 *    Two "on" points       and one "conic" point
 *                             between them
 *                 *
 *   #            __      Two "on" points with two "conic"
 *    \          -  -     points between them. The point
 *     \        /    \    marked '0' is the middle of the
 *      -      0      \   "off" points, and is a 'virtual'
 *       -_  _-       #   "on" point where the curve passes.
 *         --             It does not appear in the point
 *                        list.
 *         *
 *         *                # on
 *                    *     * off
 *          __---__
 *       _--       -_
 *     _-            -
 *    #               \
 *                     #
 *
 *      Two "on" points
 *    and two "cubic" point
 *       between them
 * </pre>
 *
 * Each glyphs original outline points are located on a grid of indivisible units.
 * The points are stored in the font file as 16-bit integer grid coordinates,
 * with the grid origin's being at (0, 0); they thus range from -16384 to 16383.
 *
 * Convert conic to bezier arcs:
 * Conic P0 P1 P2
 * Bezier B0 B1 B2 B3
 * B0=P0
 * B1=(P0+2*P1)/3
 * B2=(P2+2*P1)/3
 * B3=P2
 */

static void blf_glyph_to_curves(const FT_Outline &ftoutline,
                                ListBaseT<Nurb> *nurbsbase,
                                const float scale)
{
  const float eps = 0.0001f;
  const float eps_sq = eps * eps;
  Nurb *nu;
  BezTriple *bezt;
  float dx, dy;
  int j, k, l, l_first = 0;

  /* initialize as -1 to add 1 on first loop each time */
  int contour_prev;

  /* Start converting the FT data */
  int *onpoints = MEM_new_array_zeroed<int>(size_t(ftoutline.n_contours), "onpoints");

  /* Get number of on-curve points for bezier-triples (including conic virtual on-points). */
  for (j = 0, contour_prev = -1; j < ftoutline.n_contours; j++) {
    const int n = ftoutline.contours[j] - contour_prev;
    contour_prev = ftoutline.contours[j];

    for (k = 0; k < n; k++) {
      l = (j > 0) ? (k + ftoutline.contours[j - 1] + 1) : k;
      if (k == 0) {
        l_first = l;
      }

      if (ftoutline.tags[l] == FT_Curve_Tag_On) {
        onpoints[j]++;
      }

      {
        const int l_next = (k < n - 1) ? (l + 1) : l_first;
        if (ftoutline.tags[l] == FT_Curve_Tag_Conic &&
            ftoutline.tags[l_next] == FT_Curve_Tag_Conic)
        {
          onpoints[j]++;
        }
      }
    }
  }

  /* contour loop, bezier & conic styles merged */
  for (j = 0, contour_prev = -1; j < ftoutline.n_contours; j++) {
    const int n = ftoutline.contours[j] - contour_prev;
    contour_prev = ftoutline.contours[j];

    /* add new curve */
    nu = MEM_new<Nurb>("objfnt_nurb");
    bezt = MEM_new_array_zeroed<BezTriple>(size_t(onpoints[j]), "objfnt_bezt");
    BLI_addtail(nurbsbase, nu);

    nu->type = CU_BEZIER;
    nu->pntsu = onpoints[j];
    nu->resolu = 8;
    nu->flagu = CU_NURB_CYCLIC;
    nu->bezt = bezt;

    /* individual curve loop, start-end */
    for (k = 0; k < n; k++) {
      l = (j > 0) ? (k + ftoutline.contours[j - 1] + 1) : k;
      if (k == 0) {
        l_first = l;
      }

      /* virtual conic on-curve points */
      {
        const int l_next = (k < n - 1) ? (l + 1) : l_first;
        if (ftoutline.tags[l] == FT_Curve_Tag_Conic &&
            ftoutline.tags[l_next] == FT_Curve_Tag_Conic)
        {
          dx = float(ftoutline.points[l].x + ftoutline.points[l_next].x) * scale / 2.0f;
          dy = float(ftoutline.points[l].y + ftoutline.points[l_next].y) * scale / 2.0f;

          /* left handle */
          bezt->vec[0][0] = (dx + (2.0f * float(ftoutline.points[l].x)) * scale) / 3.0f;
          bezt->vec[0][1] = (dy + (2.0f * float(ftoutline.points[l].y)) * scale) / 3.0f;

          /* midpoint (virtual on-curve point) */
          bezt->vec[1][0] = dx;
          bezt->vec[1][1] = dy;

          /* right handle */
          bezt->vec[2][0] = (dx + (2.0f * float(ftoutline.points[l_next].x)) * scale) / 3.0f;
          bezt->vec[2][1] = (dy + (2.0f * float(ftoutline.points[l_next].y)) * scale) / 3.0f;

          bezt->h1 = bezt->h2 = HD_ALIGN;
          bezt->radius = 1.0f;
          bezt++;
        }
      }

      /* on-curve points */
      if (ftoutline.tags[l] == FT_Curve_Tag_On) {
        const int l_prev = (k > 0) ? (l - 1) : ftoutline.contours[j];
        const int l_next = (k < n - 1) ? (l + 1) : l_first;

        /* left handle */
        if (ftoutline.tags[l_prev] == FT_Curve_Tag_Cubic) {
          bezt->vec[0][0] = float(ftoutline.points[l_prev].x) * scale;
          bezt->vec[0][1] = float(ftoutline.points[l_prev].y) * scale;
          bezt->h1 = HD_FREE;
        }
        else if (ftoutline.tags[l_prev] == FT_Curve_Tag_Conic) {
          bezt->vec[0][0] = (float(ftoutline.points[l].x) +
                             (2.0f * float(ftoutline.points[l_prev].x))) *
                            scale / 3.0f;
          bezt->vec[0][1] = (float(ftoutline.points[l].y) +
                             (2.0f * float(ftoutline.points[l_prev].y))) *
                            scale / 3.0f;
          bezt->h1 = HD_FREE;
        }
        else {
          bezt->vec[0][0] = float(ftoutline.points[l].x) * scale -
                            (float(ftoutline.points[l].x) - float(ftoutline.points[l_prev].x)) *
                                scale / 3.0f;
          bezt->vec[0][1] = float(ftoutline.points[l].y) * scale -
                            (float(ftoutline.points[l].y) - float(ftoutline.points[l_prev].y)) *
                                scale / 3.0f;
          bezt->h1 = HD_VECT;
        }

        /* midpoint (on-curve point) */
        bezt->vec[1][0] = float(ftoutline.points[l].x) * scale;
        bezt->vec[1][1] = float(ftoutline.points[l].y) * scale;

        /* right handle */
        if (ftoutline.tags[l_next] == FT_Curve_Tag_Cubic) {
          bezt->vec[2][0] = float(ftoutline.points[l_next].x) * scale;
          bezt->vec[2][1] = float(ftoutline.points[l_next].y) * scale;
          bezt->h2 = HD_FREE;
        }
        else if (ftoutline.tags[l_next] == FT_Curve_Tag_Conic) {
          bezt->vec[2][0] = (float(ftoutline.points[l].x) +
                             (2.0f * float(ftoutline.points[l_next].x))) *
                            scale / 3.0f;
          bezt->vec[2][1] = (float(ftoutline.points[l].y) +
                             (2.0f * float(ftoutline.points[l_next].y))) *
                            scale / 3.0f;
          bezt->h2 = HD_FREE;
        }
        else {
          bezt->vec[2][0] = float(ftoutline.points[l].x) * scale -
                            (float(ftoutline.points[l].x) - float(ftoutline.points[l_next].x)) *
                                scale / 3.0f;
          bezt->vec[2][1] = float(ftoutline.points[l].y) * scale -
                            (float(ftoutline.points[l].y) - float(ftoutline.points[l_next].y)) *
                                scale / 3.0f;
          bezt->h2 = HD_VECT;
        }

        /* get the handles that are aligned, tricky...
         * - check if one of them is a vector handle.
         * - dist_squared_to_line_v2, check if the three beztriple points are on one line
         * - len_squared_v2v2, see if there's a distance between the three points
         * - len_squared_v2v2 again, to check the angle between the handles
         */
        if ((bezt->h1 != HD_VECT && bezt->h2 != HD_VECT) &&
            (dist_squared_to_line_v2(bezt->vec[0], bezt->vec[1], bezt->vec[2]) <
             (0.001f * 0.001f)) &&
            (len_squared_v2v2(bezt->vec[0], bezt->vec[1]) > eps_sq) &&
            (len_squared_v2v2(bezt->vec[1], bezt->vec[2]) > eps_sq) &&
            (len_squared_v2v2(bezt->vec[0], bezt->vec[2]) > eps_sq) &&
            (len_squared_v2v2(bezt->vec[0], bezt->vec[2]) >
             max_ff(len_squared_v2v2(bezt->vec[0], bezt->vec[1]),
                    len_squared_v2v2(bezt->vec[1], bezt->vec[2]))))
        {
          bezt->h1 = bezt->h2 = HD_ALIGN;
        }
        bezt->radius = 1.0f;
        bezt++;
      }
    }
  }

  MEM_delete(onpoints);
}

static FT_GlyphSlot blf_glyphslot_ensure_outline(FontBLF *font, uint charcode, bool use_fallback)
{
  if (charcode < 32) {
    if (ELEM(charcode, 0x10, 0x13)) {
      /* Do not render line feed or carriage return. #134972. */
      return nullptr;
    }
    /* Other C0 controls (U+0000 - U+001F) can show as space. #135421. */
    /* TODO: Return all but TAB as ".notdef" character when we have our own. */
    charcode = ' ';
  }

  /* Glyph might not come from the initial font. */
  FontBLF *font_with_glyph = font;
  FT_UInt glyph_index = use_fallback ? blf_glyph_index_from_charcode(&font_with_glyph, charcode) :
                                       blf_get_char_index(font_with_glyph, charcode);

  if (!glyph_index) {
    return nullptr;
  }

  if (!blf_ensure_face(font_with_glyph)) {
    return nullptr;
  }

  FT_GlyphSlot glyph = blf_glyph_render_outline(font, font_with_glyph, glyph_index, charcode, 0);

  if (font != font_with_glyph) {
    if (!blf_ensure_face(font)) {
      return nullptr;
    }
    double ratio = float(font->face->units_per_EM) / float(font_with_glyph->face->units_per_EM);
    FT_Matrix transform = {to_16dot16(ratio), 0, 0, to_16dot16(ratio)};
    FT_Outline_Transform(&glyph->outline, &transform);
    glyph->advance.x = int(float(glyph->advance.x) * ratio);
    glyph->metrics.horiAdvance = int(float(glyph->metrics.horiAdvance) * ratio);
  }

  return glyph;
}

bool blf_character_to_curves(FontBLF *font,
                             uint unicode,
                             ListBaseT<Nurb> *nurbsbase,
                             const float scale,
                             bool use_fallback,
                             float *r_advance)
{
  FT_GlyphSlot glyph = blf_glyphslot_ensure_outline(font, unicode, use_fallback);
  if (!glyph) {
    *r_advance = 0.0f;
    return false;
  }

  blf_glyph_to_curves(glyph->outline, nurbsbase, scale);
  *r_advance = float(glyph->advance.x) * scale;
  return true;
}

/** \} */

}  // namespace blender
