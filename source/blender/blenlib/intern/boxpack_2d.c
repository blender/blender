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

/** \file
 * \ingroup bli
 */

#include <math.h>   /* for fabsf */
#include <stdlib.h> /* for qsort */

#include "MEM_guardedalloc.h"

#include "BLI_boxpack_2d.h" /* own include */
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLI_sort.h" /* qsort_r */
#define qsort_r BLI_qsort_r

#include "BLI_strict_flags.h"

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wpadded"
#endif

/* de-duplicate as we pack */
#define USE_MERGE
/* use strip-free */
#define USE_FREE_STRIP
/* slight bias, needed when packing many boxes the _exact_ same size */
#define USE_PACK_BIAS

/* BoxPacker for backing 2D rectangles into a square
 *
 * The defined Below are for internal use only */
typedef struct BoxVert {
  float x;
  float y;

  int free : 8; /* vert status */
  uint used : 1;
  uint _pad : 23;
  uint index;

  struct BoxPack *trb; /* top right box */
  struct BoxPack *blb; /* bottom left box */
  struct BoxPack *brb; /* bottom right box */
  struct BoxPack *tlb; /* top left box */

  /* Store last intersecting boxes here
   * speedup intersection testing */
  struct BoxPack *isect_cache[4];

#ifdef USE_PACK_BIAS
  float bias;
  int _pad2;
#endif
} BoxVert;

#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wpadded"
#endif

/* free vert flags */
#define EPSILON 0.0000001f
#define EPSILON_MERGE 0.00001f
#ifdef USE_PACK_BIAS
#  define EPSILON_BIAS 0.000001f
#endif
#define BLF 1
#define TRF 2
#define TLF 4
#define BRF 8
#define CORNERFLAGS (BLF | TRF | TLF | BRF)

BLI_INLINE int quad_flag(uint q)
{
  BLI_assert(q < 4);
  return (1 << q);
}

#define BL 0
#define TR 1
#define TL 2
#define BR 3

/** \name Box Accessor Functions
 * \{ */

static float box_xmin_get(const BoxPack *box)
{
  return box->v[BL]->x;
}

static float box_xmax_get(const BoxPack *box)
{
  return box->v[TR]->x;
}

static float box_ymin_get(const BoxPack *box)
{
  return box->v[BL]->y;
}

static float box_ymax_get(const BoxPack *box)
{
  return box->v[TR]->y;
}
/** \} */

/** \name Box Placement
 * \{ */

BLI_INLINE void box_v34x_update(BoxPack *box)
{
  box->v[TL]->x = box->v[BL]->x;
  box->v[BR]->x = box->v[TR]->x;
}

BLI_INLINE void box_v34y_update(BoxPack *box)
{
  box->v[TL]->y = box->v[TR]->y;
  box->v[BR]->y = box->v[BL]->y;
}

static void box_xmin_set(BoxPack *box, const float f)
{
  box->v[TR]->x = f + box->w;
  box->v[BL]->x = f;
  box_v34x_update(box);
}

static void box_xmax_set(BoxPack *box, const float f)
{
  box->v[BL]->x = f - box->w;
  box->v[TR]->x = f;
  box_v34x_update(box);
}

static void box_ymin_set(BoxPack *box, const float f)
{
  box->v[TR]->y = f + box->h;
  box->v[BL]->y = f;
  box_v34y_update(box);
}

static void box_ymax_set(BoxPack *box, const float f)
{
  box->v[BL]->y = f - box->h;
  box->v[TR]->y = f;
  box_v34y_update(box);
}
/** \} */

/** \name Box Utils
 * \{ */

static float box_area(const BoxPack *box)
{
  return box->w * box->h;
}

static bool box_isect(const BoxPack *box_a, const BoxPack *box_b)
{
  return !(box_xmin_get(box_a) + EPSILON >= box_xmax_get(box_b) ||
           box_ymin_get(box_a) + EPSILON >= box_ymax_get(box_b) ||
           box_xmax_get(box_a) - EPSILON <= box_xmin_get(box_b) ||
           box_ymax_get(box_a) - EPSILON <= box_ymin_get(box_b));
}

/** \} */

/* compiler should inline */
static float max_ff(const float a, const float b)
{
  return b > a ? b : a;
}

#ifdef USE_PACK_BIAS
/* set when used is enabled */
static void vert_bias_update(BoxVert *v)
{
  BLI_assert(v->used);
  v->bias = (v->x * v->y) * EPSILON_BIAS;
}
#endif

#if 0
#  define BOXDEBUG(b) \
    printf("\tBox Debug i %i, w:%.3f h:%.3f x:%.3f y:%.3f\n", b->index, b->w, b->h, b->x, b->y)
#endif

/** \name Box/Vert Sorting
 * \{ */

/* qsort function - sort largest to smallest */
static int box_areasort(const void *p1, const void *p2)
{
  const BoxPack *b1 = p1, *b2 = p2;
  const float a1 = box_area(b1);
  const float a2 = box_area(b2);

  if (a1 < a2) {
    return 1;
  }
  if (a1 > a2) {
    return -1;
  }
  return 0;
}

/* qsort vertex sorting function
 * sorts from lower left to top right It uses the current box's width and height
 * as offsets when sorting, this has the result of not placing boxes outside
 * the bounds of the existing backed area where possible
 */
struct VertSortContext {
  BoxVert *vertarray;
  float box_width, box_height;
};

static int vertex_sort(const void *p1, const void *p2, void *vs_ctx_p)
{
  const struct VertSortContext *vs_ctx = vs_ctx_p;
  const BoxVert *v1, *v2;
  float a1, a2;

  v1 = &vs_ctx->vertarray[*((const uint *)p1)];
  v2 = &vs_ctx->vertarray[*((const uint *)p2)];

#ifdef USE_FREE_STRIP
  /* push free verts to the end so we can strip */
  if (UNLIKELY(v1->free == 0 && v2->free == 0)) {
    return 0;
  }
  if (UNLIKELY(v1->free == 0)) {
    return 1;
  }
  if (UNLIKELY(v2->free == 0)) {
    return -1;
  }
#endif

  a1 = max_ff(v1->x + vs_ctx->box_width, v1->y + vs_ctx->box_height);
  a2 = max_ff(v2->x + vs_ctx->box_width, v2->y + vs_ctx->box_height);

#ifdef USE_PACK_BIAS
  a1 += v1->bias;
  a2 += v2->bias;
#endif

  /* sort largest to smallest */
  if (a1 > a2) {
    return 1;
  }
  if (a1 < a2) {
    return -1;
  }
  return 0;
}
/** \} */

/**
 * Main box-packing function accessed from other functions
 * This sets boxes x,y to positive values, sorting from 0,0 outwards.
 * There is no limit to the space boxes may take, only that they will be packed
 * tightly into the lower left hand corner (0,0)
 *
 * \param boxarray: a pre-allocated array of boxes.
 *      only the 'box->x' and 'box->y' are set, 'box->w' and 'box->h' are used,
 *      'box->index' is not used at all, the only reason its there
 *          is that the box array is sorted by area and programs need to be able
 *          to have some way of writing the boxes back to the original data.
 * \param len: the number of boxes in the array.
 * \param r_tot_x, r_tot_y: set so you can normalize the data.
 *  */
void BLI_box_pack_2d(BoxPack *boxarray, const uint len, float *r_tot_x, float *r_tot_y)
{
  uint box_index, verts_pack_len, i, j, k;
  uint *vertex_pack_indices; /* an array of indices used for sorting verts */
  bool isect;
  float tot_x = 0.0f, tot_y = 0.0f;

  BoxPack *box, *box_test; /*current box and another for intersection tests*/
  BoxVert *vert;           /* the current vert */

  struct VertSortContext vs_ctx;

  if (!len) {
    *r_tot_x = tot_x;
    *r_tot_y = tot_y;
    return;
  }

  /* Sort boxes, biggest first */
  qsort(boxarray, (size_t)len, sizeof(BoxPack), box_areasort);

  /* add verts to the boxes, these are only used internally  */
  vert = MEM_mallocN(sizeof(BoxVert[4]) * (size_t)len, "BoxPack Verts");
  vertex_pack_indices = MEM_mallocN(sizeof(int[3]) * (size_t)len, "BoxPack Indices");

  vs_ctx.vertarray = vert;

  for (box = boxarray, box_index = 0, i = 0; box_index < len; box_index++, box++) {

    vert->blb = vert->brb = vert->tlb = vert->isect_cache[0] = vert->isect_cache[1] =
        vert->isect_cache[2] = vert->isect_cache[3] = NULL;
    vert->free = CORNERFLAGS & ~TRF;
    vert->trb = box;
    vert->used = false;
    vert->index = i++;
    box->v[BL] = vert++;

    vert->trb = vert->brb = vert->tlb = vert->isect_cache[0] = vert->isect_cache[1] =
        vert->isect_cache[2] = vert->isect_cache[3] = NULL;
    vert->free = CORNERFLAGS & ~BLF;
    vert->blb = box;
    vert->used = false;
    vert->index = i++;
    box->v[TR] = vert++;

    vert->trb = vert->blb = vert->tlb = vert->isect_cache[0] = vert->isect_cache[1] =
        vert->isect_cache[2] = vert->isect_cache[3] = NULL;
    vert->free = CORNERFLAGS & ~BRF;
    vert->brb = box;
    vert->used = false;
    vert->index = i++;
    box->v[TL] = vert++;

    vert->trb = vert->blb = vert->brb = vert->isect_cache[0] = vert->isect_cache[1] =
        vert->isect_cache[2] = vert->isect_cache[3] = NULL;
    vert->free = CORNERFLAGS & ~TLF;
    vert->tlb = box;
    vert->used = false;
    vert->index = i++;
    box->v[BR] = vert++;
  }
  vert = NULL;

  /* Pack the First box!
   * then enter the main box-packing loop */

  box = boxarray; /* get the first box  */
  /* First time, no boxes packed */
  box->v[BL]->free = 0; /* Can't use any if these */
  box->v[BR]->free &= ~(BLF | BRF);
  box->v[TL]->free &= ~(BLF | TLF);

  tot_x = box->w;
  tot_y = box->h;

  /* This sets all the vertex locations */
  box_xmin_set(box, 0.0f);
  box_ymin_set(box, 0.0f);
  box->x = box->y = 0.0f;

  for (i = 0; i < 4; i++) {
    box->v[i]->used = true;
#ifdef USE_PACK_BIAS
    vert_bias_update(box->v[i]);
#endif
  }

  for (i = 0; i < 3; i++) {
    vertex_pack_indices[i] = box->v[i + 1]->index;
  }
  verts_pack_len = 3;
  box++; /* next box, needed for the loop below */
  /* ...done packing the first box */

  /* Main boxpacking loop */
  for (box_index = 1; box_index < len; box_index++, box++) {

    /* These floats are used for sorting re-sorting */
    vs_ctx.box_width = box->w;
    vs_ctx.box_height = box->h;

    qsort_r(vertex_pack_indices, (size_t)verts_pack_len, sizeof(int), vertex_sort, &vs_ctx);

#ifdef USE_FREE_STRIP
    /* strip free vertices */
    i = verts_pack_len - 1;
    while ((i != 0) && vs_ctx.vertarray[vertex_pack_indices[i]].free == 0) {
      i--;
    }
    verts_pack_len = i + 1;
#endif

    /* Pack the box in with the others */
    /* sort the verts */
    isect = true;

    for (i = 0; i < verts_pack_len && isect; i++) {
      vert = &vs_ctx.vertarray[vertex_pack_indices[i]];
      /* printf("\ttesting vert %i %i %i %f %f\n", i,
       *        vert->free, verts_pack_len, vert->x, vert->y); */

      /* This vert has a free quadrant
       * Test if we can place the box here
       * vert->free & quad_flags[j] - Checks
       * */

      for (j = 0; (j < 4) && isect; j++) {
        if (vert->free & quad_flag(j)) {
          switch (j) {
            case BL:
              box_xmax_set(box, vert->x);
              box_ymax_set(box, vert->y);
              break;
            case TR:
              box_xmin_set(box, vert->x);
              box_ymin_set(box, vert->y);
              break;
            case TL:
              box_xmax_set(box, vert->x);
              box_ymin_set(box, vert->y);
              break;
            case BR:
              box_xmin_set(box, vert->x);
              box_ymax_set(box, vert->y);
              break;
          }

          /* Now we need to check that the box intersects
           * with any other boxes
           * Assume no intersection... */
          isect = false;

          if (/* Constrain boxes to positive X/Y values */
              box_xmin_get(box) < 0.0f || box_ymin_get(box) < 0.0f ||
              /* check for last intersected */
              (vert->isect_cache[j] && box_isect(box, vert->isect_cache[j]))) {
            /* Here we check that the last intersected
             * box will intersect with this one using
             * isect_cache that can store a pointer to a
             * box for each quadrant
             * big speedup */
            isect = true;
          }
          else {
            /* do a full search for colliding box
             * this is really slow, some spatially divided
             * data-structure would be better */
            for (box_test = boxarray; box_test != box; box_test++) {
              if (box_isect(box, box_test)) {
                /* Store the last intersecting here as cache
                 * for faster checking next time around */
                vert->isect_cache[j] = box_test;
                isect = true;
                break;
              }
            }
          }

          if (!isect) {

            /* maintain the total width and height */
            tot_x = max_ff(box_xmax_get(box), tot_x);
            tot_y = max_ff(box_ymax_get(box), tot_y);

            /* Place the box */
            vert->free &= (signed char)(~quad_flag(j));

            switch (j) {
              case TR:
                box->v[BL] = vert;
                vert->trb = box;
                break;
              case TL:
                box->v[BR] = vert;
                vert->tlb = box;
                break;
              case BR:
                box->v[TL] = vert;
                vert->brb = box;
                break;
              case BL:
                box->v[TR] = vert;
                vert->blb = box;
                break;
            }

            /* Mask free flags for verts that are
             * on the bottom or side so we don't get
             * boxes outside the given rectangle ares
             *
             * We can do an else/if here because only the first
             * box can be at the very bottom left corner */
            if (box_xmin_get(box) <= 0) {
              box->v[TL]->free &= ~(TLF | BLF);
              box->v[BL]->free &= ~(TLF | BLF);
            }
            else if (box_ymin_get(box) <= 0) {
              box->v[BL]->free &= ~(BRF | BLF);
              box->v[BR]->free &= ~(BRF | BLF);
            }

            /* The following block of code does a logical
             * check with 2 adjacent boxes, its possible to
             * flag verts on one or both of the boxes
             * as being used by checking the width or
             * height of both boxes */
            if (vert->tlb && vert->trb && (box == vert->tlb || box == vert->trb)) {
              if (UNLIKELY(fabsf(vert->tlb->h - vert->trb->h) < EPSILON_MERGE)) {
#ifdef USE_MERGE
#  define A (vert->trb->v[TL])
#  define B (vert->tlb->v[TR])
#  define MASK (BLF | BRF)
                BLI_assert(A->used != B->used);
                if (A->used) {
                  A->free &= B->free & ~MASK;
                  B = A;
                }
                else {
                  B->free &= A->free & ~MASK;
                  A = B;
                }
                BLI_assert((A->free & MASK) == 0);
#  undef A
#  undef B
#  undef MASK
#else
                vert->tlb->v[TR]->free &= ~BLF;
                vert->trb->v[TL]->free &= ~BRF;
#endif
              }
              else if (vert->tlb->h > vert->trb->h) {
                vert->trb->v[TL]->free &= ~(TLF | BLF);
              }
              else /* if (vert->tlb->h < vert->trb->h) */ {
                vert->tlb->v[TR]->free &= ~(TRF | BRF);
              }
            }
            else if (vert->blb && vert->brb && (box == vert->blb || box == vert->brb)) {
              if (UNLIKELY(fabsf(vert->blb->h - vert->brb->h) < EPSILON_MERGE)) {
#ifdef USE_MERGE
#  define A (vert->blb->v[BR])
#  define B (vert->brb->v[BL])
#  define MASK (TRF | TLF)
                BLI_assert(A->used != B->used);
                if (A->used) {
                  A->free &= B->free & ~MASK;
                  B = A;
                }
                else {
                  B->free &= A->free & ~MASK;
                  A = B;
                }
                BLI_assert((A->free & MASK) == 0);
#  undef A
#  undef B
#  undef MASK
#else
                vert->blb->v[BR]->free &= ~TRF;
                vert->brb->v[BL]->free &= ~TLF;
#endif
              }
              else if (vert->blb->h > vert->brb->h) {
                vert->brb->v[BL]->free &= ~(TLF | BLF);
              }
              else /* if (vert->blb->h < vert->brb->h) */ {
                vert->blb->v[BR]->free &= ~(TRF | BRF);
              }
            }
            /* Horizontal */
            if (vert->tlb && vert->blb && (box == vert->tlb || box == vert->blb)) {
              if (UNLIKELY(fabsf(vert->tlb->w - vert->blb->w) < EPSILON_MERGE)) {
#ifdef USE_MERGE
#  define A (vert->blb->v[TL])
#  define B (vert->tlb->v[BL])
#  define MASK (TRF | BRF)
                BLI_assert(A->used != B->used);
                if (A->used) {
                  A->free &= B->free & ~MASK;
                  B = A;
                }
                else {
                  B->free &= A->free & ~MASK;
                  A = B;
                }
                BLI_assert((A->free & MASK) == 0);
#  undef A
#  undef B
#  undef MASK
#else
                vert->blb->v[TL]->free &= ~TRF;
                vert->tlb->v[BL]->free &= ~BRF;
#endif
              }
              else if (vert->tlb->w > vert->blb->w) {
                vert->blb->v[TL]->free &= ~(TLF | TRF);
              }
              else /* if (vert->tlb->w < vert->blb->w) */ {
                vert->tlb->v[BL]->free &= ~(BLF | BRF);
              }
            }
            else if (vert->trb && vert->brb && (box == vert->trb || box == vert->brb)) {
              if (UNLIKELY(fabsf(vert->trb->w - vert->brb->w) < EPSILON_MERGE)) {

#ifdef USE_MERGE
#  define A (vert->brb->v[TR])
#  define B (vert->trb->v[BR])
#  define MASK (TLF | BLF)
                BLI_assert(A->used != B->used);
                if (A->used) {
                  A->free &= B->free & ~MASK;
                  B = A;
                }
                else {
                  B->free &= A->free & ~MASK;
                  A = B;
                }
                BLI_assert((A->free & MASK) == 0);
#  undef A
#  undef B
#  undef MASK
#else
                vert->brb->v[TR]->free &= ~TLF;
                vert->trb->v[BR]->free &= ~BLF;
#endif
              }
              else if (vert->trb->w > vert->brb->w) {
                vert->brb->v[TR]->free &= ~(TLF | TRF);
              }
              else /* if (vert->trb->w < vert->brb->w) */ {
                vert->trb->v[BR]->free &= ~(BLF | BRF);
              }
            }
            /* End logical check */

            for (k = 0; k < 4; k++) {
              if (box->v[k]->used == false) {
                box->v[k]->used = true;
#ifdef USE_PACK_BIAS
                vert_bias_update(box->v[k]);
#endif
                vertex_pack_indices[verts_pack_len] = box->v[k]->index;
                verts_pack_len++;
              }
            }
            /* The Box verts are only used internally
             * Update the box x and y since that's what external
             * functions will see */
            box->x = box_xmin_get(box);
            box->y = box_ymin_get(box);
          }
        }
      }
    }
  }

  *r_tot_x = tot_x;
  *r_tot_y = tot_y;

  /* free all the verts, not really needed because they shouldn't be
   * touched anymore but accessing the pointers would crash blender */
  for (box_index = 0; box_index < len; box_index++) {
    box = boxarray + box_index;
    box->v[0] = box->v[1] = box->v[2] = box->v[3] = NULL;
  }
  MEM_freeN(vertex_pack_indices);
  MEM_freeN(vs_ctx.vertarray);
}

/* Packs boxes into a fixed area.
 * boxes and packed are linked lists containing structs that can be cast to
 * FixedSizeBoxPack (i.e. contains a FixedSizeBoxPack as its first element).
 * Boxes that were packed successfully are placed into *packed and removed from *boxes.
 *
 * The algorithm is a simplified version of https://github.com/TeamHypersomnia/rectpack2D.
 * Better ones could be used, but for the current use case (packing Image tiles into GPU
 * textures) this is fine.
 *
 * Note that packing efficiency depends on the order of the input boxes. Generally speaking,
 * larger boxes should come first, though how exactly size is best defined (e.g. area,
 * perimeter) depends on the particular application. */
void BLI_box_pack_2d_fixedarea(ListBase *boxes, int width, int height, ListBase *packed)
{
  ListBase spaces = {NULL};
  FixedSizeBoxPack *full_rect = MEM_callocN(sizeof(FixedSizeBoxPack), __func__);
  full_rect->w = width;
  full_rect->h = height;

  BLI_addhead(&spaces, full_rect);

  /* The basic idea of the algorithm is to keep a list of free spaces in the packing area.
   * Then, for each box to be packed, we try to find a space that can contain it.
   * The found space is then split into the area that is occupied by the box and the
   * remaining area, which is reinserted into the free space list.
   * By inserting the smaller remaining spaces first, the algorithm tries to use these
   * smaller spaces first instead of "wasting" a large space. */
  LISTBASE_FOREACH_MUTABLE (FixedSizeBoxPack *, box, boxes) {
    LISTBASE_FOREACH (FixedSizeBoxPack *, space, &spaces) {
      /* Skip this space if it's too small. */
      if (box->w > space->w || box->h > space->h) {
        continue;
      }

      /* Pack this box into this space. */
      box->x = space->x;
      box->y = space->y;
      BLI_remlink(boxes, box);
      BLI_addtail(packed, box);

      if (box->w == space->w && box->h == space->h) {
        /* Box exactly fills space, so just remove the space. */
        BLI_remlink(&spaces, space);
        MEM_freeN(space);
      }
      else if (box->w == space->w) {
        /* Box fills the entire width, so we can just contract the box
         * to the upper part that remains. */
        space->y += box->h;
        space->h -= box->h;
      }
      else if (box->h == space->h) {
        /* Box fills the entire height, so we can just contract the box
         * to the right part that remains. */
        space->x += box->w;
        space->w -= box->w;
      }
      else {
        /* Split the remaining L-shaped space into two spaces.
         * There are two ways to do so, we pick the one that produces the biggest
         * remaining space:
         *
         *  Horizontal Split            Vertical Split
         * ###################        ###################
         * #                 #        #       -         #
         * #      Large      #        # Small -         #
         * #                 #        #       -         #
         * #********---------#        #********  Large  #
         * #  Box  *  Small  #        #  Box  *         #
         * #       *         #        #       *         #
         * ###################        ###################
         *
         */
        int area_hsplit_large = space->w * (space->h - box->h);
        int area_vsplit_large = (space->w - box->w) * space->h;

        /* Perform split. This space becomes the larger space,
         * while the new smaller space is inserted _before_ it. */
        FixedSizeBoxPack *new_space = MEM_callocN(sizeof(FixedSizeBoxPack), __func__);
        if (area_hsplit_large > area_vsplit_large) {
          new_space->x = space->x + box->w;
          new_space->y = space->y;
          new_space->w = space->w - box->w;
          new_space->h = box->h;

          space->y += box->h;
          space->h -= box->h;
        }
        else {
          new_space->x = space->x;
          new_space->y = space->y + box->h;
          new_space->w = box->w;
          new_space->h = space->h - box->h;

          space->x += box->w;
          space->w -= box->w;
        }
        BLI_addhead(&spaces, new_space);
      }

      break;
    }
  }

  BLI_freelistN(&spaces);
}
