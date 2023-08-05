/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "MOD_lineart.h"

#include "lineart_intern.h"

#include <cmath>

#define LRT_OTHER_VERT(e, vt) ((vt) == (e)->v1 ? (e)->v2 : ((vt) == (e)->v2 ? (e)->v1 : nullptr))

struct Object;

/* Get a connected line, only for lines who has the exact given vert, or (in the case of
 * intersection lines) who has a vert that has the exact same position. */
static LineartEdge *lineart_line_get_connected(LineartBoundingArea *ba,
                                               LineartVert *vt,
                                               LineartVert **new_vt,
                                               int match_flag,
                                               uint8_t match_isec_mask,
                                               Object *match_isec_object)
{
  for (int i = 0; i < ba->line_count; i++) {
    LineartEdge *n_e = ba->linked_lines[i];

    if (!(n_e->flags & LRT_EDGE_FLAG_ALL_TYPE) || (n_e->flags & LRT_EDGE_FLAG_CHAIN_PICKED)) {
      continue;
    }

    if (match_flag && ((n_e->flags & LRT_EDGE_FLAG_ALL_TYPE) & match_flag) == 0) {
      continue;
    }

    if (n_e->intersection_mask != match_isec_mask) {
      continue;
    }

    *new_vt = LRT_OTHER_VERT(n_e, vt);
    if (*new_vt) {
      return n_e;
    }

    if (n_e->flags & LRT_EDGE_FLAG_INTERSECTION) {
      if (n_e->object_ref != match_isec_object) {
        continue;
      }
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

  return nullptr;
}

static LineartEdgeChain *lineart_chain_create(LineartData *ld)
{
  LineartEdgeChain *ec;
  ec = static_cast<LineartEdgeChain *>(
      lineart_mem_acquire(ld->chain_data_pool, sizeof(LineartEdgeChain)));

  BLI_addtail(&ld->chains, ec);

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
      ((eci->pos[1] + threshold) >= y) && ((eci->pos[1] - threshold) <= y))
  {
    return true;
  }
  return false;
}

static LineartEdgeChainItem *lineart_chain_append_point(LineartData *ld,
                                                        LineartEdgeChain *ec,
                                                        float fbcoord[4],
                                                        float gpos[3],
                                                        float normal[3],
                                                        uint8_t type,
                                                        int level,
                                                        uint8_t material_mask_bits,
                                                        uint32_t shadow_mask_bits,
                                                        size_t index)
{
  LineartEdgeChainItem *eci;

  if (lineart_point_overlapping(
          static_cast<LineartEdgeChainItem *>(ec->chain.last), fbcoord[0], fbcoord[1], 1e-5))
  {
    /* Because the new chain point is overlapping, just replace the type and occlusion level of the
     * current point. This makes it so that the line to the point after this one has the correct
     * type and level. */
    LineartEdgeChainItem *old_eci = static_cast<LineartEdgeChainItem *>(ec->chain.last);
    old_eci->line_type = type;
    old_eci->occlusion = level;
    old_eci->material_mask_bits = material_mask_bits;
    old_eci->shadow_mask_bits = shadow_mask_bits;
    return old_eci;
  }

  eci = static_cast<LineartEdgeChainItem *>(
      lineart_mem_acquire(ld->chain_data_pool, sizeof(LineartEdgeChainItem)));

  copy_v4_v4(eci->pos, fbcoord);
  copy_v3_v3(eci->gpos, gpos);
  eci->index = index;
  copy_v3_v3(eci->normal, normal);
  eci->line_type = type & LRT_EDGE_FLAG_ALL_TYPE;
  eci->occlusion = level;
  eci->material_mask_bits = material_mask_bits;
  eci->shadow_mask_bits = shadow_mask_bits;
  BLI_addtail(&ec->chain, eci);

  return eci;
}

static LineartEdgeChainItem *lineart_chain_prepend_point(LineartData *ld,
                                                         LineartEdgeChain *ec,
                                                         float fbcoord[4],
                                                         float gpos[3],
                                                         float normal[3],
                                                         uint8_t type,
                                                         int level,
                                                         uint8_t material_mask_bits,
                                                         uint32_t shadow_mask_bits,
                                                         size_t index)
{
  LineartEdgeChainItem *eci;

  if (lineart_point_overlapping(
          static_cast<LineartEdgeChainItem *>(ec->chain.first), fbcoord[0], fbcoord[1], 1e-5))
  {
    return static_cast<LineartEdgeChainItem *>(ec->chain.first);
  }

  eci = static_cast<LineartEdgeChainItem *>(
      lineart_mem_acquire(ld->chain_data_pool, sizeof(LineartEdgeChainItem)));

  copy_v4_v4(eci->pos, fbcoord);
  copy_v3_v3(eci->gpos, gpos);
  eci->index = index;
  copy_v3_v3(eci->normal, normal);
  eci->line_type = type & LRT_EDGE_FLAG_ALL_TYPE;
  eci->occlusion = level;
  eci->material_mask_bits = material_mask_bits;
  eci->shadow_mask_bits = shadow_mask_bits;
  BLI_addhead(&ec->chain, eci);

  return eci;
}

void MOD_lineart_chain_feature_lines(LineartData *ld)
{
  LineartEdgeChain *ec;
  LineartEdgeChainItem *eci;
  LineartBoundingArea *ba;
  LineartEdgeSegment *es;
  int last_occlusion;
  uint8_t last_transparency;
  uint32_t last_shadow;
  /* Used when converting from double. */
  float use_fbcoord[4];
  float use_gpos[3];

#define VERT_COORD_TO_FLOAT(a) \
  copy_v4fl_v4db(use_fbcoord, (a)->fbcoord); \
  copy_v3fl_v3db(use_gpos, (a)->gloc);

#define POS_TO_FLOAT(lpos, gpos) \
  copy_v3fl_v3db(use_fbcoord, lpos); \
  copy_v3fl_v3db(use_gpos, gpos);

  LRT_ITER_ALL_LINES_BEGIN
  {
    if (!(e->flags & LRT_EDGE_FLAG_ALL_TYPE) || (e->flags & LRT_EDGE_FLAG_CHAIN_PICKED)) {
      LRT_ITER_ALL_LINES_NEXT
      continue;
    }

    e->flags |= LRT_EDGE_FLAG_CHAIN_PICKED;

    ec = lineart_chain_create(ld);

    /* One chain can only have one object_ref and intersection_mask,
     * so we assign them based on the first segment we found. */
    ec->object_ref = e->object_ref;
    ec->intersection_mask = e->intersection_mask;

    LineartEdge *new_e;
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
    ba = MOD_lineart_get_bounding_area(ld, e->v1->fbcoord[0], e->v1->fbcoord[1]);
    new_vt = e->v1;
    es = static_cast<LineartEdgeSegment *>(e->segments.first);
    VERT_COORD_TO_FLOAT(new_vt);
    lineart_chain_prepend_point(ld,
                                ec,
                                use_fbcoord,
                                use_gpos,
                                N,
                                e->flags,
                                es->occlusion,
                                es->material_mask_bits,
                                es->shadow_mask_bits,
                                e->v1->index);
    while (ba && (new_e = lineart_line_get_connected(
                      ba, new_vt, &new_vt, e->flags, ec->intersection_mask, ec->object_ref)))
    {
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
        LISTBASE_FOREACH_BACKWARD (LineartEdgeSegment *, es, &new_e->segments) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * es->ratio / (es->ratio * lfb[3] + (1 - es->ratio) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, es->ratio);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          use_fbcoord[3] = interpf(new_e->v2->fbcoord[3], new_e->v1->fbcoord[3], global_at);
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_prepend_point(ld,
                                      ec,
                                      use_fbcoord,
                                      use_gpos,
                                      N,
                                      new_e->flags,
                                      es->occlusion,
                                      es->material_mask_bits,
                                      es->shadow_mask_bits,
                                      new_e->v1->index);
          last_occlusion = es->occlusion;
          last_transparency = es->material_mask_bits;
          last_shadow = es->shadow_mask_bits;
        }
      }
      else if (new_vt == new_e->v2) {
        es = static_cast<LineartEdgeSegment *>(new_e->segments.first);
        last_occlusion = es->occlusion;
        last_transparency = es->material_mask_bits;
        last_shadow = es->shadow_mask_bits;
        es = es->next;
        for (; es; es = es->next) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * es->ratio / (es->ratio * lfb[3] + (1 - es->ratio) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, es->ratio);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          use_fbcoord[3] = interpf(new_e->v2->fbcoord[3], new_e->v1->fbcoord[3], global_at);
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_prepend_point(ld,
                                      ec,
                                      use_fbcoord,
                                      use_gpos,
                                      N,
                                      new_e->flags,
                                      last_occlusion,
                                      last_transparency,
                                      last_shadow,
                                      new_e->v2->index);
          last_occlusion = es->occlusion;
          last_transparency = es->material_mask_bits;
          last_shadow = es->shadow_mask_bits;
        }
        VERT_COORD_TO_FLOAT(new_e->v2);
        lineart_chain_prepend_point(ld,
                                    ec,
                                    use_fbcoord,
                                    use_gpos,
                                    N,
                                    new_e->flags,
                                    last_occlusion,
                                    last_transparency,
                                    last_shadow,
                                    new_e->v2->index);
      }
      ba = MOD_lineart_get_bounding_area(ld, new_vt->fbcoord[0], new_vt->fbcoord[1]);
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
    es = static_cast<LineartEdgeSegment *>(e->segments.first);
    last_occlusion = es->occlusion;
    last_transparency = es->material_mask_bits;
    last_shadow = es->shadow_mask_bits;
    for (es = es->next; es; es = es->next) {
      double gpos[3], lpos[3];
      double *lfb = e->v1->fbcoord, *rfb = e->v2->fbcoord;
      double global_at = lfb[3] * es->ratio / (es->ratio * lfb[3] + (1 - es->ratio) * rfb[3]);
      interp_v3_v3v3_db(lpos, e->v1->fbcoord, e->v2->fbcoord, es->ratio);
      interp_v3_v3v3_db(gpos, e->v1->gloc, e->v2->gloc, global_at);
      use_fbcoord[3] = interpf(e->v2->fbcoord[3], e->v1->fbcoord[3], global_at);
      POS_TO_FLOAT(lpos, gpos)
      lineart_chain_append_point(ld,
                                 ec,
                                 use_fbcoord,
                                 use_gpos,
                                 N,
                                 e->flags,
                                 es->occlusion,
                                 es->material_mask_bits,
                                 es->shadow_mask_bits,
                                 e->v1->index);
      last_occlusion = es->occlusion;
      last_transparency = es->material_mask_bits;
      last_shadow = es->shadow_mask_bits;
    }
    VERT_COORD_TO_FLOAT(e->v2)
    lineart_chain_append_point(ld,
                               ec,
                               use_fbcoord,
                               use_gpos,
                               N,
                               e->flags,
                               last_occlusion,
                               last_transparency,
                               last_shadow,
                               e->v2->index);

    /*  Step 3: grow right. */
    ba = MOD_lineart_get_bounding_area(ld, e->v2->fbcoord[0], e->v2->fbcoord[1]);
    new_vt = e->v2;
    while (ba && (new_e = lineart_line_get_connected(
                      ba, new_vt, &new_vt, e->flags, ec->intersection_mask, ec->object_ref)))
    {
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
      eci = static_cast<LineartEdgeChainItem *>(ec->chain.last);
      eci->line_type = new_e->flags & LRT_EDGE_FLAG_ALL_TYPE;

      if (new_vt == new_e->v1) {
        es = static_cast<LineartEdgeSegment *>(new_e->segments.last);
        last_occlusion = es->occlusion;
        last_transparency = es->material_mask_bits;
        last_shadow = es->shadow_mask_bits;
        /* Fix leading vertex occlusion. */
        eci->occlusion = last_occlusion;
        eci->material_mask_bits = last_transparency;
        eci->shadow_mask_bits = last_shadow;
        LISTBASE_FOREACH_BACKWARD (LineartEdgeSegment *, es, &new_e->segments) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * es->ratio / (es->ratio * lfb[3] + (1 - es->ratio) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, es->ratio);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          use_fbcoord[3] = interpf(new_e->v2->fbcoord[3], new_e->v1->fbcoord[3], global_at);
          last_occlusion = es->prev ? es->prev->occlusion : last_occlusion;
          last_transparency = es->prev ? es->prev->material_mask_bits : last_transparency;
          last_shadow = es->prev ? es->prev->shadow_mask_bits : last_shadow;
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_append_point(ld,
                                     ec,
                                     use_fbcoord,
                                     use_gpos,
                                     N,
                                     new_e->flags,
                                     last_occlusion,
                                     last_transparency,
                                     last_shadow,
                                     new_e->v1->index);
        }
      }
      else if (new_vt == new_e->v2) {
        es = static_cast<LineartEdgeSegment *>(new_e->segments.first);
        last_occlusion = es->occlusion;
        last_transparency = es->material_mask_bits;
        last_shadow = es->shadow_mask_bits;
        eci->occlusion = last_occlusion;
        eci->material_mask_bits = last_transparency;
        eci->shadow_mask_bits = last_shadow;
        es = es->next;
        for (; es; es = es->next) {
          double gpos[3], lpos[3];
          double *lfb = new_e->v1->fbcoord, *rfb = new_e->v2->fbcoord;
          double global_at = lfb[3] * es->ratio / (es->ratio * lfb[3] + (1 - es->ratio) * rfb[3]);
          interp_v3_v3v3_db(lpos, new_e->v1->fbcoord, new_e->v2->fbcoord, es->ratio);
          interp_v3_v3v3_db(gpos, new_e->v1->gloc, new_e->v2->gloc, global_at);
          use_fbcoord[3] = interpf(new_e->v2->fbcoord[3], new_e->v1->fbcoord[3], global_at);
          POS_TO_FLOAT(lpos, gpos)
          lineart_chain_append_point(ld,
                                     ec,
                                     use_fbcoord,
                                     use_gpos,
                                     N,
                                     new_e->flags,
                                     es->occlusion,
                                     es->material_mask_bits,
                                     es->shadow_mask_bits,
                                     new_e->v2->index);
          last_occlusion = es->occlusion;
          last_transparency = es->material_mask_bits;
          last_shadow = es->shadow_mask_bits;
        }
        VERT_COORD_TO_FLOAT(new_e->v2)
        lineart_chain_append_point(ld,
                                   ec,
                                   use_fbcoord,
                                   use_gpos,
                                   N,
                                   new_e->flags,
                                   last_occlusion,
                                   last_transparency,
                                   last_shadow,
                                   new_e->v2->index);
      }
      ba = MOD_lineart_get_bounding_area(ld, new_vt->fbcoord[0], new_vt->fbcoord[1]);
    }
    if (ld->conf.fuzzy_everything) {
      ec->type = LRT_EDGE_FLAG_CONTOUR;
    }
    else {
      ec->type = (e->flags & LRT_EDGE_FLAG_ALL_TYPE);
    }
  }
  LRT_ITER_ALL_LINES_END
}

static LineartBoundingArea *lineart_bounding_area_get_eci_recursive(LineartData *ld,
                                                                    LineartBoundingArea *root,
                                                                    LineartEdgeChainItem *eci)
{
  if (root->child == nullptr) {
    return root;
  }

  LineartBoundingArea *ch = root->child;
#define IN_BOUND(ba, eci) \
  ba.l <= eci->pos[0] && ba.r >= eci->pos[0] && ba.b <= eci->pos[1] && ba.u >= eci->pos[1]

  if (IN_BOUND(ch[0], eci)) {
    return lineart_bounding_area_get_eci_recursive(ld, &ch[0], eci);
  }
  if (IN_BOUND(ch[1], eci)) {
    return lineart_bounding_area_get_eci_recursive(ld, &ch[1], eci);
  }
  if (IN_BOUND(ch[2], eci)) {
    return lineart_bounding_area_get_eci_recursive(ld, &ch[2], eci);
  }
  if (IN_BOUND(ch[3], eci)) {
    return lineart_bounding_area_get_eci_recursive(ld, &ch[3], eci);
  }
#undef IN_BOUND
  return nullptr;
}

static LineartBoundingArea *lineart_bounding_area_get_end_point(LineartData *ld,
                                                                LineartEdgeChainItem *eci)
{
  if (!eci) {
    return nullptr;
  }
  LineartBoundingArea *root = MOD_lineart_get_parent_bounding_area(ld, eci->pos[0], eci->pos[1]);
  if (root == nullptr) {
    return nullptr;
  }
  return lineart_bounding_area_get_eci_recursive(ld, root, eci);
}

/**
 * Here we will try to connect geometry space chains together in image space. However we can't
 * chain two chains together if their end and start points lie on the border between two bounding
 * areas, this happens either when 1) the geometry is way too dense, or 2) the chaining threshold
 * is too big that it covers multiple small bounding areas.
 */
static void lineart_bounding_area_link_point_recursive(LineartData *ld,
                                                       LineartBoundingArea *root,
                                                       LineartEdgeChain *ec,
                                                       LineartEdgeChainItem *eci)
{
  if (root->child == nullptr) {
    LineartChainRegisterEntry *cre = static_cast<LineartChainRegisterEntry *>(
        lineart_list_append_pointer_pool_sized(
            &root->linked_chains, ld->chain_data_pool, ec, sizeof(LineartChainRegisterEntry)));

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
      lineart_bounding_area_link_point_recursive(ld, &ch[0], ec, eci);
    }
    else if (IN_BOUND(ch[1], eci)) {
      lineart_bounding_area_link_point_recursive(ld, &ch[1], ec, eci);
    }
    else if (IN_BOUND(ch[2], eci)) {
      lineart_bounding_area_link_point_recursive(ld, &ch[2], ec, eci);
    }
    else if (IN_BOUND(ch[3], eci)) {
      lineart_bounding_area_link_point_recursive(ld, &ch[3], ec, eci);
    }

#undef IN_BOUND
  }
}

static void lineart_bounding_area_link_chain(LineartData *ld, LineartEdgeChain *ec)
{
  LineartEdgeChainItem *pl = static_cast<LineartEdgeChainItem *>(ec->chain.first);
  LineartEdgeChainItem *pr = static_cast<LineartEdgeChainItem *>(ec->chain.last);
  LineartBoundingArea *ba1 = MOD_lineart_get_parent_bounding_area(ld, pl->pos[0], pl->pos[1]);
  LineartBoundingArea *ba2 = MOD_lineart_get_parent_bounding_area(ld, pr->pos[0], pr->pos[1]);

  if (ba1) {
    lineart_bounding_area_link_point_recursive(ld, ba1, ec, pl);
  }
  if (ba2) {
    lineart_bounding_area_link_point_recursive(ld, ba2, ec, pr);
  }
}

static bool lineart_chain_fix_ambiguous_segments(LineartEdgeChain *ec,
                                                 LineartEdgeChainItem *last_matching_eci,
                                                 float distance_threshold,
                                                 bool preserve_details,
                                                 LineartEdgeChainItem **r_next_eci)
{
  float dist_accum = 0;

  int fixed_occ = last_matching_eci->occlusion;
  uint8_t fixed_mask = last_matching_eci->material_mask_bits;
  uint32_t fixed_shadow = last_matching_eci->shadow_mask_bits;

  LineartEdgeChainItem *can_skip_to = nullptr;
  LineartEdgeChainItem *last_eci = last_matching_eci;
  for (LineartEdgeChainItem *eci = last_matching_eci->next; eci; eci = eci->next) {
    dist_accum += len_v2v2(last_eci->pos, eci->pos);
    if (dist_accum > distance_threshold) {
      break;
    }
    last_eci = eci;
    /* The reason for this is because we don't want visible segments to be "skipped" into
     * connecting with invisible segments. */
    if (eci->occlusion < fixed_occ) {
      break;
    }
    if (eci->material_mask_bits == fixed_mask && eci->occlusion == fixed_occ &&
        eci->shadow_mask_bits == fixed_shadow)
    {
      can_skip_to = eci;
    }
  }
  if (can_skip_to) {
    /* Either mark all in-between segments with the same occlusion and mask or delete those
     * different ones. */
    LineartEdgeChainItem *next_eci;
    for (LineartEdgeChainItem *eci = last_matching_eci->next; eci != can_skip_to; eci = next_eci) {
      next_eci = eci->next;
      if (eci->material_mask_bits == fixed_mask && eci->occlusion == fixed_occ &&
          eci->shadow_mask_bits == fixed_shadow)
      {
        continue;
      }
      if (preserve_details) {
        eci->material_mask_bits = fixed_mask;
        eci->occlusion = fixed_occ;
        eci->shadow_mask_bits = fixed_shadow;
      }
      else {
        BLI_remlink(&ec->chain, eci);
      }
    }
    *r_next_eci = can_skip_to;
    return true;
  }
  return false;
}

void MOD_lineart_chain_split_for_fixed_occlusion(LineartData *ld)
{
  LineartEdgeChainItem *eci, *next_eci;
  ListBase swap = {nullptr};

  swap.first = ld->chains.first;
  swap.last = ld->chains.last;

  ld->chains.last = ld->chains.first = nullptr;

  int loop_id = 0;
  while (LineartEdgeChain *ec = static_cast<LineartEdgeChain *>(BLI_pophead(&swap))) {
    ec->next = ec->prev = nullptr;
    BLI_addtail(&ld->chains, ec);

    ec->loop_id = loop_id;
    loop_id++;

    LineartEdgeChainItem *first_eci = (LineartEdgeChainItem *)ec->chain.first;
    int fixed_occ = first_eci->occlusion;
    uint8_t fixed_mask = first_eci->material_mask_bits;
    uint32_t fixed_shadow = first_eci->shadow_mask_bits;
    ec->level = fixed_occ;
    ec->material_mask_bits = fixed_mask;
    ec->shadow_mask_bits = fixed_shadow;
    for (eci = first_eci->next; eci; eci = next_eci) {
      next_eci = eci->next;
      if (eci->occlusion != fixed_occ || eci->material_mask_bits != fixed_mask ||
          eci->shadow_mask_bits != fixed_shadow)
      {
        if (next_eci) {
          if (lineart_point_overlapping(next_eci, eci->pos[0], eci->pos[1], 1e-5)) {
            continue;
          }
          if (lineart_chain_fix_ambiguous_segments(ec,
                                                   eci->prev,
                                                   ld->conf.chaining_image_threshold,
                                                   ld->conf.chain_preserve_details,
                                                   &next_eci))
          {
            continue;
          }
        }
        else {
          /* Set the same occlusion level for the end vertex, so when further connection is needed
           * the backwards occlusion info is also correct. */
          eci->occlusion = fixed_occ;
          eci->shadow_mask_bits = fixed_shadow;
          eci->material_mask_bits = fixed_mask;
          /* No need to split at the last point anyway. */
          break;
        }
        LineartEdgeChain *new_ec = lineart_chain_create(ld);
        new_ec->chain.first = eci;
        new_ec->chain.last = ec->chain.last;
        new_ec->loop_id = loop_id;
        ec->chain.last = eci->prev;
        ((LineartEdgeChainItem *)ec->chain.last)->next = nullptr;
        eci->prev = nullptr;

        /* End the previous one. */
        lineart_chain_append_point(ld,
                                   ec,
                                   eci->pos,
                                   eci->gpos,
                                   eci->normal,
                                   eci->line_type,
                                   fixed_occ,
                                   fixed_mask,
                                   fixed_shadow,
                                   eci->index);
        new_ec->object_ref = ec->object_ref;
        new_ec->type = ec->type;
        new_ec->intersection_mask = ec->intersection_mask;
        ec = new_ec;
        fixed_occ = eci->occlusion;
        fixed_mask = eci->material_mask_bits;
        fixed_shadow = eci->shadow_mask_bits;
        ec->level = fixed_occ;
        ec->material_mask_bits = fixed_mask;
        ec->shadow_mask_bits = fixed_shadow;
      }
    }
  }

  MOD_lineart_chain_discard_unused(ld, DBL_EDGE_LIM, ld->conf.max_occlusion_level);

  LISTBASE_FOREACH (LineartEdgeChain *, iec, &ld->chains) {
    lineart_bounding_area_link_chain(ld, iec);
  }
}

/**
 * NOTE: segment type (crease/material/contour...) is ambiguous after this.
 */
static void lineart_chain_connect(LineartData * /*ld*/,
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
    eci = static_cast<LineartEdgeChainItem *>(sub->chain.first);
    if (lineart_point_overlapping(
            static_cast<LineartEdgeChainItem *>(onto->chain.last), eci->pos[0], eci->pos[1], 1e-5))
    {
      BLI_pophead(&sub->chain);
      if (sub->chain.first == nullptr) {
        return;
      }
    }
    ((LineartEdgeChainItem *)onto->chain.last)->next = static_cast<LineartEdgeChainItem *>(
        sub->chain.first);
    ((LineartEdgeChainItem *)sub->chain.first)->prev = static_cast<LineartEdgeChainItem *>(
        onto->chain.last);
    onto->chain.last = sub->chain.last;
  }
  else {              /*  L-R L--R. */
    if (!reverse_2) { /*  R-L L--R. */
      BLI_listbase_reverse(&sub->chain);
    }
    eci = static_cast<LineartEdgeChainItem *>(onto->chain.first);
    if (lineart_point_overlapping(
            static_cast<LineartEdgeChainItem *>(sub->chain.last), eci->pos[0], eci->pos[1], 1e-5))
    {
      BLI_pophead(&onto->chain);
      if (onto->chain.first == nullptr) {
        return;
      }
    }
    ((LineartEdgeChainItem *)sub->chain.last)->next = static_cast<LineartEdgeChainItem *>(
        onto->chain.first);
    ((LineartEdgeChainItem *)onto->chain.first)->prev = static_cast<LineartEdgeChainItem *>(
        sub->chain.last);
    onto->chain.first = sub->chain.first;
  }
}

static LineartChainRegisterEntry *lineart_chain_get_closest_cre(LineartData *ld,
                                                                LineartBoundingArea *ba,
                                                                LineartEdgeChain *ec,
                                                                LineartEdgeChainItem *eci,
                                                                int occlusion,
                                                                uint8_t material_mask_bits,
                                                                uint8_t isec_mask,
                                                                uint32_t shadow_mask,
                                                                int loop_id,
                                                                float dist,
                                                                float *result_new_len,
                                                                LineartBoundingArea *caller_ba)
{

  LineartChainRegisterEntry *closest_cre = nullptr;

  /* Keep using for loop because `cre` could be removed from the iteration before getting to the
   * next one. */
  LISTBASE_FOREACH_MUTABLE (LineartChainRegisterEntry *, cre, &ba->linked_chains) {
    if (cre->ec->object_ref != ec->object_ref) {
      if (!ld->conf.fuzzy_everything) {
        if (ld->conf.fuzzy_intersections) {
          /* If none of those are intersection lines... */
          if (!(cre->ec->type & LRT_EDGE_FLAG_INTERSECTION) &&
              !(ec->type & LRT_EDGE_FLAG_INTERSECTION)) {
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
        (cre->ec->material_mask_bits != material_mask_bits) ||
        (cre->ec->intersection_mask != isec_mask) || (cre->ec->shadow_mask_bits != shadow_mask))
    {
      continue;
    }
    if (!ld->conf.fuzzy_everything) {
      if (cre->ec->type != ec->type) {
        if (ld->conf.fuzzy_intersections) {
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

    float new_len = ld->conf.use_geometry_space_chain ? len_v3v3(cre->eci->gpos, eci->gpos) :
                                                        len_v2v2(cre->eci->pos, eci->pos);
    /* Even if the vertex is not from the same contour loop, we try to chain it still if the
     * distance is small enough. This way we can better chain smaller loops and smooth them out
     * later. */
    if (((cre->ec->loop_id == loop_id) && (new_len < dist)) ||
        ((cre->ec->loop_id != loop_id) && (new_len < dist / 10)))
    {
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
    LISTBASE_FOREACH (LinkData *, link, list) { \
      LineartBoundingArea *sba = (LineartBoundingArea *)link->data; \
      adjacent_closest = lineart_chain_get_closest_cre(ld, \
                                                       sba, \
                                                       ec, \
                                                       eci, \
                                                       occlusion, \
                                                       material_mask_bits, \
                                                       isec_mask, \
                                                       shadow_mask, \
                                                       loop_id, \
                                                       dist, \
                                                       &adjacent_new_len, \
                                                       ba); \
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

void MOD_lineart_chain_connect(LineartData *ld)
{
  LineartEdgeChainItem *eci_l, *eci_r;
  LineartBoundingArea *ba_l, *ba_r;
  LineartChainRegisterEntry *closest_cre_l, *closest_cre_r, *closest_cre;
  float dist = ld->conf.chaining_image_threshold;
  float dist_l, dist_r;
  int reverse_main, loop_id;
  uint8_t occlusion, material_mask_bits, isec_mask;
  uint32_t shadow_mask;
  ListBase swap = {nullptr};

  if (ld->conf.chaining_image_threshold < 0.0001) {
    return;
  }

  swap.first = ld->chains.first;
  swap.last = ld->chains.last;

  ld->chains.last = ld->chains.first = nullptr;

  while (LineartEdgeChain *ec = static_cast<LineartEdgeChain *>(BLI_pophead(&swap))) {
    ec->next = ec->prev = nullptr;
    if (ec->picked || ec->chain.first == ec->chain.last) {
      continue;
    }
    BLI_addtail(&ld->chains, ec);
    loop_id = ec->loop_id;

    if (ec->type == LRT_EDGE_FLAG_LOOSE && (!ld->conf.use_loose_edge_chain)) {
      continue;
    }

    occlusion = ec->level;
    material_mask_bits = ec->material_mask_bits;
    isec_mask = ec->intersection_mask;
    shadow_mask = ec->shadow_mask_bits;

    eci_l = static_cast<LineartEdgeChainItem *>(ec->chain.first);
    eci_r = static_cast<LineartEdgeChainItem *>(ec->chain.last);
    while ((ba_l = lineart_bounding_area_get_end_point(ld, eci_l)) &&
           (ba_r = lineart_bounding_area_get_end_point(ld, eci_r)))
    {
      closest_cre_l = lineart_chain_get_closest_cre(ld,
                                                    ba_l,
                                                    ec,
                                                    eci_l,
                                                    occlusion,
                                                    material_mask_bits,
                                                    isec_mask,
                                                    shadow_mask,
                                                    loop_id,
                                                    dist,
                                                    &dist_l,
                                                    nullptr);
      closest_cre_r = lineart_chain_get_closest_cre(ld,
                                                    ba_r,
                                                    ec,
                                                    eci_r,
                                                    occlusion,
                                                    material_mask_bits,
                                                    isec_mask,
                                                    shadow_mask,
                                                    loop_id,
                                                    dist,
                                                    &dist_r,
                                                    nullptr);
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
        lineart_chain_connect(ld, ec, closest_cre->ec, reverse_main, 0);
      }
      else {
        lineart_chain_connect(ld, ec, closest_cre->ec, reverse_main, 1);
      }
      BLI_remlink(&swap, closest_cre->ec);
      eci_l = static_cast<LineartEdgeChainItem *>(ec->chain.first);
      eci_r = static_cast<LineartEdgeChainItem *>(ec->chain.last);
    }
    ec->picked = 1;
  }
}

float MOD_lineart_chain_compute_length(LineartEdgeChain *ec)
{
  LineartEdgeChainItem *eci;
  float offset_accum = 0;
  float dist;
  float last_point[2];

  eci = static_cast<LineartEdgeChainItem *>(ec->chain.first);
  if (!eci) {
    return 0;
  }
  copy_v2_v2(last_point, eci->pos);
  LISTBASE_FOREACH (LineartEdgeChainItem *, eci, &ec->chain) {
    dist = len_v2v2(eci->pos, last_point);
    offset_accum += dist;
    copy_v2_v2(last_point, eci->pos);
  }
  return offset_accum;
}

void MOD_lineart_chain_discard_unused(LineartData *ld,
                                      const float threshold,
                                      uint8_t max_occlusion)
{
  LineartEdgeChain *ec, *next_ec;
  for (ec = static_cast<LineartEdgeChain *>(ld->chains.first); ec; ec = next_ec) {
    next_ec = ec->next;
    if (ec->level > max_occlusion || MOD_lineart_chain_compute_length(ec) < threshold) {
      BLI_remlink(&ld->chains, ec);
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

void MOD_lineart_chain_clear_picked_flag(LineartCache *lc)
{
  if (lc == nullptr) {
    return;
  }
  LISTBASE_FOREACH (LineartEdgeChain *, ec, &lc->chains) {
    ec->picked = 0;
  }
}

LineartElementLinkNode *lineart_find_matching_eln_obj(ListBase *elns, Object *ob)
{
  LISTBASE_FOREACH (LineartElementLinkNode *, eln, elns) {
    if (eln->object_ref == ob) {
      return eln;
    }
  }
  return nullptr;
}

void MOD_lineart_finalize_chains(LineartData *ld)
{
  LISTBASE_FOREACH (LineartEdgeChain *, ec, &ld->chains) {
    if (ELEM(ec->type,
             LRT_EDGE_FLAG_INTERSECTION,
             LRT_EDGE_FLAG_PROJECTED_SHADOW,
             LRT_EDGE_FLAG_LIGHT_CONTOUR))
    {
      continue;
    }
    LineartElementLinkNode *eln = lineart_find_matching_eln_obj(&ld->geom.vertex_buffer_pointers,
                                                                ec->object_ref);
    BLI_assert(eln != nullptr);
    if (LIKELY(eln)) {
      LISTBASE_FOREACH (LineartEdgeChainItem *, eci, &ec->chain) {
        if (eci->index > eln->global_index_offset) {
          eci->index -= eln->global_index_offset;
        }
      }
    }
  }
}

void MOD_lineart_smooth_chains(LineartData *ld, float tolerance)
{
  LISTBASE_FOREACH (LineartEdgeChain *, ec, &ld->chains) {
    /* Go through the chain two times, once from each direction. */
    for (int times = 0; times < 2; times++) {
      for (LineartEdgeChainItem *eci = static_cast<LineartEdgeChainItem *>(ec->chain.first),
                                *next_eci = eci->next;
           eci;
           eci = next_eci)
      {
        LineartEdgeChainItem *eci2, *eci3, *eci4;

        if (!(eci2 = eci->next) || !(eci3 = eci2->next)) {
          /* Not enough points to simplify. */
          next_eci = eci->next;
          continue;
        }
        /* No need to care for different line types/occlusion and so on, because at this stage they
         * are all the same within a chain.
         *
         * We need to simplify a chain from this:
         * 1-----------2
         *        3-----------4
         * to this:
         * 1-----------2--_
         *                 `--4 */

        /* If p3 is within the p1-p2 segment of a width of "tolerance", in other words, p3 is
         * approximately on the segment of p1-p2. */
        if (dist_to_line_segment_v2(eci3->pos, eci->pos, eci2->pos) < tolerance) {
          float vec2[2], vec3[2], v2n[2], ratio, len2;
          sub_v2_v2v2(vec2, eci2->pos, eci->pos);
          sub_v2_v2v2(vec3, eci3->pos, eci->pos);
          normalize_v2_v2(v2n, vec2);
          ratio = dot_v2v2(v2n, vec3);
          len2 = len_v2(vec2);
          /* Because this smoothing applies on geometries of different scales in the same scene,
           * some small scale features (e.g. the "tails" on the inner ring of a torus geometry)
           * could be completely erased if the tolerance value is set for accommodating the entire
           * scene. Those situations typically result in (ratio << 0), looks like this:
           *                         1---2
           * 3-------------------------------4
           * (this sort of long zigzag obviously are "features" that can't be erased)
           * setting a ratio of -10 turned out to be a reasonable threshold in tests. */
          if (ratio < len2 && ratio > -len2 * 10) {
            /* We only remove p3 if p4 is on the extension of p1->p2. */
            if ((eci4 = eci3->next) &&
                (dist_to_line_v2(eci4->pos, eci->pos, eci2->pos) < tolerance)) {
              BLI_remlink(&ec->chain, eci3);
              next_eci = eci;
              continue;
            }
            if (!eci4) {
              /* See if the last segment's direction is reversed, if so remove that.
               * Basically we don't need to preserve p3 if the entire chain looked like this:
               * ...----1----3===2 */
              if (len_v2(vec2) > len_v2(vec3)) {
                BLI_remlink(&ec->chain, eci3);
              }
              next_eci = nullptr;
              continue;
            }
          }
        }
        next_eci = eci->next;
      }
      BLI_listbase_reverse(&ec->chain);
    }
  }
}

static LineartEdgeChainItem *lineart_chain_create_crossing_point(LineartData *ld,
                                                                 LineartEdgeChainItem *eci_inside,
                                                                 LineartEdgeChainItem *eci_outside)
{
  float isec[2];
  /* l: left, r: right, b: bottom, u: top. */
  float ref_lu[2] = {-1.0f, 1.0f}, ref_lb[2] = {-1.0f, -1.0f}, ref_ru[2] = {1.0f, 1.0f},
        ref_rb[2] = {1.0f, -1.0f};
  bool found = false;
  LineartEdgeChainItem *eci2 = eci_outside, *eci1 = eci_inside;
  if (eci2->pos[0] < -1.0f) {
    found = (isect_seg_seg_v2_point(eci1->pos, eci2->pos, ref_lu, ref_lb, isec) > 0);
  }
  if (!found && eci2->pos[0] > 1.0f) {
    found = (isect_seg_seg_v2_point(eci1->pos, eci2->pos, ref_ru, ref_rb, isec) > 0);
  }
  if (!found && eci2->pos[1] < -1.0f) {
    found = (isect_seg_seg_v2_point(eci1->pos, eci2->pos, ref_lb, ref_rb, isec) > 0);
  }
  if (!found && eci2->pos[1] > 1.0f) {
    found = (isect_seg_seg_v2_point(eci1->pos, eci2->pos, ref_lu, ref_ru, isec) > 0);
  }

  if (UNLIKELY(!found)) {
    return nullptr;
  }

  float ratio = (fabs(eci2->pos[0] - eci1->pos[0]) > fabs(eci2->pos[1] - eci1->pos[1])) ?
                    ratiof(eci1->pos[0], eci2->pos[0], isec[0]) :
                    ratiof(eci1->pos[1], eci2->pos[1], isec[1]);
  float gratio = eci1->pos[3] * ratio / (ratio * eci1->pos[3] + (1 - ratio) * eci2->pos[3]);

  LineartEdgeChainItem *eci = static_cast<LineartEdgeChainItem *>(
      lineart_mem_acquire(ld->chain_data_pool, sizeof(LineartEdgeChainItem)));
  memcpy(eci, eci1, sizeof(LineartEdgeChainItem));
  interp_v3_v3v3(eci->gpos, eci1->gpos, eci2->gpos, gratio);
  interp_v3_v3v3(eci->pos, eci1->pos, eci2->pos, ratio);
  eci->pos[3] = interpf(eci2->pos[3], eci1->pos[3], gratio);
  eci->next = eci->prev = nullptr;
  return eci;
}

#define LRT_ECI_INSIDE(eci) \
  ((eci)->pos[0] >= -1.0f && (eci)->pos[0] <= 1.0f && (eci)->pos[1] >= -1.0f && \
   (eci)->pos[1] <= 1.0f)

void MOD_lineart_chain_clip_at_border(LineartData *ld)
{
  LineartEdgeChainItem *eci, *next_eci, *prev_eci, *new_eci;
  bool is_inside, new_inside;
  ListBase swap = {nullptr};
  swap.first = ld->chains.first;
  swap.last = ld->chains.last;

  ld->chains.last = ld->chains.first = nullptr;
  while (LineartEdgeChain *ec = static_cast<LineartEdgeChain *>(BLI_pophead(&swap))) {
    bool ec_added = false;
    LineartEdgeChainItem *first_eci = (LineartEdgeChainItem *)ec->chain.first;
    is_inside = LRT_ECI_INSIDE(first_eci) ? true : false;
    if (!is_inside) {
      ec->picked = 1;
    }
    for (eci = first_eci->next; eci; eci = next_eci) {
      next_eci = eci->next;
      prev_eci = eci->prev;

      /* We only need to do something if the edge crossed from outside to the inside or from inside
       * to the outside. */
      if ((new_inside = LRT_ECI_INSIDE(eci)) != is_inside) {
        if (new_inside == false) {
          /* Stroke goes out. */
          new_eci = lineart_chain_create_crossing_point(ld, prev_eci, eci);

          LineartEdgeChain *new_ec = static_cast<LineartEdgeChain *>(
              lineart_mem_acquire(ld->chain_data_pool, sizeof(LineartEdgeChain)));
          memcpy(new_ec, ec, sizeof(LineartEdgeChain));
          new_ec->chain.first = next_eci;
          eci->prev = nullptr;
          prev_eci->next = nullptr;
          ec->chain.last = prev_eci;
          BLI_addtail(&ec->chain, new_eci);
          BLI_addtail(&ld->chains, ec);
          ec_added = true;
          ec = new_ec;

          next_eci = eci->next;
          is_inside = new_inside;
          continue;
        }
        /* Stroke comes in. */
        new_eci = lineart_chain_create_crossing_point(ld, eci, prev_eci);

        ec->chain.first = eci;
        eci->prev = nullptr;

        BLI_addhead(&ec->chain, new_eci);

        ec_added = false;

        next_eci = eci->next;
        is_inside = new_inside;
        continue;
      }
    }

    if ((!ec_added) && is_inside) {
      BLI_addtail(&ld->chains, ec);
    }
  }
}

void MOD_lineart_chain_split_angle(LineartData *ld, float angle_threshold_rad)
{
  LineartEdgeChainItem *eci, *next_eci, *prev_eci;
  ListBase swap = {nullptr};

  swap.first = ld->chains.first;
  swap.last = ld->chains.last;

  ld->chains.last = ld->chains.first = nullptr;

  while (LineartEdgeChain *ec = static_cast<LineartEdgeChain *>(BLI_pophead(&swap))) {
    ec->next = ec->prev = nullptr;
    BLI_addtail(&ld->chains, ec);
    LineartEdgeChainItem *first_eci = (LineartEdgeChainItem *)ec->chain.first;
    for (eci = first_eci->next; eci; eci = next_eci) {
      next_eci = eci->next;
      prev_eci = eci->prev;
      float angle = M_PI;
      if (next_eci && prev_eci) {
        angle = angle_v2v2v2(prev_eci->pos, eci->pos, next_eci->pos);
      }
      else {
        break; /* No need to split at the last point anyway. */
      }
      if (angle < angle_threshold_rad) {
        LineartEdgeChain *new_ec;
        new_ec = lineart_chain_create(ld);
        new_ec->chain.first = eci;
        new_ec->chain.last = ec->chain.last;
        ec->chain.last = eci->prev;
        ((LineartEdgeChainItem *)ec->chain.last)->next = nullptr;
        eci->prev = nullptr;

        /* End the previous one. */
        lineart_chain_append_point(ld,
                                   ec,
                                   eci->pos,
                                   eci->gpos,
                                   eci->normal,
                                   eci->line_type,
                                   ec->level,
                                   eci->material_mask_bits,
                                   eci->shadow_mask_bits,
                                   eci->index);
        new_ec->object_ref = ec->object_ref;
        new_ec->type = ec->type;
        new_ec->level = ec->level;
        new_ec->loop_id = ec->loop_id;
        new_ec->intersection_mask = ec->intersection_mask;
        new_ec->material_mask_bits = ec->material_mask_bits;
        new_ec->shadow_mask_bits = ec->shadow_mask_bits;
        ec = new_ec;
      }
    }
  }
}

void MOD_lineart_chain_offset_towards_camera(LineartData *ld, float dist, bool use_custom_camera)
{
  float dir[3];
  float cam[3];
  float view[3];
  float view_clamp[3];

  if (use_custom_camera) {
    copy_v3fl_v3db(cam, ld->conf.camera_pos);
  }
  else {
    copy_v3fl_v3db(cam, ld->conf.active_camera_pos);
  }

  if (ld->conf.cam_is_persp) {
    LISTBASE_FOREACH (LineartEdgeChain *, ec, &ld->chains) {
      LISTBASE_FOREACH (LineartEdgeChainItem *, eci, &ec->chain) {
        sub_v3_v3v3(dir, cam, eci->gpos);
        float orig_len = len_v3(dir);
        normalize_v3(dir);
        mul_v3_fl(dir, MIN2(dist, orig_len - ld->conf.near_clip));
        add_v3_v3(eci->gpos, dir);
      }
    }
  }
  else {
    copy_v3fl_v3db(view, ld->conf.view_vector);
    LISTBASE_FOREACH (LineartEdgeChain *, ec, &ld->chains) {
      LISTBASE_FOREACH (LineartEdgeChainItem *, eci, &ec->chain) {
        sub_v3_v3v3(dir, cam, eci->gpos);
        float len_lim = dot_v3v3(view, dir) - ld->conf.near_clip;
        normalize_v3_v3(view_clamp, view);
        mul_v3_fl(view_clamp, MIN2(dist, len_lim));
        add_v3_v3(eci->gpos, view_clamp);
      }
    }
  }
}

void MOD_lineart_chain_find_silhouette_backdrop_objects(LineartData *ld)
{
  LISTBASE_FOREACH (LineartEdgeChain *, ec, &ld->chains) {
    if (ec->type == LRT_EDGE_FLAG_CONTOUR &&
        ec->shadow_mask_bits & LRT_SHADOW_SILHOUETTE_ERASED_GROUP) {
      uint32_t target = ec->shadow_mask_bits & LRT_OBINDEX_HIGHER;
      LineartElementLinkNode *eln = lineart_find_matching_eln(&ld->geom.line_buffer_pointers,
                                                              target);
      if (!eln) {
        continue;
      }
      ec->silhouette_backdrop = static_cast<Object *>(eln->object_ref);
    }
  }
}
