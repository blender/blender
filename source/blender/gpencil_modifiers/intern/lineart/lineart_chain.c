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

#define LRT_OTHER_RV(e, rv) ((rv) == (e)->v1 ? (e)->v2 : ((rv) == (e)->v2 ? (e)->v1 : NULL))

/* Get a connected line, only for lines who has the exact given vert, or (in the case of
 * intersection lines) who has a vert that has the exact same position. */
static LineartEdge *lineart_line_get_connected(LineartBoundingArea *ba,
                                               LineartVert *rv,
                                               LineartVert **new_rv,
                                               int match_flag)
{
  LISTBASE_FOREACH (LinkData *, lip, &ba->linked_lines) {
    LineartEdge *n_e = lip->data;

    if ((!(n_e->flags & LRT_EDGE_FLAG_ALL_TYPE)) || (n_e->flags & LRT_EDGE_FLAG_CHAIN_PICKED)) {
      continue;
    }

    if (match_flag && ((n_e->flags & LRT_EDGE_FLAG_ALL_TYPE) & match_flag) == 0) {
      continue;
    }

    *new_rv = LRT_OTHER_RV(n_e, rv);
    if (*new_rv) {
      return n_e;
    }

    if (n_e->flags & LRT_EDGE_FLAG_INTERSECTION) {
      if (rv->fbcoord[0] == n_e->v1->fbcoord[0] && rv->fbcoord[1] == n_e->v1->fbcoord[1]) {
        *new_rv = LRT_OTHER_RV(n_e, n_e->v1);
        return n_e;
      }
      if (rv->fbcoord[0] == n_e->v2->fbcoord[0] && rv->fbcoord[1] == n_e->v2->fbcoord[1]) {
        *new_rv = LRT_OTHER_RV(n_e, n_e->v2);
        return n_e;
      }
    }
  }

  return NULL;
}

static LineartLineChain *lineart_chain_create(LineartRenderBuffer *rb)
{
  LineartLineChain *rlc;
  rlc = lineart_mem_aquire(&rb->render_data_pool, sizeof(LineartLineChain));

  BLI_addtail(&rb->chains, rlc);

  return rlc;
}

static bool lineart_point_overlapping(LineartLineChainItem *rlci,
                                      float x,
                                      float y,
                                      double threshold)
{
  if (!rlci) {
    return false;
  }
  if (((rlci->pos[0] + threshold) >= x) && ((rlci->pos[0] - threshold) <= x) &&
      ((rlci->pos[1] + threshold) >= y) && ((rlci->pos[1] - threshold) <= y)) {
    return true;
  }
  return false;
}

static LineartLineChainItem *lineart_chain_append_point(LineartRenderBuffer *rb,
                                                        LineartLineChain *rlc,
                                                        float *fbcoord,
                                                        float *gpos,
                                                        float *normal,
                                                        char type,
                                                        int level,
                                                        unsigned char transparency_mask,
                                                        size_t index)
{
  LineartLineChainItem *rlci;

  if (lineart_point_overlapping(rlc->chain.last, fbcoord[0], fbcoord[1], 1e-5)) {
    /* Because the new chain point is overlapping, just replace the type and occlusion level of the
     * current point. This makes it so that the line to the point after this one has the correct
     * type and level. */
    LineartLineChainItem *old_rlci = rlc->chain.last;
    old_rlci->line_type = type;
    old_rlci->occlusion = level;
    old_rlci->transparency_mask = transparency_mask;
    return old_rlci;
  }

  rlci = lineart_mem_aquire(&rb->render_data_pool, sizeof(LineartLineChainItem));

  copy_v2_v2(rlci->pos, fbcoord);
  copy_v3_v3(rlci->gpos, gpos);
  rlci->index = index;
  copy_v3_v3(rlci->normal, normal);
  rlci->line_type = type & LRT_EDGE_FLAG_ALL_TYPE;
  rlci->occlusion = level;
  rlci->transparency_mask = transparency_mask;
  BLI_addtail(&rlc->chain, rlci);

  return rlci;
}

static LineartLineChainItem *lineart_chain_prepend_point(LineartRenderBuffer *rb,
                                                         LineartLineChain *rlc,
                                                         float *fbcoord,
                                                         float *gpos,
                                                         float *normal,
                                                         char type,
                                                         int level,
                                                         unsigned char transparency_mask,
                                                         size_t index)
{
  LineartLineChainItem *rlci;

  if (lineart_point_overlapping(rlc->chain.first, fbcoord[0], fbcoord[1], 1e-5)) {
    return rlc->chain.first;
  }

  rlci = lineart_mem_aquire(&rb->render_data_pool, sizeof(LineartLineChainItem));

  copy_v2_v2(rlci->pos, fbcoord);
  copy_v3_v3(rlci->gpos, gpos);
  rlci->index = index;
  copy_v3_v3(rlci->normal, normal);
  rlci->line_type = type & LRT_EDGE_FLAG_ALL_TYPE;
  rlci->occlusion = level;
  rlci->transparency_mask = transparency_mask;
  BLI_addhead(&rlc->chain, rlci);

  return rlci;
}

void MOD_lineart_chain_feature_lines(LineartRenderBuffer *rb)
{
  LineartLineChain *rlc;
  LineartLineChainItem *rlci;
  LineartBoundingArea *ba;
  LineartLineSegment *rls;
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

    rlc = lineart_chain_create(rb);

    /* One chain can only have one object_ref,
     * so we assign it based on the first segment we found. */
    rlc->object_ref = e->object_ref;

    LineartEdge *new_e = e;
    LineartVert *new_rv;
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
    new_rv = e->v1;
    rls = e->segments.first;
    VERT_COORD_TO_FLOAT(new_rv);
    lineart_chain_prepend_point(rb,
                                rlc,
                                use_fbcoord,
                                use_gpos,
                                N,
                                e->flags,
                                rls->occlusion,
                                rls->transparency_mask,
                                e->v1_obindex);
    while (ba && (new_e = lineart_line_get_connected(ba, new_rv, &new_rv, e->flags))) {
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

      if (new_rv == new_e->v1) {
        for (rls = new_e->segments.last; rls; rls = rls->prev) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * rls->at / (rls->at * lfb[3] + (1 - rls->at) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, rls->at);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_prepend_point(rb,
                                      rlc,
                                      use_fbcoord,
                                      use_gpos,
                                      N,
                                      new_e->flags,
                                      rls->occlusion,
                                      rls->transparency_mask,
                                      new_e->v1_obindex);
          last_occlusion = rls->occlusion;
          last_transparency = rls->transparency_mask;
        }
      }
      else if (new_rv == new_e->v2) {
        rls = new_e->segments.first;
        last_occlusion = rls->occlusion;
        last_transparency = rls->transparency_mask;
        rls = rls->next;
        for (; rls; rls = rls->next) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * rls->at / (rls->at * lfb[3] + (1 - rls->at) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, rls->at);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_prepend_point(rb,
                                      rlc,
                                      use_fbcoord,
                                      use_gpos,
                                      N,
                                      new_e->flags,
                                      last_occlusion,
                                      last_transparency,
                                      new_e->v2_obindex);
          last_occlusion = rls->occlusion;
          last_transparency = rls->transparency_mask;
        }
        VERT_COORD_TO_FLOAT(new_e->v2);
        lineart_chain_prepend_point(rb,
                                    rlc,
                                    use_fbcoord,
                                    use_gpos,
                                    N,
                                    new_e->flags,
                                    last_occlusion,
                                    last_transparency,
                                    new_e->v2_obindex);
      }
      ba = MOD_lineart_get_bounding_area(rb, new_rv->fbcoord[0], new_rv->fbcoord[1]);
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
    rls = e->segments.first;
    last_occlusion = ((LineartLineSegment *)rls)->occlusion;
    last_transparency = ((LineartLineSegment *)rls)->transparency_mask;
    for (rls = rls->next; rls; rls = rls->next) {
      double gpos[3], lpos[3];
      double *lfb = e->v1->fbcoord, *rfb = e->v2->fbcoord;
      double global_at = lfb[3] * rls->at / (rls->at * lfb[3] + (1 - rls->at) * rfb[3]);
      interp_v3_v3v3_db(lpos, e->v1->fbcoord, e->v2->fbcoord, rls->at);
      interp_v3_v3v3_db(gpos, e->v1->gloc, e->v2->gloc, global_at);
      POS_TO_FLOAT(lpos, gpos)
      lineart_chain_append_point(rb,
                                 rlc,
                                 use_fbcoord,
                                 use_gpos,
                                 N,
                                 e->flags,
                                 rls->occlusion,
                                 rls->transparency_mask,
                                 e->v1_obindex);
      last_occlusion = rls->occlusion;
      last_transparency = rls->transparency_mask;
    }
    VERT_COORD_TO_FLOAT(e->v2)
    lineart_chain_append_point(rb,
                               rlc,
                               use_fbcoord,
                               use_gpos,
                               N,
                               e->flags,
                               last_occlusion,
                               last_transparency,
                               e->v2_obindex);

    /*  Step 3: grow right. */
    ba = MOD_lineart_get_bounding_area(rb, e->v2->fbcoord[0], e->v2->fbcoord[1]);
    new_rv = e->v2;
    while (ba && (new_e = lineart_line_get_connected(ba, new_rv, &new_rv, e->flags))) {
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
      rlci = rlc->chain.last;
      rlci->line_type = new_e->flags & LRT_EDGE_FLAG_ALL_TYPE;

      if (new_rv == new_e->v1) {
        rls = new_e->segments.last;
        last_occlusion = rls->occlusion;
        last_transparency = rls->transparency_mask;
        /* Fix leading vertex occlusion. */
        rlci->occlusion = last_occlusion;
        rlci->transparency_mask = last_transparency;
        for (rls = new_e->segments.last; rls; rls = rls->prev) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * rls->at / (rls->at * lfb[3] + (1 - rls->at) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, rls->at);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          last_occlusion = rls->prev ? rls->prev->occlusion : last_occlusion;
          last_transparency = rls->prev ? rls->prev->transparency_mask : last_transparency;
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_append_point(rb,
                                     rlc,
                                     use_fbcoord,
                                     use_gpos,
                                     N,
                                     new_e->flags,
                                     last_occlusion,
                                     last_transparency,
                                     new_e->v1_obindex);
        }
      }
      else if (new_rv == new_e->v2) {
        rls = new_e->segments.first;
        last_occlusion = rls->occlusion;
        last_transparency = rls->transparency_mask;
        rlci->occlusion = last_occlusion;
        rlci->transparency_mask = last_transparency;
        rls = rls->next;
        for (; rls; rls = rls->next) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * rls->at / (rls->at * lfb[3] + (1 - rls->at) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, rls->at);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_append_point(rb,
                                     rlc,
                                     use_fbcoord,
                                     use_gpos,
                                     N,
                                     new_e->flags,
                                     rls->occlusion,
                                     rls->transparency_mask,
                                     new_e->v2_obindex);
          last_occlusion = rls->occlusion;
          last_transparency = rls->transparency_mask;
        }
        VERT_COORD_TO_FLOAT(new_e->v2)
        lineart_chain_append_point(rb,
                                   rlc,
                                   use_fbcoord,
                                   use_gpos,
                                   N,
                                   new_e->flags,
                                   last_occlusion,
                                   last_transparency,
                                   new_e->v2_obindex);
      }
      ba = MOD_lineart_get_bounding_area(rb, new_rv->fbcoord[0], new_rv->fbcoord[1]);
    }
    if (rb->fuzzy_everything) {
      rlc->type = LRT_EDGE_FLAG_CONTOUR;
    }
    else {
      rlc->type = (e->flags & LRT_EDGE_FLAG_ALL_TYPE);
    }
  }
  LRT_ITER_ALL_LINES_END
}

static LineartBoundingArea *lineart_bounding_area_get_rlci_recursive(LineartRenderBuffer *rb,
                                                                     LineartBoundingArea *root,
                                                                     LineartLineChainItem *rlci)
{
  if (root->child == NULL) {
    return root;
  }

  LineartBoundingArea *ch = root->child;
#define IN_BOUND(ba, rlci) \
  ba.l <= rlci->pos[0] && ba.r >= rlci->pos[0] && ba.b <= rlci->pos[1] && ba.u >= rlci->pos[1]

  if (IN_BOUND(ch[0], rlci)) {
    return lineart_bounding_area_get_rlci_recursive(rb, &ch[0], rlci);
  }
  if (IN_BOUND(ch[1], rlci)) {
    return lineart_bounding_area_get_rlci_recursive(rb, &ch[1], rlci);
  }
  if (IN_BOUND(ch[2], rlci)) {
    return lineart_bounding_area_get_rlci_recursive(rb, &ch[2], rlci);
  }
  if (IN_BOUND(ch[3], rlci)) {
    return lineart_bounding_area_get_rlci_recursive(rb, &ch[3], rlci);
  }
#undef IN_BOUND
  return NULL;
}

static LineartBoundingArea *lineart_bounding_area_get_end_point(LineartRenderBuffer *rb,
                                                                LineartLineChainItem *rlci)
{
  if (!rlci) {
    return NULL;
  }
  LineartBoundingArea *root = MOD_lineart_get_parent_bounding_area(rb, rlci->pos[0], rlci->pos[1]);
  if (root == NULL) {
    return NULL;
  }
  return lineart_bounding_area_get_rlci_recursive(rb, root, rlci);
}

/**
 * Here we will try to connect geometry space chains together in image space. However we can't
 * chain two chains together if their end and start points lie on the border between two bounding
 * areas, this happens either when 1) the geometry is way too dense, or 2) the chaining threshold
 * is too big that it covers multiple small bounding areas.
 */
static void lineart_bounding_area_link_point_recursive(LineartRenderBuffer *rb,
                                                       LineartBoundingArea *root,
                                                       LineartLineChain *rlc,
                                                       LineartLineChainItem *rlci)
{
  if (root->child == NULL) {
    LineartChainRegisterEntry *cre = lineart_list_append_pointer_pool_sized(
        &root->linked_chains, &rb->render_data_pool, rlc, sizeof(LineartChainRegisterEntry));

    cre->rlci = rlci;

    if (rlci == rlc->chain.first) {
      cre->is_left = 1;
    }
  }
  else {
    LineartBoundingArea *ch = root->child;

#define IN_BOUND(ba, rlci) \
  ba.l <= rlci->pos[0] && ba.r >= rlci->pos[0] && ba.b <= rlci->pos[1] && ba.u >= rlci->pos[1]

    if (IN_BOUND(ch[0], rlci)) {
      lineart_bounding_area_link_point_recursive(rb, &ch[0], rlc, rlci);
    }
    else if (IN_BOUND(ch[1], rlci)) {
      lineart_bounding_area_link_point_recursive(rb, &ch[1], rlc, rlci);
    }
    else if (IN_BOUND(ch[2], rlci)) {
      lineart_bounding_area_link_point_recursive(rb, &ch[2], rlc, rlci);
    }
    else if (IN_BOUND(ch[3], rlci)) {
      lineart_bounding_area_link_point_recursive(rb, &ch[3], rlc, rlci);
    }

#undef IN_BOUND
  }
}

static void lineart_bounding_area_link_chain(LineartRenderBuffer *rb, LineartLineChain *rlc)
{
  LineartLineChainItem *pl = rlc->chain.first;
  LineartLineChainItem *pr = rlc->chain.last;
  LineartBoundingArea *ba1 = MOD_lineart_get_parent_bounding_area(rb, pl->pos[0], pl->pos[1]);
  LineartBoundingArea *ba2 = MOD_lineart_get_parent_bounding_area(rb, pr->pos[0], pr->pos[1]);

  if (ba1) {
    lineart_bounding_area_link_point_recursive(rb, ba1, rlc, pl);
  }
  if (ba2) {
    lineart_bounding_area_link_point_recursive(rb, ba2, rlc, pr);
  }
}

void MOD_lineart_chain_split_for_fixed_occlusion(LineartRenderBuffer *rb)
{
  LineartLineChain *rlc, *new_rlc;
  LineartLineChainItem *rlci, *next_rlci;
  ListBase swap = {0};

  swap.first = rb->chains.first;
  swap.last = rb->chains.last;

  rb->chains.last = rb->chains.first = NULL;

  while ((rlc = BLI_pophead(&swap)) != NULL) {
    rlc->next = rlc->prev = NULL;
    BLI_addtail(&rb->chains, rlc);
    LineartLineChainItem *first_rlci = (LineartLineChainItem *)rlc->chain.first;
    int fixed_occ = first_rlci->occlusion;
    unsigned char fixed_mask = first_rlci->transparency_mask;
    rlc->level = fixed_occ;
    rlc->transparency_mask = fixed_mask;
    for (rlci = first_rlci->next; rlci; rlci = next_rlci) {
      next_rlci = rlci->next;
      if (rlci->occlusion != fixed_occ || rlci->transparency_mask != fixed_mask) {
        if (next_rlci) {
          if (lineart_point_overlapping(next_rlci, rlci->pos[0], rlci->pos[1], 1e-5)) {
            continue;
          }
        }
        else {
          /* Set the same occlusion level for the end vertex, so when further connection is needed
           * the backwards occlusion info is also correct.  */
          rlci->occlusion = fixed_occ;
          rlci->transparency_mask = fixed_mask;
          /* No need to split at the last point anyway. */
          break;
        }
        new_rlc = lineart_chain_create(rb);
        new_rlc->chain.first = rlci;
        new_rlc->chain.last = rlc->chain.last;
        rlc->chain.last = rlci->prev;
        ((LineartLineChainItem *)rlc->chain.last)->next = 0;
        rlci->prev = 0;

        /* End the previous one. */
        lineart_chain_append_point(rb,
                                   rlc,
                                   rlci->pos,
                                   rlci->gpos,
                                   rlci->normal,
                                   rlci->line_type,
                                   fixed_occ,
                                   fixed_mask,
                                   rlci->index);
        new_rlc->object_ref = rlc->object_ref;
        new_rlc->type = rlc->type;
        rlc = new_rlc;
        fixed_occ = rlci->occlusion;
        fixed_mask = rlci->transparency_mask;
        rlc->level = fixed_occ;
        rlc->transparency_mask = fixed_mask;
      }
    }
  }
  LISTBASE_FOREACH (LineartLineChain *, irlc, &rb->chains) {
    lineart_bounding_area_link_chain(rb, irlc);
  }
}

/**
 * Note: segment type (crease/material/contour...) is ambiguous after this.
 */
static void lineart_chain_connect(LineartRenderBuffer *UNUSED(rb),
                                  LineartLineChain *onto,
                                  LineartLineChain *sub,
                                  int reverse_1,
                                  int reverse_2)
{
  LineartLineChainItem *rlci;
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
    rlci = sub->chain.first;
    if (lineart_point_overlapping(onto->chain.last, rlci->pos[0], rlci->pos[1], 1e-5)) {
      BLI_pophead(&sub->chain);
      if (sub->chain.first == NULL) {
        return;
      }
    }
    ((LineartLineChainItem *)onto->chain.last)->next = sub->chain.first;
    ((LineartLineChainItem *)sub->chain.first)->prev = onto->chain.last;
    onto->chain.last = sub->chain.last;
  }
  else {              /*  L-R L--R. */
    if (!reverse_2) { /*  R-L L--R. */
      BLI_listbase_reverse(&sub->chain);
    }
    rlci = onto->chain.first;
    if (lineart_point_overlapping(sub->chain.last, rlci->pos[0], rlci->pos[1], 1e-5)) {
      BLI_pophead(&onto->chain);
      if (onto->chain.first == NULL) {
        return;
      }
    }
    ((LineartLineChainItem *)sub->chain.last)->next = onto->chain.first;
    ((LineartLineChainItem *)onto->chain.first)->prev = sub->chain.last;
    onto->chain.first = sub->chain.first;
  }
}

static LineartChainRegisterEntry *lineart_chain_get_closest_cre(LineartRenderBuffer *rb,
                                                                LineartBoundingArea *ba,
                                                                LineartLineChain *rlc,
                                                                LineartLineChainItem *rlci,
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
    if (cre->rlc->object_ref != rlc->object_ref) {
      if (!rb->fuzzy_everything) {
        if (rb->fuzzy_intersections) {
          /* If none of those are intersection lines... */
          if ((!(cre->rlc->type & LRT_EDGE_FLAG_INTERSECTION)) &&
              (!(rlc->type & LRT_EDGE_FLAG_INTERSECTION))) {
            continue; /* We don't want to chain along different objects at the moment. */
          }
        }
        else {
          continue;
        }
      }
    }
    if (cre->rlc->picked || cre->picked) {
      continue;
    }
    if (cre->rlc == rlc || (!cre->rlc->chain.first) || (cre->rlc->level != occlusion) ||
        (cre->rlc->transparency_mask != transparency_mask)) {
      continue;
    }
    if (!rb->fuzzy_everything) {
      if (cre->rlc->type != rlc->type) {
        if (rb->fuzzy_intersections) {
          if (!(cre->rlc->type == LRT_EDGE_FLAG_INTERSECTION ||
                rlc->type == LRT_EDGE_FLAG_INTERSECTION)) {
            continue; /* Fuzzy intersections but no intersection line found. */
          }
        }
        else { /* Line type different but no fuzzy. */
          continue;
        }
      }
    }

    float new_len = len_v2v2(cre->rlci->pos, rlci->pos);
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
          rb, sba, rlc, rlci, occlusion, transparency_mask, dist, &adjacent_new_len, ba); \
      if (adjacent_new_len < dist) { \
        dist = adjacent_new_len; \
        closest_cre = adjacent_closest; \
      } \
    } \
  }
  if (!caller_ba) {
    LRT_TEST_ADJACENT_AREAS(rlci->pos[0] - ba->l, &ba->lp);
    LRT_TEST_ADJACENT_AREAS(ba->r - rlci->pos[0], &ba->rp);
    LRT_TEST_ADJACENT_AREAS(ba->u - rlci->pos[1], &ba->up);
    LRT_TEST_ADJACENT_AREAS(rlci->pos[1] - ba->b, &ba->bp);
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
  LineartLineChain *rlc;
  LineartLineChainItem *rlci_l, *rlci_r;
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

  while ((rlc = BLI_pophead(&swap)) != NULL) {
    rlc->next = rlc->prev = NULL;
    if (rlc->picked) {
      continue;
    }
    BLI_addtail(&rb->chains, rlc);

    occlusion = rlc->level;
    transparency_mask = rlc->transparency_mask;

    rlci_l = rlc->chain.first;
    rlci_r = rlc->chain.last;
    while ((ba_l = lineart_bounding_area_get_end_point(rb, rlci_l)) &&
           (ba_r = lineart_bounding_area_get_end_point(rb, rlci_r))) {
      closest_cre_l = lineart_chain_get_closest_cre(
          rb, ba_l, rlc, rlci_l, occlusion, transparency_mask, dist, &dist_l, NULL);
      closest_cre_r = lineart_chain_get_closest_cre(
          rb, ba_r, rlc, rlci_r, occlusion, transparency_mask, dist, &dist_r, NULL);
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
      closest_cre->rlc->picked = 1;
      if (closest_cre->is_left) {
        lineart_chain_connect(rb, rlc, closest_cre->rlc, reverse_main, 0);
      }
      else {
        lineart_chain_connect(rb, rlc, closest_cre->rlc, reverse_main, 1);
      }
      BLI_remlink(&swap, closest_cre->rlc);
      rlci_l = rlc->chain.first;
      rlci_r = rlc->chain.last;
    }
    rlc->picked = 1;
  }
}

/**
 * Length is in image space.
 */
float MOD_lineart_chain_compute_length(LineartLineChain *rlc)
{
  LineartLineChainItem *rlci;
  float offset_accum = 0;
  float dist;
  float last_point[2];

  rlci = rlc->chain.first;
  copy_v2_v2(last_point, rlci->pos);
  for (rlci = rlc->chain.first; rlci; rlci = rlci->next) {
    dist = len_v2v2(rlci->pos, last_point);
    offset_accum += dist;
    copy_v2_v2(last_point, rlci->pos);
  }
  return offset_accum;
}

void MOD_lineart_chain_discard_short(LineartRenderBuffer *rb, const float threshold)
{
  LineartLineChain *rlc, *next_rlc;
  for (rlc = rb->chains.first; rlc; rlc = next_rlc) {
    next_rlc = rlc->next;
    if (MOD_lineart_chain_compute_length(rlc) < threshold) {
      BLI_remlink(&rb->chains, rlc);
    }
  }
}

int MOD_lineart_chain_count(const LineartLineChain *rlc)
{
  int count = 0;
  LISTBASE_FOREACH (LineartLineChainItem *, rlci, &rlc->chain) {
    count++;
  }
  return count;
}

void MOD_lineart_chain_clear_picked_flag(LineartRenderBuffer *rb)
{
  if (rb == NULL) {
    return;
  }
  LISTBASE_FOREACH (LineartLineChain *, rlc, &rb->chains) {
    rlc->picked = 0;
  }
}

/**
 * This should always be the last stage!, see the end of
 * #MOD_lineart_chain_split_for_fixed_occlusion().
 */
void MOD_lineart_chain_split_angle(LineartRenderBuffer *rb, float angle_threshold_rad)
{
  LineartLineChain *rlc, *new_rlc;
  LineartLineChainItem *rlci, *next_rlci, *prev_rlci;
  ListBase swap = {0};

  swap.first = rb->chains.first;
  swap.last = rb->chains.last;

  rb->chains.last = rb->chains.first = NULL;

  while ((rlc = BLI_pophead(&swap)) != NULL) {
    rlc->next = rlc->prev = NULL;
    BLI_addtail(&rb->chains, rlc);
    LineartLineChainItem *first_rlci = (LineartLineChainItem *)rlc->chain.first;
    for (rlci = first_rlci->next; rlci; rlci = next_rlci) {
      next_rlci = rlci->next;
      prev_rlci = rlci->prev;
      float angle = M_PI;
      if (next_rlci && prev_rlci) {
        angle = angle_v2v2v2(prev_rlci->pos, rlci->pos, next_rlci->pos);
      }
      else {
        break; /* No need to split at the last point anyway.*/
      }
      if (angle < angle_threshold_rad) {
        new_rlc = lineart_chain_create(rb);
        new_rlc->chain.first = rlci;
        new_rlc->chain.last = rlc->chain.last;
        rlc->chain.last = rlci->prev;
        ((LineartLineChainItem *)rlc->chain.last)->next = 0;
        rlci->prev = 0;

        /* End the previous one. */
        lineart_chain_append_point(rb,
                                   rlc,
                                   rlci->pos,
                                   rlci->gpos,
                                   rlci->normal,
                                   rlci->line_type,
                                   rlc->level,
                                   rlci->transparency_mask,
                                   rlci->index);
        new_rlc->object_ref = rlc->object_ref;
        new_rlc->type = rlc->type;
        new_rlc->level = rlc->level;
        new_rlc->transparency_mask = rlc->transparency_mask;
        rlc = new_rlc;
      }
    }
  }
}
