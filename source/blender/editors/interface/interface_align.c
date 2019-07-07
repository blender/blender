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
 * The Original Code is Copyright (C) 2015 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 */

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "UI_interface.h"

#include "interface_intern.h"

#ifdef USE_UIBUT_SPATIAL_ALIGN

/**
 * This struct stores a (simplified) 2D representation of all buttons of a same align group,
 * with their immediate neighbors (if found),
 * and needed value to compute 'stitching' of aligned buttons.
 *
 * \note This simplistic struct cannot fully represent complex layouts where buttons share some
 *       'align space' with several others (see schema below), we'd need linked list and more
 *       complex code to handle that. However, looks like we can do without that for now,
 *       which is rather lucky!
 *
 *       <pre>
 *       +--------+-------+
 *       | BUT 1  | BUT 2 |      BUT 3 has two 'top' neighbors...
 *       |----------------|  =>  In practice, we only store one of BUT 1 or 2 (which ones is not
 *       |      BUT 3     |      really deterministic), and assume the other stores a ref to BUT 3.
 *       +----------------+
 *       </pre>
 *
 *       This will probably not work in all possible cases,
 *       but not sure we want to support such exotic cases anyway.
 */
typedef struct ButAlign {
  uiBut *but;

  /* Neighbor buttons */
  struct ButAlign *neighbors[4];

  /* Pointers to coordinates (rctf values) of the button. */
  float *borders[4];

  /* Distances to the neighbors. */
  float dists[4];

  /* Flags, used to mark whether we should 'stitch'
   * the corners of this button with its neighbors' ones. */
  char flags[4];
} ButAlign;

/* Side-related enums and flags. */
enum {
  /* Sides (used as indices, order is **crucial**,
   * this allows us to factorize code in a loop over the four sides). */
  LEFT = 0,
  TOP = 1,
  RIGHT = 2,
  DOWN = 3,
  TOTSIDES = 4,

  /* Stitch flags, built from sides values. */
  STITCH_LEFT = 1 << LEFT,
  STITCH_TOP = 1 << TOP,
  STITCH_RIGHT = 1 << RIGHT,
  STITCH_DOWN = 1 << DOWN,
};

/* Mapping between 'our' sides and 'public' UI_BUT_ALIGN flags, order must match enum above. */
#  define SIDE_TO_UI_BUT_ALIGN \
    { \
      UI_BUT_ALIGN_LEFT, UI_BUT_ALIGN_TOP, UI_BUT_ALIGN_RIGHT, UI_BUT_ALIGN_DOWN \
    }

/* Given one side, compute the three other ones */
#  define SIDE1(_s) (((_s) + 1) % TOTSIDES)
#  define OPPOSITE(_s) (((_s) + 2) % TOTSIDES)
#  define SIDE2(_s) (((_s) + 3) % TOTSIDES)

/* 0: LEFT/RIGHT sides; 1 = TOP/DOWN sides. */
#  define IS_COLUMN(_s) ((_s) % 2)

/* Stitch flag from side value. */
#  define STITCH(_s) (1 << (_s))

/* Max distance between to buttons for them to be 'mergeable'. */
#  define MAX_DELTA 0.45f * max_ii(UI_UNIT_Y, UI_UNIT_X)

bool ui_but_can_align(const uiBut *but)
{
  const bool btype_can_align = !ELEM(but->type,
                                     UI_BTYPE_LABEL,
                                     UI_BTYPE_CHECKBOX,
                                     UI_BTYPE_CHECKBOX_N,
                                     UI_BTYPE_TAB,
                                     UI_BTYPE_SEPR,
                                     UI_BTYPE_SEPR_LINE,
                                     UI_BTYPE_SEPR_SPACER);
  return (btype_can_align && (BLI_rctf_size_x(&but->rect) > 0.0f) &&
          (BLI_rctf_size_y(&but->rect) > 0.0f));
}

int ui_but_align_opposite_to_area_align_get(const ARegion *ar)
{
  switch (ar->alignment) {
    case RGN_ALIGN_TOP:
      return UI_BUT_ALIGN_DOWN;
    case RGN_ALIGN_BOTTOM:
      return UI_BUT_ALIGN_TOP;
    case RGN_ALIGN_LEFT:
      return UI_BUT_ALIGN_RIGHT;
    case RGN_ALIGN_RIGHT:
      return UI_BUT_ALIGN_LEFT;
  }

  return 0;
}

/**
 * This function checks a pair of buttons (assumed in a same align group),
 * and if they are neighbors, set needed data accordingly.
 *
 * \note It is designed to be called in total random order of buttons.
 * Order-based optimizations are done by caller.
 */
static void block_align_proximity_compute(ButAlign *butal, ButAlign *butal_other)
{
  /* That's the biggest gap between two borders to consider them 'alignable'. */
  const float max_delta = MAX_DELTA;
  float delta, delta_side_opp;
  int side, side_opp;

  const bool butal_can_align = ui_but_can_align(butal->but);
  const bool butal_other_can_align = ui_but_can_align(butal_other->but);

  const bool buts_share[2] = {
      /* Sharing same line? */
      !((*butal->borders[DOWN] >= *butal_other->borders[TOP]) ||
        (*butal->borders[TOP] <= *butal_other->borders[DOWN])),
      /* Sharing same column? */
      !((*butal->borders[LEFT] >= *butal_other->borders[RIGHT]) ||
        (*butal->borders[RIGHT] <= *butal_other->borders[LEFT])),
  };

  /* Early out in case buttons share no column or line, or if none can align... */
  if (!(buts_share[0] || buts_share[1]) || !(butal_can_align || butal_other_can_align)) {
    return;
  }

  for (side = 0; side < RIGHT; side++) {
    /* We are only interested in buttons which share a same line
     * (LEFT/RIGHT sides) or column (TOP/DOWN sides). */
    if (buts_share[IS_COLUMN(side)]) {
      side_opp = OPPOSITE(side);

      /* We check both opposite sides at once, because with very small buttons,
       * delta could be below max_delta for the wrong side
       * (that is, in horizontal case, the total width of two buttons can be below max_delta).
       * We rely on exact zero value here as an 'already processed' flag,
       * so ensure we never actually set a zero value at this stage.
       * FLT_MIN is zero-enough for UI position computing. ;) */
      delta = max_ff(fabsf(*butal->borders[side] - *butal_other->borders[side_opp]), FLT_MIN);
      delta_side_opp = max_ff(fabsf(*butal->borders[side_opp] - *butal_other->borders[side]),
                              FLT_MIN);
      if (delta_side_opp < delta) {
        SWAP(int, side, side_opp);
        delta = delta_side_opp;
      }

      if (delta < max_delta) {
        /* We are only interested in neighbors that are
         * at least as close as already found ones. */
        if (delta <= butal->dists[side]) {
          {
            /* We found an as close or closer neighbor.
             * If both buttons are alignable, we set them as each other neighbors.
             * Else, we have an unalignable one, we need to reset the others matching
             * neighbor to NULL if its 'proximity distance'
             * is really lower with current one.
             *
             * NOTE: We cannot only execute that piece of code in case we found a
             *       **closer** neighbor, due to the limited way we represent neighbors
             *       (buttons only know **one** neighbor on each side, when they can
             *       actually have several ones), it would prevent some buttons to be
             *       properly 'neighborly-initialized'. */
            if (butal_can_align && butal_other_can_align) {
              butal->neighbors[side] = butal_other;
              butal_other->neighbors[side_opp] = butal;
            }
            else if (butal_can_align && (delta < butal->dists[side])) {
              butal->neighbors[side] = NULL;
            }
            else if (butal_other_can_align && (delta < butal_other->dists[side_opp])) {
              butal_other->neighbors[side_opp] = NULL;
            }
            butal->dists[side] = butal_other->dists[side_opp] = delta;
          }

          if (butal_can_align && butal_other_can_align) {
            const int side_s1 = SIDE1(side);
            const int side_s2 = SIDE2(side);

            const int stitch = STITCH(side);
            const int stitch_opp = STITCH(side_opp);

            if (butal->neighbors[side] == NULL) {
              butal->neighbors[side] = butal_other;
            }
            if (butal_other->neighbors[side_opp] == NULL) {
              butal_other->neighbors[side_opp] = butal;
            }

            /* We have a pair of neighbors, we have to check whether we
             *   can stitch their matching corners.
             *   E.g. if butal_other is on the left of butal (that is, side == LEFT),
             *        if both TOP (side_s1) coordinates of buttons are close enough,
             *        we can stitch their upper matching corners,
             *        and same for DOWN (side_s2) side. */
            delta = fabsf(*butal->borders[side_s1] - *butal_other->borders[side_s1]);
            if (delta < max_delta) {
              butal->flags[side_s1] |= stitch;
              butal_other->flags[side_s1] |= stitch_opp;
            }
            delta = fabsf(*butal->borders[side_s2] - *butal_other->borders[side_s2]);
            if (delta < max_delta) {
              butal->flags[side_s2] |= stitch;
              butal_other->flags[side_s2] |= stitch_opp;
            }
          }
        }
        /* We assume two buttons can only share one side at most - for until
         * we have spherical UI. */
        return;
      }
    }
  }
}

/**
 * This function takes care of case described in this schema:
 *
 * <pre>
 * +-----------+-----------+
 * |   BUT 1   |   BUT 2   |
 * |-----------------------+
 * |   BUT 3   |
 * +-----------+
 * </pre>
 *
 * Here, BUT 3 RIGHT side would not get 'dragged' to align with BUT 1 RIGHT side,
 * since BUT 3 has not RIGHT neighbor.
 * So, this function, when called with BUT 1, will 'walk' the whole column in \a side_s1 direction
 * (TOP or DOWN when called for RIGHT side), and force buttons like BUT 3 to align as needed,
 * if BUT 1 and BUT 3 were detected as needing top-right corner stitching in
 * #block_align_proximity_compute() step.
 *
 * \note To avoid doing this twice, some stitching flags are cleared to break the
 * 'stitching connection' between neighbors.
 */
static void block_align_stitch_neighbors(ButAlign *butal,
                                         const int side,
                                         const int side_opp,
                                         const int side_s1,
                                         const int side_s2,
                                         const int align,
                                         const int align_opp,
                                         const float co)
{
  ButAlign *butal_neighbor;

  const int stitch_s1 = STITCH(side_s1);
  const int stitch_s2 = STITCH(side_s2);

  /* We have to check stitching flags on both sides of the stitching,
   * since we only clear one of them flags to break any future loop on same 'columns/side' case.
   * Also, if butal is spanning over several rows or columns of neighbors,
   * it may have both of its stitching flags
   * set, but would not be the case of its immediate neighbor! */
  while ((butal->flags[side] & stitch_s1) && (butal = butal->neighbors[side_s1]) &&
         (butal->flags[side] & stitch_s2)) {
    butal_neighbor = butal->neighbors[side];

    /* If we actually do have a neighbor, we directly set its values accordingly,
     * and clear its matching 'dist' to prevent it being set again later... */
    if (butal_neighbor) {
      butal->but->drawflag |= align;
      butal_neighbor->but->drawflag |= align_opp;
      *butal_neighbor->borders[side_opp] = co;
      butal_neighbor->dists[side_opp] = 0.0f;
    }
    /* See definition of UI_BUT_ALIGN_STITCH_LEFT/TOP for reason of this... */
    else if (side == LEFT) {
      butal->but->drawflag |= UI_BUT_ALIGN_STITCH_LEFT;
    }
    else if (side == TOP) {
      butal->but->drawflag |= UI_BUT_ALIGN_STITCH_TOP;
    }
    *butal->borders[side] = co;
    butal->dists[side] = 0.0f;
    /* Clearing one of the 'flags pair' here is enough to prevent this loop running on
     * the same column, side and direction again. */
    butal->flags[side] &= ~stitch_s2;
  }
}

/**
 * Helper to sort ButAlign items by:
 *   - Their align group.
 *   - Their vertical position.
 *   - Their horizontal position.
 */
static int ui_block_align_butal_cmp(const void *a, const void *b)
{
  const ButAlign *butal = a;
  const ButAlign *butal_other = b;

  /* Sort by align group. */
  if (butal->but->alignnr != butal_other->but->alignnr) {
    return butal->but->alignnr - butal_other->but->alignnr;
  }

  /* Sort vertically.
   * Note that Y of buttons is decreasing (first buttons have higher Y value than later ones). */
  if (*butal->borders[TOP] != *butal_other->borders[TOP]) {
    return (*butal_other->borders[TOP] > *butal->borders[TOP]) ? 1 : -1;
  }

  /* Sort horizontally. */
  if (*butal->borders[LEFT] != *butal_other->borders[LEFT]) {
    return (*butal->borders[LEFT] > *butal_other->borders[LEFT]) ? 1 : -1;
  }

  /* XXX We cannot actually assert here, since in some very compressed space cases,
   *     stupid UI code produces widgets which have the same TOP and LEFT positions...
   *     We do not care really,
   *     because this happens when UI is way too small to be usable anyway. */
  /* BLI_assert(0); */
  return 0;
}

static void ui_block_align_but_to_region(uiBut *but, const ARegion *region)
{
  rctf *rect = &but->rect;
  const float but_width = BLI_rctf_size_x(rect);
  const float but_height = BLI_rctf_size_y(rect);
  const float outline_px = U.pixelsize; /* This may have to be made more variable. */

  switch (but->drawflag & UI_BUT_ALIGN) {
    case UI_BUT_ALIGN_TOP:
      rect->ymax = region->winy + outline_px;
      rect->ymin = but->rect.ymax - but_height;
      break;
    case UI_BUT_ALIGN_DOWN:
      rect->ymin = -outline_px;
      rect->ymax = rect->ymin + but_height;
      break;
    case UI_BUT_ALIGN_LEFT:
      rect->xmin = -outline_px;
      rect->xmax = rect->xmin + but_width;
      break;
    case UI_BUT_ALIGN_RIGHT:
      rect->xmax = region->winx + outline_px;
      rect->xmin = rect->xmax - but_width;
      break;
    default:
      BLI_assert(0);
      break;
  }
}

/**
 * Compute the alignment of all 'align groups' of buttons in given block.
 *
 * This is using an order-independent algorithm,
 * i.e. alignment of buttons should be OK regardless of order in which
 * they are added to the block.
 */
void ui_block_align_calc(uiBlock *block, const ARegion *region)
{
  uiBut *but;
  int num_buttons = 0;

  const int sides_to_ui_but_align_flags[4] = SIDE_TO_UI_BUT_ALIGN;

  ButAlign *butal_array;
  ButAlign *butal, *butal_other;
  int side;
  int i, j;

  /* First loop: we count number of buttons belonging to an align group,
   * and clear their align flag.
   * Tabs get some special treatment here, they get aligned to region border. */
  for (but = block->buttons.first; but; but = but->next) {
    /* special case: tabs need to be aligned to a region border, drawflag tells which one */
    if (but->type == UI_BTYPE_TAB) {
      ui_block_align_but_to_region(but, region);
    }
    else {
      /* Clear old align flags. */
      but->drawflag &= ~UI_BUT_ALIGN_ALL;
    }

    if (but->alignnr != 0) {
      num_buttons++;
    }
  }

  if (num_buttons < 2) {
    /* No need to go further if we have nothing to align... */
    return;
  }

  butal_array = alloca(sizeof(*butal_array) * (size_t)num_buttons);
  memset(butal_array, 0, sizeof(*butal_array) * (size_t)num_buttons);

  /* Second loop: we initialize our ButAlign data for each button. */
  for (but = block->buttons.first, butal = butal_array; but; but = but->next) {
    if (but->alignnr != 0) {
      butal->but = but;
      butal->borders[LEFT] = &but->rect.xmin;
      butal->borders[RIGHT] = &but->rect.xmax;
      butal->borders[DOWN] = &but->rect.ymin;
      butal->borders[TOP] = &but->rect.ymax;
      copy_v4_fl(butal->dists, FLT_MAX);
      butal++;
    }
  }

  /* This will give us ButAlign items regrouped by align group, vertical and horizontal location.
   * Note that, given how buttons are defined in UI code,
   * butal_array shall already be "nearly sorted"... */
  qsort(butal_array, (size_t)num_buttons, sizeof(*butal_array), ui_block_align_butal_cmp);

  /* Third loop: for each pair of buttons in the same align group,
   * we compute their potential proximity. Note that each pair is checked only once, and that we
   * break early in case we know all remaining pairs will always be too far away. */
  for (i = 0, butal = butal_array; i < num_buttons; i++, butal++) {
    const short alignnr = butal->but->alignnr;

    for (j = i + 1, butal_other = &butal_array[i + 1]; j < num_buttons; j++, butal_other++) {
      const float max_delta = MAX_DELTA;

      /* Since they are sorted, buttons after current butal can only be of same or higher
       * group, and once they are not of same group, we know we can break this sub-loop and
       * start checking with next butal. */
      if (butal_other->but->alignnr != alignnr) {
        break;
      }

      /* Since they are sorted vertically first, buttons after current butal can only be at
       * same or lower height, and once they are lower than a given threshold, we know we can
       * break this sub-loop and start checking with next butal. */
      if ((*butal->borders[DOWN] - *butal_other->borders[TOP]) > max_delta) {
        break;
      }

      block_align_proximity_compute(butal, butal_other);
    }
  }

  /* Fourth loop: we have all our 'aligned' buttons as a 'map' in butal_array. We need to:
   *     - update their relevant coordinates to stitch them.
   *     - assign them valid flags.
   */
  for (i = 0; i < num_buttons; i++) {
    butal = &butal_array[i];

    for (side = 0; side < TOTSIDES; side++) {
      butal_other = butal->neighbors[side];

      if (butal_other) {
        const int side_opp = OPPOSITE(side);
        const int side_s1 = SIDE1(side);
        const int side_s2 = SIDE2(side);

        const int align = sides_to_ui_but_align_flags[side];
        const int align_opp = sides_to_ui_but_align_flags[side_opp];

        float co;

        butal->but->drawflag |= align;
        butal_other->but->drawflag |= align_opp;
        if (butal->dists[side]) {
          float *delta = &butal->dists[side];

          if (*butal->borders[side] < *butal_other->borders[side_opp]) {
            *delta *= 0.5f;
          }
          else {
            *delta *= -0.5f;
          }
          co = (*butal->borders[side] += *delta);

          if (butal_other->dists[side_opp]) {
            BLI_assert(butal_other->dists[side_opp] * 0.5f == fabsf(*delta));
            *butal_other->borders[side_opp] = co;
            butal_other->dists[side_opp] = 0.0f;
          }
          *delta = 0.0f;
        }
        else {
          co = *butal->borders[side];
        }

        block_align_stitch_neighbors(
            butal, side, side_opp, side_s1, side_s2, align, align_opp, co);
        block_align_stitch_neighbors(
            butal, side, side_opp, side_s2, side_s1, align, align_opp, co);
      }
    }
  }
}

#  undef SIDE_TO_UI_BUT_ALIGN
#  undef SIDE1
#  undef OPPOSITE
#  undef SIDE2
#  undef IS_COLUMN
#  undef STITCH
#  undef MAX_DELTA

#else

bool ui_but_can_align(uiBut *but)
{
  return !ELEM(but->type,
               UI_BTYPE_LABEL,
               UI_BTYPE_CHECKBOX,
               UI_BTYPE_CHECKBOX_N,
               UI_BTYPE_SEPR,
               UI_BTYPE_SEPR_LINE,
               UI_BTYPE_SEPR_SPACER);
}

static bool buts_are_horiz(uiBut *but1, uiBut *but2)
{
  float dx, dy;

  /* simple case which can fail if buttons shift apart
   * with proportional layouts, see: [#38602] */
  if ((but1->rect.ymin == but2->rect.ymin) && (but1->rect.xmin != but2->rect.xmin)) {
    return true;
  }

  dx = fabsf(but1->rect.xmax - but2->rect.xmin);
  dy = fabsf(but1->rect.ymin - but2->rect.ymax);

  return (dx <= dy);
}

static void ui_block_align_calc_but(uiBut *first, short nr)
{
  uiBut *prev, *but = NULL, *next;
  int flag = 0, cols = 0, rows = 0;

  /* auto align */

  for (but = first; but && but->alignnr == nr; but = but->next) {
    if (but->next && but->next->alignnr == nr) {
      if (buts_are_horiz(but, but->next)) {
        cols++;
      }
      else {
        rows++;
      }
    }
  }

  /* rows == 0: 1 row, cols == 0: 1 column */

  /* note;  how it uses 'flag' in loop below (either set it, or OR it) is confusing */
  for (but = first, prev = NULL; but && but->alignnr == nr; prev = but, but = but->next) {
    next = but->next;
    if (next && next->alignnr != nr) {
      next = NULL;
    }

    /* clear old flag */
    but->drawflag &= ~UI_BUT_ALIGN;

    if (flag == 0) { /* first case */
      if (next) {
        if (buts_are_horiz(but, next)) {
          if (rows == 0) {
            flag = UI_BUT_ALIGN_RIGHT;
          }
          else {
            flag = UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_RIGHT;
          }
        }
        else {
          flag = UI_BUT_ALIGN_DOWN;
        }
      }
    }
    else if (next == NULL) { /* last case */
      if (prev) {
        if (buts_are_horiz(prev, but)) {
          if (rows == 0) {
            flag = UI_BUT_ALIGN_LEFT;
          }
          else {
            flag = UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_LEFT;
          }
        }
        else {
          flag = UI_BUT_ALIGN_TOP;
        }
      }
    }
    else if (buts_are_horiz(but, next)) {
      /* check if this is already second row */
      if (prev && buts_are_horiz(prev, but) == 0) {
        flag &= ~UI_BUT_ALIGN_LEFT;
        flag |= UI_BUT_ALIGN_TOP;
        /* exception case: bottom row */
        if (rows > 0) {
          uiBut *bt = but;
          while (bt && bt->alignnr == nr) {
            if (bt->next && bt->next->alignnr == nr && buts_are_horiz(bt, bt->next) == 0) {
              break;
            }
            bt = bt->next;
          }
          if (bt == NULL || bt->alignnr != nr) {
            flag = UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_RIGHT;
          }
        }
      }
      else {
        flag |= UI_BUT_ALIGN_LEFT;
      }
    }
    else {
      if (cols == 0) {
        flag |= UI_BUT_ALIGN_TOP;
      }
      else { /* next button switches to new row */

        if (prev && buts_are_horiz(prev, but)) {
          flag |= UI_BUT_ALIGN_LEFT;
        }
        else {
          flag &= ~UI_BUT_ALIGN_LEFT;
          flag |= UI_BUT_ALIGN_TOP;
        }

        if ((flag & UI_BUT_ALIGN_TOP) == 0) { /* still top row */
          if (prev) {
            if (next && buts_are_horiz(but, next)) {
              flag = UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_LEFT | UI_BUT_ALIGN_RIGHT;
            }
            else {
              /* last button in top row */
              flag = UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_LEFT;
            }
          }
          else {
            flag |= UI_BUT_ALIGN_DOWN;
          }
        }
        else {
          flag |= UI_BUT_ALIGN_TOP;
        }
      }
    }

    but->drawflag |= flag;

    /* merge coordinates */
    if (prev) {
      /* simple cases */
      if (rows == 0) {
        but->rect.xmin = (prev->rect.xmax + but->rect.xmin) / 2.0f;
        prev->rect.xmax = but->rect.xmin;
      }
      else if (cols == 0) {
        but->rect.ymax = (prev->rect.ymin + but->rect.ymax) / 2.0f;
        prev->rect.ymin = but->rect.ymax;
      }
      else {
        if (buts_are_horiz(prev, but)) {
          but->rect.xmin = (prev->rect.xmax + but->rect.xmin) / 2.0f;
          prev->rect.xmax = but->rect.xmin;
          /* copy height too */
          but->rect.ymax = prev->rect.ymax;
        }
        else if (prev->prev && buts_are_horiz(prev->prev, prev) == 0) {
          /* the previous button is a single one in its row */
          but->rect.ymax = (prev->rect.ymin + but->rect.ymax) / 2.0f;
          prev->rect.ymin = but->rect.ymax;

          but->rect.xmin = prev->rect.xmin;
          if (next && buts_are_horiz(but, next) == 0) {
            but->rect.xmax = prev->rect.xmax;
          }
        }
        else {
          /* the previous button is not a single one in its row */
          but->rect.ymax = prev->rect.ymin;
        }
      }
    }
  }
}

void ui_block_align_calc(uiBlock *block)
{
  uiBut *but;
  short nr;

  /* align buttons with same align nr */
  for (but = block->buttons.first; but;) {
    if (but->alignnr) {
      nr = but->alignnr;
      ui_block_align_calc_but(but, nr);

      /* skip with same number */
      for (; but && but->alignnr == nr; but = but->next) {
        /* pass */
      }

      if (!but) {
        break;
      }
    }
    else {
      but = but->next;
    }
  }
}
#endif
