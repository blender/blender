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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "MOD_lineart.h"

#include "lineart_intern.h"

#include <math.h>

#define LRT_OTHER_VERT(e, vt) ((vt) == (e)->v1 ? (e)->v2 : ((vt) == (e)->v2 ? (e)->v1 : NULL))

/* Get a connected line, only for lines who has the exact given vert, or (in the case of
 * intersection lines) who has a vert that has the exact same position. */
static LineartEdge *lineart_line_get_connected(LineartBoundingArea *ba,
                                               LineartVert *vt,
                                               LineartVert **new_vt,
                                               int match_flag)
{
  for (int i = 0; i < ba->line_count; i++) {
    LineartEdge *n_e = ba->linked_lines[i];

    if ((!(n_e->flags & LRT_EDGE_FLAG_ALL_TYPE)) || (n_e->flags & LRT_EDGE_FLAG_CHAIN_PICKED)) {
      continue;
    }

    if (match_flag && ((n_e->flags & LRT_EDGE_FLAG_ALL_TYPE) & match_flag) == 0) {
      continue;
    }

    *new_vt = LRT_OTHER_VERT(n_e, vt);
    if (*new_vt) {
      return n_e;
    }

    if (n_e->flags & LRT_EDGE_FLAG_INTERSECTION) {
      if (vt->fbcoord[0] == n_e->v1->fbcoord[0] && vt->fbcoord[1] == n_e->v1->fbcoord[1]) {
        *new_vt = LRT_OTHER_VERT(n_e, n_e->v1);
        return n_e;
      }
      if (vt->fbcoord[0] == n_e->v2->fbcoord[0] && vt->fbcoord[1] == n_e->v2->fbcoord[1]) {
        *new_vt = LRT_OTHER_VERT(n_e, n_e->v2);
        return n_e;
      }
    }
  }

  return NULL;
}

static LineartEdgeChain *lineart_chain_create(LineartRenderBuffer *rb)
{
  LineartEdgeChain *ec;
  ec = lineart_mem_acquire(&rb->render_data_pool, sizeof(LineartEdgeChain));

  BLI_addtail(&rb->chains, ec);

  return ec;
}

static bool lineart_point_overlapping(LineartEdgeChainItem *eci,
                                      float x,
                                      float y,
                                      double threshold)
{
  if (!eci) {
    return false;
  }
  if (((eci->pos[0] + threshold) >= x) && ((eci->pos[0] - threshold) <= x) &&
      ((eci->pos[1] + threshold) >= y) && ((eci->pos[1] - threshold) <= y)) {
    return true;
  }
  return false;
}

static LineartEdgeChainItem *lineart_chain_append_point(LineartRenderBuffer *rb,
                                                        LineartEdgeChain *ec,
                                                        float *fbcoord,
                                                        float *gpos,
                                                        float *normal,
                                                        char type,
                                                        int level,
                                                        unsigned char transparency_mask,
                                                        size_t index)
{
  LineartEdgeChainItem *eci;

  if (lineart_point_overlapping(ec->chain.last, fbcoord[0], fbcoord[1], 1e-5)) {
    /* Because the new chain point is overlapping, just replace the type and occlusion level of the
     * current point. This makes it so that the line to the point after this one has the correct
     * type and level. */
    LineartEdgeChainItem *old_rlci = ec->chain.last;
    old_rlci->line_type = type;
    old_rlci->occlusion = level;
    old_rlci->transparency_mask = transparency_mask;
    return old_rlci;
  }

  eci = lineart_mem_acquire(&rb->render_data_pool, sizeof(LineartEdgeChainItem));

  copy_v2_v2(eci->pos, fbcoord);
  copy_v3_v3(eci->gpos, gpos);
  eci->index = index;
  copy_v3_v3(eci->normal, normal);
  eci->line_type = type & LRT_EDGE_FLAG_ALL_TYPE;
  eci->occlusion = level;
  eci->transparency_mask = transparency_mask;
  BLI_addtail(&ec->chain, eci);

  return eci;
}

static LineartEdgeChainItem *lineart_chain_prepend_point(LineartRenderBuffer *rb,
                                                         LineartEdgeChain *ec,
                                                         float *fbcoord,
                                                         float *gpos,
                                                         float *normal,
                                                         char type,
                                                         int level,
                                                         unsigned char transparency_mask,
                                                         size_t index)
{
  LineartEdgeChainItem *eci;

  if (lineart_point_overlapping(ec->chain.first, fbcoord[0], fbcoord[1], 1e-5)) {
    return ec->chain.first;
  }

  eci = lineart_mem_acquire(&rb->render_data_pool, sizeof(LineartEdgeChainItem));

  copy_v2_v2(eci->pos, fbcoord);
  copy_v3_v3(eci->gpos, gpos);
  eci->index = index;
  copy_v3_v3(eci->normal, normal);
  eci->line_type = type & LRT_EDGE_FLAG_ALL_TYPE;
  eci->occlusion = level;
  eci->transparency_mask = transparency_mask;
  BLI_addhead(&ec->chain, eci);

  return eci;
}

void MOD_lineart_chain_feature_lines(LineartRenderBuffer *rb)
{
  LineartEdgeChain *ec;
  LineartEdgeChainItem *eci;
  LineartBoundingArea *ba;
  LineartEdgeSegment *es;
  int last_occlusion;
  unsigned char last_transparency;
  /* Used when converting from double. */
  float use_fbcoord[2];
  float use_gpos[3];

#define VERT_COORD_TO_FLOAT(a) \
  copy_v2fl_v2db(use_fbcoord, (a)->fbcoord); \
  copy_v3fl_v3db(use_gpos, (a)->gloc);

#define POS_TO_FLOAT(lpos, gpos) \
  copy_v2fl_v2db(use_fbcoord, lpos); \
  copy_v3fl_v3db(use_gpos, gpos);

  LRT_ITER_ALL_LINES_BEGIN
  {
    if ((!(e->flags & LRT_EDGE_FLAG_ALL_TYPE)) || (e->flags & LRT_EDGE_FLAG_CHAIN_PICKED)) {
      LRT_ITER_ALL_LINES_NEXT
      continue;
    }

    e->flags |= LRT_EDGE_FLAG_CHAIN_PICKED;

    ec = lineart_chain_create(rb);

    /* One chain can only have one object_ref,
     * so we assign it based on the first segment we found. */
    ec->object_ref = e->object_ref;

    LineartEdge *new_e = e;
    LineartVert *new_vt;
    float N[3] = {0};

    if (e->t1) {
      N[0] += e->t1->gn[0];
      N[1] += e->t1->gn[1];
      N[2] += e->t1->gn[2];
    }
    if (e->t2) {
      N[0] += e->t2->gn[0];
      N[1] += e->t2->gn[1];
      N[2] += e->t2->gn[2];
    }
    if (e->t1 || e->t2) {
      normalize_v3(N);
    }

    /*  Step 1: grow left. */
    ba = MOD_lineart_get_bounding_area(rb, e->v1->fbcoord[0], e->v1->fbcoord[1]);
    new_vt = e->v1;
    es = e->segments.first;
    VERT_COORD_TO_FLOAT(new_vt);
    lineart_chain_prepend_point(rb,
                                ec,
                                use_fbcoord,
                                use_gpos,
                                N,
                                e->flags,
                                es->occlusion,
                                es->transparency_mask,
                                e->v1_obindex);
    while (ba && (new_e = lineart_line_get_connected(ba, new_vt, &new_vt, e->flags))) {
      new_e->flags |= LRT_EDGE_FLAG_CHAIN_PICKED;

      if (new_e->t1 || new_e->t2) {
        zero_v3(N);
        if (new_e->t1) {
          N[0] += new_e->t1->gn[0];
          N[1] += new_e->t1->gn[1];
          N[2] += new_e->t1->gn[2];
        }
        if (new_e->t2) {
          N[0] += new_e->t2->gn[0];
          N[1] += new_e->t2->gn[1];
          N[2] += new_e->t2->gn[2];
        }
        normalize_v3(N);
      }

      if (new_vt == new_e->v1) {
        for (es = new_e->segments.last; es; es = es->prev) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * es->at / (es->at * lfb[3] + (1 - es->at) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, es->at);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_prepend_point(rb,
                                      ec,
                                      use_fbcoord,
                                      use_gpos,
                                      N,
                                      new_e->flags,
                                      es->occlusion,
                                      es->transparency_mask,
                                      new_e->v1_obindex);
          last_occlusion = es->occlusion;
          last_transparency = es->transparency_mask;
        }
      }
      else if (new_vt == new_e->v2) {
        es = new_e->segments.first;
        last_occlusion = es->occlusion;
        last_transparency = es->transparency_mask;
        es = es->next;
        for (; es; es = es->next) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * es->at / (es->at * lfb[3] + (1 - es->at) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, es->at);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_prepend_point(rb,
                                      ec,
                                      use_fbcoord,
                                      use_gpos,
                                      N,
                                      new_e->flags,
                                      last_occlusion,
                                      last_transparency,
                                      new_e->v2_obindex);
          last_occlusion = es->occlusion;
          last_transparency = es->transparency_mask;
        }
        VERT_COORD_TO_FLOAT(new_e->v2);
        lineart_chain_prepend_point(rb,
                                    ec,
                                    use_fbcoord,
                                    use_gpos,
                                    N,
                                    new_e->flags,
                                    last_occlusion,
                                    last_transparency,
                                    new_e->v2_obindex);
      }
      ba = MOD_lineart_get_bounding_area(rb, new_vt->fbcoord[0], new_vt->fbcoord[1]);
    }

    /* Restore normal value. */
    if (e->t1 || e->t2) {
      zero_v3(N);
      if (e->t1) {
        N[0] += e->t1->gn[0];
        N[1] += e->t1->gn[1];
        N[2] += e->t1->gn[2];
      }
      if (e->t2) {
        N[0] += e->t2->gn[0];
        N[1] += e->t2->gn[1];
        N[2] += e->t2->gn[2];
      }
      normalize_v3(N);
    }
    /*  Step 2: Adding all cuts from the given line, so we can continue connecting the right side
     * of the line. */
    es = e->segments.first;
    last_occlusion = ((LineartEdgeSegment *)es)->occlusion;
    last_transparency = ((LineartEdgeSegment *)es)->transparency_mask;
    for (es = es->next; es; es = es->next) {
      double gpos[3], lpos[3];
      double *lfb = e->v1->fbcoord, *rfb = e->v2->fbcoord;
      double global_at = lfb[3] * es->at / (es->at * lfb[3] + (1 - es->at) * rfb[3]);
      interp_v3_v3v3_db(lpos, e->v1->fbcoord, e->v2->fbcoord, es->at);
      interp_v3_v3v3_db(gpos, e->v1->gloc, e->v2->gloc, global_at);
      POS_TO_FLOAT(lpos, gpos)
      lineart_chain_append_point(rb,
                                 ec,
                                 use_fbcoord,
                                 use_gpos,
                                 N,
                                 e->flags,
                                 es->occlusion,
                                 es->transparency_mask,
                                 e->v1_obindex);
      last_occlusion = es->occlusion;
      last_transparency = es->transparency_mask;
    }
    VERT_COORD_TO_FLOAT(e->v2)
    lineart_chain_append_point(rb,
                               ec,
                               use_fbcoord,
                               use_gpos,
                               N,
                               e->flags,
                               last_occlusion,
                               last_transparency,
                               e->v2_obindex);

    /*  Step 3: grow right. */
    ba = MOD_lineart_get_bounding_area(rb, e->v2->fbcoord[0], e->v2->fbcoord[1]);
    new_vt = e->v2;
    while (ba && (new_e = lineart_line_get_connected(ba, new_vt, &new_vt, e->flags))) {
      new_e->flags |= LRT_EDGE_FLAG_CHAIN_PICKED;

      if (new_e->t1 || new_e->t2) {
        zero_v3(N);
        if (new_e->t1) {
          N[0] += new_e->t1->gn[0];
          N[1] += new_e->t1->gn[1];
          N[2] += new_e->t1->gn[2];
        }
        if (new_e->t2) {
          N[0] += new_e->t2->gn[0];
          N[1] += new_e->t2->gn[1];
          N[2] += new_e->t2->gn[2];
        }
        normalize_v3(N);
      }

      /* Fix leading vertex type. */
      eci = ec->chain.last;
      eci->line_type = new_e->flags & LRT_EDGE_FLAG_ALL_TYPE;

      if (new_vt == new_e->v1) {
        es = new_e->segments.last;
        last_occlusion = es->occlusion;
        last_transparency = es->transparency_mask;
        /* Fix leading vertex occlusion. */
        eci->occlusion = last_occlusion;
        eci->transparency_mask = last_transparency;
        for (es = new_e->segments.last; es; es = es->prev) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * es->at / (es->at * lfb[3] + (1 - es->at) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, es->at);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          last_occlusion = es->prev ? es->prev->occlusion : last_occlusion;
          last_transparency = es->prev ? es->prev->transparency_mask : last_transparency;
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_append_point(rb,
                                     ec,
                                     use_fbcoord,
                                     use_gpos,
                                     N,
                                     new_e->flags,
                                     last_occlusion,
                                     last_transparency,
                                     new_e->v1_obindex);
        }
      }
      else if (new_vt == new_e->v2) {
        es = new_e->segments.first;
        last_occlusion = es->occlusion;
        last_transparency = es->transparency_mask;
        eci->occlusion = last_occlusion;
        eci->transparency_mask = last_transparency;
        es = es->next;
        for (; es; es = es->next) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * es->at / (es->at * lfb[3] + (1 - es->at) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, es->at);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_append_point(rb,
                                     ec,
                                     use_fbcoord,
                                     use_gpos,
                                     N,
                                     new_e->flags,
                                     es->occlusion,
                                     es->transparency_mask,
                                     new_e->v2_obindex);
          last_occlusion = es->occlusion;
          last_transparency = es->transparency_mask;
        }
        VERT_COORD_TO_FLOAT(new_e->v2)
        lineart_chain_append_point(rb,
                                   ec,
                                   use_fbcoord,
                                   use_gpos,
                                   N,
                                   new_e->flags,
                                   last_occlusion,
                                   last_transparency,
                                   new_e->v2_obindex);
      }
      ba = MOD_lineart_get_bounding_area(rb, new_vt->fbcoord[0], new_vt->fbcoord[1]);
    }
    if (rb->fuzzy_everything) {
      ec->type = LRT_EDGE_FLAG_CONTOUR;
    }
    else {
      ec->type = (e->flags & LRT_EDGE_FLAG_ALL_TYPE);
    }
  }
  LRT_ITER_ALL_LINES_END
}

static LineartBoundingArea *lineart_bounding_area_get_eci_recursive(LineartRenderBuffer *rb,
                                                                    LineartBoundingArea *root,
                                                                    LineartEdgeChainItem *eci)
{
  if (root->child == NULL) {
    return root;
  }

  LineartBoundingArea *ch = root->child;
#define IN_BOUND(ba, eci) \
  ba.l <= eci->pos[0] && ba.r >= eci->pos[0] && ba.b <= eci->pos[1] && ba.u >= eci->pos[1]

  if (IN_BOUND(ch[0], eci)) {
    return lineart_bounding_area_get_eci_recursive(rb, &ch[0], eci);
  }
  if (IN_BOUND(ch[1], eci)) {
    return lineart_bounding_area_get_eci_recursive(rb, &ch[1], eci);
  }
  if (IN_BOUND(ch[2], eci)) {
    return lineart_bounding_area_get_eci_recursive(rb, &ch[2], eci);
  }
  if (IN_BOUND(ch[3], eci)) {
    return lineart_bounding_area_get_eci_recursive(rb, &ch[3], eci);
  }
#undef IN_BOUND
  return NULL;
}

static LineartBoundingArea *lineart_bounding_area_get_end_point(LineartRenderBuffer *rb,
                                                                LineartEdgeChainItem *eci)
{
  if (!eci) {
    return NULL;
  }
  LineartBoundingArea *root = MOD_lineart_get_parent_bounding_area(rb, eci->pos[0], eci->pos[1]);
  if (root == NULL) {
    return NULL;
  }
  return lineart_bounding_area_get_eci_recursive(rb, root, eci);
}

/**
 * Here we will try to connect geometry space chains together in image space. However we can't
 * chain two chains together if their end and start points lie on the border between two bounding
 * areas, this happens either when 1) the geometry is way too dense, or 2) the chaining threshold
 * is too big that it covers multiple small bounding areas.
 */
static void lineart_bounding_area_link_point_recursive(LineartRenderBuffer *rb,
                                                       LineartBoundingArea *root,
                                                       LineartEdgeChain *ec,
                                                       LineartEdgeChainItem *eci)
{
  if (root->child == NULL) {
    LineartChainRegisterEntry *cre = lineart_list_append_pointer_pool_sized(
        &root->linked_chains, &rb->render_data_pool, ec, sizeof(LineartChainRegisterEntry));

    cre->eci = eci;

    if (eci == ec->chain.first) {
      cre->is_left = 1;
    }
  }
  else {
    LineartBoundingArea *ch = root->child;

#define IN_BOUND(ba, eci) \
  ba.l <= eci->pos[0] && ba.r >= eci->pos[0] && ba.b <= eci->pos[1] && ba.u >= eci->pos[1]

    if (IN_BOUND(ch[0], eci)) {
      lineart_bounding_area_link_point_recursive(rb, &ch[0], ec, eci);
    }
    else if (IN_BOUND(ch[1], eci)) {
      lineart_bounding_area_link_point_recursive(rb, &ch[1], ec, eci);
    }
    else if (IN_BOUND(ch[2], eci)) {
      lineart_bounding_area_link_point_recursive(rb, &ch[2], ec, eci);
    }
    else if (IN_BOUND(ch[3], eci)) {
      lineart_bounding_area_link_point_recursive(rb, &ch[3], ec, eci);
    }

#undef IN_BOUND
  }
}

static void lineart_bounding_area_link_chain(LineartRenderBuffer *rb, LineartEdgeChain *ec)
{
  LineartEdgeChainItem *pl = ec->chain.first;
  LineartEdgeChainItem *pr = ec->chain.last;
  LineartBoundingArea *ba1 = MOD_lineart_get_parent_bounding_area(rb, pl->pos[0], pl->pos[1]);
  LineartBoundingArea *ba2 = MOD_lineart_get_parent_bounding_area(rb, pr->pos[0], pr->pos[1]);

  if (ba1) {
    lineart_bounding_area_link_point_recursive(rb, ba1, ec, pl);
  }
  if (ba2) {
    lineart_bounding_area_link_point_recursive(rb, ba2, ec, pr);
  }
}

void MOD_lineart_chain_split_for_fixed_occlusion(LineartRenderBuffer *rb)
{
  LineartEdgeChain *ec, *new_rlc;
  LineartEdgeChainItem *eci, *next_rlci;
  ListBase swap = {0};

  swap.first = rb->chains.first;
  swap.last = rb->chains.last;

  rb->chains.last = rb->chains.first = NULL;

  while ((ec = BLI_pophead(&swap)) != NULL) {
    ec->next = ec->prev = NULL;
    BLI_addtail(&rb->chains, ec);
    LineartEdgeChainItem *first_rlci = (LineartEdgeChainItem *)ec->chain.first;
    int fixed_occ = first_rlci->occlusion;
    unsigned char fixed_mask = first_rlci->transparency_mask;
    ec->level = fixed_occ;
    ec->transparency_mask = fixed_mask;
    for (eci = first_rlci->next; eci; eci = next_rlci) {
      next_rlci = eci->next;
      if (eci->occlusion != fixed_occ || eci->transparency_mask != fixed_mask) {
        if (next_rlci) {
          if (lineart_point_overlapping(next_rlci, eci->pos[0], eci->pos[1], 1e-5)) {
            continue;
          }
        }
        else {
          /* Set the same occlusion level for the end vertex, so when further connection is needed
           * the backwards occlusion info is also correct.  */
          eci->occlusion = fixed_occ;
          eci->transparency_mask = fixed_mask;
          /* No need to split at the last point anyway. */
          break;
        }
        new_rlc = lineart_chain_create(rb);
        new_rlc->chain.first = eci;
        new_rlc->chain.last = ec->chain.last;
        ec->chain.last = eci->prev;
        ((LineartEdgeChainItem *)ec->chain.last)->next = 0;
        eci->prev = 0;

        /* End the previous one. */
        lineart_chain_append_point(rb,
                                   ec,
                                   eci->pos,
                                   eci->gpos,
                                   eci->normal,
                                   eci->line_type,
                                   fixed_occ,
                                   fixed_mask,
                                   eci->index);
        new_rlc->object_ref = ec->object_ref;
        new_rlc->type = ec->type;
        ec = new_rlc;
        fixed_occ = eci->occlusion;
        fixed_mask = eci->transparency_mask;
        ec->level = fixed_occ;
        ec->transparency_mask = fixed_mask;
      }
    }
  }
  LISTBASE_FOREACH (LineartEdgeChain *, iec, &rb->chains) {
    lineart_bounding_area_link_chain(rb, iec);
  }
}

/**
 * Note: segment type (crease/material/contour...) is ambiguous after this.
 */
static void lineart_chain_connect(LineartRenderBuffer *UNUSED(rb),
                                  LineartEdgeChain *onto,
                                  LineartEdgeChain *sub,
                                  int reverse_1,
                                  int reverse_2)
{
  LineartEdgeChainItem *eci;
  if (onto->type == LRT_EDGE_FLAG_INTERSECTION) {
    if (sub->object_ref) {
      onto->object_ref = sub->object_ref;
      onto->type = LRT_EDGE_FLAG_CONTOUR;
    }
  }
  else if (sub->type == LRT_EDGE_FLAG_INTERSECTION) {
    if (onto->type != LRT_EDGE_FLAG_INTERSECTION) {
      onto->type = LRT_EDGE_FLAG_CONTOUR;
    }
  }
  if (!reverse_1) {  /*  L--R L-R. */
    if (reverse_2) { /*  L--R R-L. */
      BLI_listbase_reverse(&sub->chain);
    }
    eci = sub->chain.first;
    if (lineart_point_overlapping(onto->chain.last, eci->pos[0], eci->pos[1], 1e-5)) {
      BLI_pophead(&sub->chain);
      if (sub->chain.first == NULL) {
        return;
      }
    }
    ((LineartEdgeChainItem *)onto->chain.last)->next = sub->chain.first;
    ((LineartEdgeChainItem *)sub->chain.first)->prev = onto->chain.last;
    onto->chain.last = sub->chain.last;
  }
  else {              /*  L-R L--R. */
    if (!reverse_2) { /*  R-L L--R. */
      BLI_listbase_reverse(&sub->chain);
    }
    eci = onto->chain.first;
    if (lineart_point_overlapping(sub->chain.last, eci->pos[0], eci->pos[1], 1e-5)) {
      BLI_pophead(&onto->chain);
      if (onto->chain.first == NULL) {
        return;
      }
    }
    ((LineartEdgeChainItem *)sub->chain.last)->next = onto->chain.first;
    ((LineartEdgeChainItem *)onto->chain.first)->prev = sub->chain.last;
    onto->chain.first = sub->chain.first;
  }
}

static LineartChainRegisterEntry *lineart_chain_get_closest_cre(LineartRenderBuffer *rb,
                                                                LineartBoundingArea *ba,
                                                                LineartEdgeChain *ec,
                                                                LineartEdgeChainItem *eci,
                                                                int occlusion,
                                                                unsigned char transparency_mask,
                                                                float dist,
                                                                float *result_new_len,
                                                                LineartBoundingArea *caller_ba)
{

  LineartChainRegisterEntry *closest_cre = NULL;

  /* Keep using for loop because `cre` could be removed from the iteration before getting to the
   * next one. */
  LISTBASE_FOREACH_MUTABLE (LineartChainRegisterEntry *, cre, &ba->linked_chains) {
    if (cre->ec->object_ref != ec->object_ref) {
      if (!rb->fuzzy_everything) {
        if (rb->fuzzy_intersections) {
          /* If none of those are intersection lines... */
          if ((!(cre->ec->type & LRT_EDGE_FLAG_INTERSECTION)) &&
              (!(ec->type & LRT_EDGE_FLAG_INTERSECTION))) {
            continue; /* We don't want to chain along different objects at the moment. */
          }
        }
        else {
          continue;
        }
      }
    }
    if (cre->ec->picked || cre->picked) {
      continue;
    }
    if (cre->ec == ec || (!cre->ec->chain.first) || (cre->ec->level != occlusion) ||
        (cre->ec->transparency_mask != transparency_mask)) {
      continue;
    }
    if (!rb->fuzzy_everything) {
      if (cre->ec->type != ec->type) {
        if (rb->fuzzy_intersections) {
          if (!(cre->ec->type == LRT_EDGE_FLAG_INTERSECTION ||
                ec->type == LRT_EDGE_FLAG_INTERSECTION)) {
            continue; /* Fuzzy intersections but no intersection line found. */
          }
        }
        else { /* Line type different but no fuzzy. */
          continue;
        }
      }
    }

    float new_len = len_v2v2(cre->eci->pos, eci->pos);
    if (new_len < dist) {
      closest_cre = cre;
      dist = new_len;
      if (result_new_len) {
        (*result_new_len) = new_len;
      }
    }
  }

  /* We want a closer point anyway. So using modified dist is fine. */
  float adjacent_new_len = dist;
  LineartChainRegisterEntry *adjacent_closest;

#define LRT_TEST_ADJACENT_AREAS(dist_to, list) \
  if (dist_to < dist && dist_to > 0) { \
    LISTBASE_FOREACH (LinkData *, ld, list) { \
      LineartBoundingArea *sba = (LineartBoundingArea *)ld->data; \
      adjacent_closest = lineart_chain_get_closest_cre( \
          rb, sba, ec, eci, occlusion, transparency_mask, dist, &adjacent_new_len, ba); \
      if (adjacent_new_len < dist) { \
        dist = adjacent_new_len; \
        closest_cre = adjacent_closest; \
      } \
    } \
  }
  if (!caller_ba) {
    LRT_TEST_ADJACENT_AREAS(eci->pos[0] - ba->l, &ba->lp);
    LRT_TEST_ADJACENT_AREAS(ba->r - eci->pos[0], &ba->rp);
    LRT_TEST_ADJACENT_AREAS(ba->u - eci->pos[1], &ba->up);
    LRT_TEST_ADJACENT_AREAS(eci->pos[1] - ba->b, &ba->bp);
  }
  if (result_new_len) {
    (*result_new_len) = dist;
  }
  return closest_cre;
}

/**
 * This function only connects two different chains. It will not do any clean up or smart chaining.
 * So no: removing overlapping chains, removal of short isolated segments, and no loop reduction is
 * implemented yet.
 */
void MOD_lineart_chain_connect(LineartRenderBuffer *rb)
{
  LineartEdgeChain *ec;
  LineartEdgeChainItem *rlci_l, *rlci_r;
  LineartBoundingArea *ba_l, *ba_r;
  LineartChainRegisterEntry *closest_cre_l, *closest_cre_r, *closest_cre;
  float dist = rb->chaining_image_threshold;
  float dist_l, dist_r;
  int occlusion, reverse_main;
  unsigned char transparency_mask;
  ListBase swap = {0};

  if (rb->chaining_image_threshold < 0.0001) {
    return;
  }

  swap.first = rb->chains.first;
  swap.last = rb->chains.last;

  rb->chains.last = rb->chains.first = NULL;

  while ((ec = BLI_pophead(&swap)) != NULL) {
    ec->next = ec->prev = NULL;
    if (ec->picked) {
      continue;
    }
    BLI_addtail(&rb->chains, ec);

    occlusion = ec->level;
    transparency_mask = ec->transparency_mask;

    rlci_l = ec->chain.first;
    rlci_r = ec->chain.last;
    while ((ba_l = lineart_bounding_area_get_end_point(rb, rlci_l)) &&
           (ba_r = lineart_bounding_area_get_end_point(rb, rlci_r))) {
      closest_cre_l = lineart_chain_get_closest_cre(
          rb, ba_l, ec, rlci_l, occlusion, transparency_mask, dist, &dist_l, NULL);
      closest_cre_r = lineart_chain_get_closest_cre(
          rb, ba_r, ec, rlci_r, occlusion, transparency_mask, dist, &dist_r, NULL);
      if (closest_cre_l && closest_cre_r) {
        if (dist_l < dist_r) {
          closest_cre = closest_cre_l;
          reverse_main = 1;
        }
        else {
          closest_cre = closest_cre_r;
          reverse_main = 0;
        }
      }
      else if (closest_cre_l) {
        closest_cre = closest_cre_l;
        reverse_main = 1;
      }
      else if (closest_cre_r) {
        closest_cre = closest_cre_r;
        BLI_remlink(&ba_r->linked_chains, closest_cre_r);
        reverse_main = 0;
      }
      else {
        break;
      }
      closest_cre->picked = 1;
      closest_cre->ec->picked = 1;
      if (closest_cre->is_left) {
        lineart_chain_connect(rb, ec, closest_cre->ec, reverse_main, 0);
      }
      else {
        lineart_chain_connect(rb, ec, closest_cre->ec, reverse_main, 1);
      }
      BLI_remlink(&swap, closest_cre->ec);
      rlci_l = ec->chain.first;
      rlci_r = ec->chain.last;
    }
    ec->picked = 1;
  }
}

/**
 * Length is in image space.
 */
float MOD_lineart_chain_compute_length(LineartEdgeChain *ec)
{
  LineartEdgeChainItem *eci;
  float offset_accum = 0;
  float dist;
  float last_point[2];

  eci = ec->chain.first;
  copy_v2_v2(last_point, eci->pos);
  for (eci = ec->chain.first; eci; eci = eci->next) {
    dist = len_v2v2(eci->pos, last_point);
    offset_accum += dist;
    copy_v2_v2(last_point, eci->pos);
  }
  return offset_accum;
}

void MOD_lineart_chain_discard_short(LineartRenderBuffer *rb, const float threshold)
{
  LineartEdgeChain *ec, *next_rlc;
  for (ec = rb->chains.first; ec; ec = next_rlc) {
    next_rlc = ec->next;
    if (MOD_lineart_chain_compute_length(ec) < threshold) {
      BLI_remlink(&rb->chains, ec);
    }
  }
}

int MOD_lineart_chain_count(const LineartEdgeChain *ec)
{
  int count = 0;
  LISTBASE_FOREACH (LineartEdgeChainItem *, eci, &ec->chain) {
    count++;
  }
  return count;
}

void MOD_lineart_chain_clear_picked_flag(LineartRenderBuffer *rb)
{
  if (rb == NULL) {
    return;
  }
  LISTBASE_FOREACH (LineartEdgeChain *, ec, &rb->chains) {
    ec->picked = 0;
  }
}

/**
 * This should always be the last stage!, see the end of
 * #MOD_lineart_chain_split_for_fixed_occlusion().
 */
void MOD_lineart_chain_split_angle(LineartRenderBuffer *rb, float angle_threshold_rad)
{
  LineartEdgeChain *ec, *new_rlc;
  LineartEdgeChainItem *eci, *next_rlci, *prev_rlci;
  ListBase swap = {0};

  swap.first = rb->chains.first;
  swap.last = rb->chains.last;

  rb->chains.last = rb->chains.first = NULL;

  while ((ec = BLI_pophead(&swap)) != NULL) {
    ec->next = ec->prev = NULL;
    BLI_addtail(&rb->chains, ec);
    LineartEdgeChainItem *first_rlci = (LineartEdgeChainItem *)ec->chain.first;
    for (eci = first_rlci->next; eci; eci = next_rlci) {
      next_rlci = eci->next;
      prev_rlci = eci->prev;
      float angle = M_PI;
      if (next_rlci && prev_rlci) {
        angle = angle_v2v2v2(prev_rlci->pos, eci->pos, next_rlci->pos);
      }
      else {
        break; /* No need to split at the last point anyway.*/
      }
      if (angle < angle_threshold_rad) {
        new_rlc = lineart_chain_create(rb);
        new_rlc->chain.first = eci;
        new_rlc->chain.last = ec->chain.last;
        ec->chain.last = eci->prev;
        ((LineartEdgeChainItem *)ec->chain.last)->next = 0;
        eci->prev = 0;

        /* End the previous one. */
        lineart_chain_append_point(rb,
                                   ec,
                                   eci->pos,
                                   eci->gpos,
                                   eci->normal,
                                   eci->line_type,
                                   ec->level,
                                   eci->transparency_mask,
                                   eci->index);
        new_rlc->object_ref = ec->object_ref;
        new_rlc->type = ec->type;
        new_rlc->level = ec->level;
        new_rlc->transparency_mask = ec->transparency_mask;
        ec = new_rlc;
      }
    }
  }
}
