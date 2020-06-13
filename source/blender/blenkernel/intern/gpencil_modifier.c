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
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup bke
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lattice.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_material.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_modifiertypes.h"

static GpencilModifierTypeInfo *modifier_gpencil_types[NUM_GREASEPENCIL_MODIFIER_TYPES] = {NULL};

/* *************************************************** */
/* Geometry Utilities */

/* calculate stroke normal using some points */
void BKE_gpencil_stroke_normal(const bGPDstroke *gps, float r_normal[3])
{
  if (gps->totpoints < 3) {
    zero_v3(r_normal);
    return;
  }

  bGPDspoint *points = gps->points;
  int totpoints = gps->totpoints;

  const bGPDspoint *pt0 = &points[0];
  const bGPDspoint *pt1 = &points[1];
  const bGPDspoint *pt3 = &points[(int)(totpoints * 0.75)];

  float vec1[3];
  float vec2[3];

  /* initial vector (p0 -> p1) */
  sub_v3_v3v3(vec1, &pt1->x, &pt0->x);

  /* point vector at 3/4 */
  sub_v3_v3v3(vec2, &pt3->x, &pt0->x);

  /* vector orthogonal to polygon plane */
  cross_v3_v3v3(r_normal, vec1, vec2);

  /* Normalize vector */
  normalize_v3(r_normal);
}

/* Stroke Simplify ------------------------------------- */

/* Reduce a series of points to a simplified version, but
 * maintains the general shape of the series
 *
 * Ramer - Douglas - Peucker algorithm
 * by http ://en.wikipedia.org/wiki/Ramer-Douglas-Peucker_algorithm
 */
void BKE_gpencil_stroke_simplify_adaptive(bGPDstroke *gps, float epsilon)
{
  bGPDspoint *old_points = MEM_dupallocN(gps->points);
  int totpoints = gps->totpoints;
  char *marked = NULL;
  char work;

  int start = 0;
  int end = gps->totpoints - 1;

  marked = MEM_callocN(totpoints, "GP marked array");
  marked[start] = 1;
  marked[end] = 1;

  work = 1;
  int totmarked = 0;
  /* while still reducing */
  while (work) {
    int ls, le;
    work = 0;

    ls = start;
    le = start + 1;

    /* while not over interval */
    while (ls < end) {
      int max_i = 0;
      /* divided to get more control */
      float max_dist = epsilon / 10.0f;

      /* find the next marked point */
      while (marked[le] == 0) {
        le++;
      }

      for (int i = ls + 1; i < le; i++) {
        float point_on_line[3];
        float dist;

        closest_to_line_segment_v3(
            point_on_line, &old_points[i].x, &old_points[ls].x, &old_points[le].x);

        dist = len_v3v3(point_on_line, &old_points[i].x);

        if (dist > max_dist) {
          max_dist = dist;
          max_i = i;
        }
      }

      if (max_i != 0) {
        work = 1;
        marked[max_i] = 1;
        totmarked++;
      }

      ls = le;
      le = ls + 1;
    }
  }

  /* adding points marked */
  MDeformVert *old_dvert = NULL;
  MDeformVert *dvert_src = NULL;

  if (gps->dvert != NULL) {
    old_dvert = MEM_dupallocN(gps->dvert);
  }
  /* resize gps */
  int j = 0;
  for (int i = 0; i < totpoints; i++) {
    bGPDspoint *pt_src = &old_points[i];
    bGPDspoint *pt = &gps->points[j];

    if ((marked[i]) || (i == 0) || (i == totpoints - 1)) {
      memcpy(pt, pt_src, sizeof(bGPDspoint));
      if (gps->dvert != NULL) {
        dvert_src = &old_dvert[i];
        MDeformVert *dvert = &gps->dvert[j];
        memcpy(dvert, dvert_src, sizeof(MDeformVert));
        if (dvert_src->dw) {
          memcpy(dvert->dw, dvert_src->dw, sizeof(MDeformWeight));
        }
      }
      j++;
    }
    else {
      if (gps->dvert != NULL) {
        dvert_src = &old_dvert[i];
        BKE_gpencil_free_point_weights(dvert_src);
      }
    }
  }

  gps->totpoints = j;

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gps);

  MEM_SAFE_FREE(old_points);
  MEM_SAFE_FREE(old_dvert);
  MEM_SAFE_FREE(marked);
}

/* Simplify alternate vertex of stroke except extremes */
void BKE_gpencil_stroke_simplify_fixed(bGPDstroke *gps)
{
  if (gps->totpoints < 5) {
    return;
  }

  /* save points */
  bGPDspoint *old_points = MEM_dupallocN(gps->points);
  MDeformVert *old_dvert = NULL;
  MDeformVert *dvert_src = NULL;

  if (gps->dvert != NULL) {
    old_dvert = MEM_dupallocN(gps->dvert);
  }

  /* resize gps */
  int newtot = (gps->totpoints - 2) / 2;
  if (((gps->totpoints - 2) % 2) > 0) {
    newtot++;
  }
  newtot += 2;

  gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * newtot);
  if (gps->dvert != NULL) {
    gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * newtot);
  }

  int j = 0;
  for (int i = 0; i < gps->totpoints; i++) {
    bGPDspoint *pt_src = &old_points[i];
    bGPDspoint *pt = &gps->points[j];

    if ((i == 0) || (i == gps->totpoints - 1) || ((i % 2) > 0.0)) {
      memcpy(pt, pt_src, sizeof(bGPDspoint));
      if (gps->dvert != NULL) {
        dvert_src = &old_dvert[i];
        MDeformVert *dvert = &gps->dvert[j];
        memcpy(dvert, dvert_src, sizeof(MDeformVert));
        if (dvert_src->dw) {
          memcpy(dvert->dw, dvert_src->dw, sizeof(MDeformWeight));
        }
      }
      j++;
    }
    else {
      if (gps->dvert != NULL) {
        dvert_src = &old_dvert[i];
        BKE_gpencil_free_point_weights(dvert_src);
      }
    }
  }

  gps->totpoints = j;
  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gps);

  MEM_SAFE_FREE(old_points);
  MEM_SAFE_FREE(old_dvert);
}

/* *************************************************** */
/* Modifier Utilities */

/* Lattice Modifier ---------------------------------- */
/* Usually, evaluation of the lattice modifier is self-contained.
 * However, since GP's modifiers operate on a per-stroke basis,
 * we need to these two extra functions that called before/after
 * each loop over all the geometry being evaluated.
 */

/* init lattice deform data */
void BKE_gpencil_lattice_init(Object *ob)
{
  GpencilModifierData *md;
  for (md = ob->greasepencil_modifiers.first; md; md = md->next) {
    if (md->type == eGpencilModifierType_Lattice) {
      LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
      Object *latob = NULL;

      latob = mmd->object;
      if ((!latob) || (latob->type != OB_LATTICE)) {
        return;
      }
      if (mmd->cache_data) {
        BKE_lattice_deform_data_destroy((struct LatticeDeformData *)mmd->cache_data);
      }

      /* init deform data */
      mmd->cache_data = (struct LatticeDeformData *)BKE_lattice_deform_data_create(latob, ob);
    }
  }
}

/* clear lattice deform data */
void BKE_gpencil_lattice_clear(Object *ob)
{
  GpencilModifierData *md;
  for (md = ob->greasepencil_modifiers.first; md; md = md->next) {
    if (md->type == eGpencilModifierType_Lattice) {
      LatticeGpencilModifierData *mmd = (LatticeGpencilModifierData *)md;
      if ((mmd) && (mmd->cache_data)) {
        BKE_lattice_deform_data_destroy((struct LatticeDeformData *)mmd->cache_data);
        mmd->cache_data = NULL;
      }
    }
  }
}

/* *************************************************** */
/* Modifier Methods - Evaluation Loops, etc. */

/* check if exist geometry modifiers */
bool BKE_gpencil_has_geometry_modifiers(Object *ob)
{
  GpencilModifierData *md;
  for (md = ob->greasepencil_modifiers.first; md; md = md->next) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

    if (mti && mti->generateStrokes) {
      return true;
    }
  }
  return false;
}

/* check if exist time modifiers */
bool BKE_gpencil_has_time_modifiers(Object *ob)
{
  GpencilModifierData *md;
  for (md = ob->greasepencil_modifiers.first; md; md = md->next) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

    if (mti && mti->remapTime) {
      return true;
    }
  }
  return false;
}

/* Check if exist transform stroke modifiers (to rotate sculpt or edit). */
bool BKE_gpencil_has_transform_modifiers(Object *ob)
{
  GpencilModifierData *md;
  for (md = ob->greasepencil_modifiers.first; md; md = md->next) {
    /* Only if enabled in edit mode. */
    if (!GPENCIL_MODIFIER_EDIT(md, true) && GPENCIL_MODIFIER_ACTIVE(md, false)) {
      if ((md->type == eGpencilModifierType_Armature) || (md->type == eGpencilModifierType_Hook) ||
          (md->type == eGpencilModifierType_Lattice) ||
          (md->type == eGpencilModifierType_Offset)) {
        return true;
      }
    }
  }
  return false;
}

/* apply time modifiers */
static int gpencil_time_modifier(
    Depsgraph *depsgraph, Scene *scene, Object *ob, bGPDlayer *gpl, int cfra, bool is_render)
{
  GpencilModifierData *md;
  bGPdata *gpd = ob->data;
  const bool is_edit = GPENCIL_ANY_EDIT_MODE(gpd);
  int nfra = cfra;

  for (md = ob->greasepencil_modifiers.first; md; md = md->next) {
    if (GPENCIL_MODIFIER_ACTIVE(md, is_render)) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

      if ((GPENCIL_MODIFIER_EDIT(md, is_edit)) && (!is_render)) {
        continue;
      }

      if (mti->remapTime) {
        nfra = mti->remapTime(md, depsgraph, scene, ob, gpl, cfra);
        /* if the frame number changed, don't evaluate more and return */
        if (nfra != cfra) {
          return nfra;
        }
      }
    }
  }

  /* if no time modifier, return original frame number */
  return nfra;
}
/* *************************************************** */

void BKE_gpencil_frame_active_set(Depsgraph *depsgraph, bGPdata *gpd)
{
  DEG_debug_print_eval(depsgraph, __func__, gpd->id.name, gpd);
  int ctime = (int)DEG_get_ctime(depsgraph);

  /* update active frame */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    gpl->actframe = BKE_gpencil_layer_frame_get(gpl, ctime, GP_GETFRAME_USE_PREV);
  }

  if (DEG_is_active(depsgraph)) {
    bGPdata *gpd_orig = (bGPdata *)DEG_get_original_id(&gpd->id);

    /* sync "actframe" changes back to main-db too,
     * so that editing tools work with copy-on-write
     * when the current frame changes
     */
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_orig->layers) {
      gpl->actframe = BKE_gpencil_layer_frame_get(gpl, ctime, GP_GETFRAME_USE_PREV);
    }
  }
}

void BKE_gpencil_modifier_init(void)
{
  /* Initialize modifier types */
  gpencil_modifier_type_init(modifier_gpencil_types); /* MOD_gpencil_util.c */
}

GpencilModifierData *BKE_gpencil_modifier_new(int type)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(type);
  GpencilModifierData *md = MEM_callocN(mti->struct_size, mti->struct_name);

  /* note, this name must be made unique later */
  BLI_strncpy(md->name, DATA_(mti->name), sizeof(md->name));

  md->type = type;
  md->mode = eGpencilModifierMode_Realtime | eGpencilModifierMode_Render |
             eGpencilModifierMode_Expanded;
  md->flag = eGpencilModifierFlag_OverrideLibrary_Local;

  if (mti->flags & eGpencilModifierTypeFlag_EnableInEditmode) {
    md->mode |= eGpencilModifierMode_Editmode;
  }

  if (mti->initData) {
    mti->initData(md);
  }

  return md;
}

static void modifier_free_data_id_us_cb(void *UNUSED(userData),
                                        Object *UNUSED(ob),
                                        ID **idpoin,
                                        int cb_flag)
{
  ID *id = *idpoin;
  if (id != NULL && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_min(id);
  }
}

void BKE_gpencil_modifier_free_ex(GpencilModifierData *md, const int flag)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (mti->foreachIDLink) {
      mti->foreachIDLink(md, NULL, modifier_free_data_id_us_cb, NULL);
    }
    else if (mti->foreachObjectLink) {
      mti->foreachObjectLink(
          md, NULL, (GreasePencilObjectWalkFunc)modifier_free_data_id_us_cb, NULL);
    }
  }

  if (mti->freeData) {
    mti->freeData(md);
  }
  if (md->error) {
    MEM_freeN(md->error);
  }

  MEM_freeN(md);
}

void BKE_gpencil_modifier_free(GpencilModifierData *md)
{
  BKE_gpencil_modifier_free_ex(md, 0);
}

/* check unique name */
bool BKE_gpencil_modifier_unique_name(ListBase *modifiers, GpencilModifierData *gmd)
{
  if (modifiers && gmd) {
    const GpencilModifierTypeInfo *gmti = BKE_gpencil_modifier_get_info(gmd->type);
    return BLI_uniquename(modifiers,
                          gmd,
                          DATA_(gmti->name),
                          '.',
                          offsetof(GpencilModifierData, name),
                          sizeof(gmd->name));
  }
  return false;
}

bool BKE_gpencil_modifier_depends_ontime(GpencilModifierData *md)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

  return mti->dependsOnTime && mti->dependsOnTime(md);
}

const GpencilModifierTypeInfo *BKE_gpencil_modifier_get_info(GpencilModifierType type)
{
  /* type unsigned, no need to check < 0 */
  if (type < NUM_GREASEPENCIL_MODIFIER_TYPES && modifier_gpencil_types[type]->name[0] != '\0') {
    return modifier_gpencil_types[type];
  }
  else {
    return NULL;
  }
}

void BKE_gpencil_modifier_copydata_generic(const GpencilModifierData *md_src,
                                           GpencilModifierData *md_dst)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md_src->type);

  /* md_dst may have already be fully initialized with some extra allocated data,
   * we need to free it now to avoid memleak. */
  if (mti->freeData) {
    mti->freeData(md_dst);
  }

  const size_t data_size = sizeof(GpencilModifierData);
  const char *md_src_data = ((const char *)md_src) + data_size;
  char *md_dst_data = ((char *)md_dst) + data_size;
  BLI_assert(data_size <= (size_t)mti->struct_size);
  memcpy(md_dst_data, md_src_data, (size_t)mti->struct_size - data_size);
}

static void gpencil_modifier_copy_data_id_us_cb(void *UNUSED(userData),
                                                Object *UNUSED(ob),
                                                ID **idpoin,
                                                int cb_flag)
{
  ID *id = *idpoin;
  if (id != NULL && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_plus(id);
  }
}

void BKE_gpencil_modifier_copydata_ex(GpencilModifierData *md,
                                      GpencilModifierData *target,
                                      const int flag)
{
  const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

  target->mode = md->mode;
  target->flag = md->flag;

  if (mti->copyData) {
    mti->copyData(md, target);
  }

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    if (mti->foreachIDLink) {
      mti->foreachIDLink(target, NULL, gpencil_modifier_copy_data_id_us_cb, NULL);
    }
    else if (mti->foreachObjectLink) {
      mti->foreachObjectLink(
          target, NULL, (GreasePencilObjectWalkFunc)gpencil_modifier_copy_data_id_us_cb, NULL);
    }
  }
}

void BKE_gpencil_modifier_copydata(GpencilModifierData *md, GpencilModifierData *target)
{
  BKE_gpencil_modifier_copydata_ex(md, target, 0);
}

GpencilModifierData *BKE_gpencil_modifiers_findby_type(Object *ob, GpencilModifierType type)
{
  GpencilModifierData *md = ob->greasepencil_modifiers.first;

  for (; md; md = md->next) {
    if (md->type == type) {
      break;
    }
  }

  return md;
}

void BKE_gpencil_modifiers_foreach_ID_link(Object *ob, GreasePencilIDWalkFunc walk, void *userData)
{
  GpencilModifierData *md = ob->greasepencil_modifiers.first;

  for (; md; md = md->next) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

    if (mti->foreachIDLink) {
      mti->foreachIDLink(md, ob, walk, userData);
    }
    else if (mti->foreachObjectLink) {
      /* each Object can masquerade as an ID, so this should be OK */
      GreasePencilObjectWalkFunc fp = (GreasePencilObjectWalkFunc)walk;
      mti->foreachObjectLink(md, ob, fp, userData);
    }
  }
}

void BKE_gpencil_modifiers_foreach_tex_link(Object *ob,
                                            GreasePencilTexWalkFunc walk,
                                            void *userData)
{
  GpencilModifierData *md = ob->greasepencil_modifiers.first;

  for (; md; md = md->next) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

    if (mti->foreachTexLink) {
      mti->foreachTexLink(md, ob, walk, userData);
    }
  }
}

GpencilModifierData *BKE_gpencil_modifiers_findby_name(Object *ob, const char *name)
{
  return BLI_findstring(&(ob->greasepencil_modifiers), name, offsetof(GpencilModifierData, name));
}

void BKE_gpencil_stroke_subdivide(bGPDstroke *gps, int level, int type)
{
  bGPDspoint *temp_points;
  MDeformVert *temp_dverts = NULL;
  MDeformVert *dvert = NULL;
  MDeformVert *dvert_final = NULL;
  MDeformVert *dvert_next = NULL;
  int totnewpoints, oldtotpoints;
  int i2;

  for (int s = 0; s < level; s++) {
    totnewpoints = gps->totpoints - 1;
    /* duplicate points in a temp area */
    temp_points = MEM_dupallocN(gps->points);
    oldtotpoints = gps->totpoints;

    /* resize the points arrays */
    gps->totpoints += totnewpoints;
    gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
    if (gps->dvert != NULL) {
      temp_dverts = MEM_dupallocN(gps->dvert);
      gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
    }

    /* move points from last to first to new place */
    i2 = gps->totpoints - 1;
    for (int i = oldtotpoints - 1; i > 0; i--) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *pt_final = &gps->points[i2];

      copy_v3_v3(&pt_final->x, &pt->x);
      pt_final->pressure = pt->pressure;
      pt_final->strength = pt->strength;
      pt_final->time = pt->time;
      pt_final->flag = pt->flag;
      pt_final->runtime.pt_orig = pt->runtime.pt_orig;
      pt_final->runtime.idx_orig = pt->runtime.idx_orig;
      copy_v4_v4(pt_final->vert_color, pt->vert_color);

      if (gps->dvert != NULL) {
        dvert = &temp_dverts[i];
        dvert_final = &gps->dvert[i2];
        dvert_final->totweight = dvert->totweight;
        dvert_final->dw = dvert->dw;
      }
      i2 -= 2;
    }
    /* interpolate mid points */
    i2 = 1;
    for (int i = 0; i < oldtotpoints - 1; i++) {
      bGPDspoint *pt = &temp_points[i];
      bGPDspoint *next = &temp_points[i + 1];
      bGPDspoint *pt_final = &gps->points[i2];

      /* add a half way point */
      interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
      pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
      pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
      CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
      pt_final->time = interpf(pt->time, next->time, 0.5f);
      pt_final->runtime.pt_orig = NULL;
      pt_final->flag = 0;
      interp_v4_v4v4(pt_final->vert_color, pt->vert_color, next->vert_color, 0.5f);

      if (gps->dvert != NULL) {
        dvert = &temp_dverts[i];
        dvert_next = &temp_dverts[i + 1];
        dvert_final = &gps->dvert[i2];

        dvert_final->totweight = dvert->totweight;
        dvert_final->dw = MEM_dupallocN(dvert->dw);

        /* interpolate weight values */
        for (int d = 0; d < dvert->totweight; d++) {
          MDeformWeight *dw_a = &dvert->dw[d];
          if (dvert_next->totweight > d) {
            MDeformWeight *dw_b = &dvert_next->dw[d];
            MDeformWeight *dw_final = &dvert_final->dw[d];
            dw_final->weight = interpf(dw_a->weight, dw_b->weight, 0.5f);
          }
        }
      }

      i2 += 2;
    }

    MEM_SAFE_FREE(temp_points);
    MEM_SAFE_FREE(temp_dverts);

    /* move points to smooth stroke (not simple type )*/
    if (type != GP_SUBDIV_SIMPLE) {
      /* duplicate points in a temp area with the new subdivide data */
      temp_points = MEM_dupallocN(gps->points);

      /* extreme points are not changed */
      for (int i = 0; i < gps->totpoints - 2; i++) {
        bGPDspoint *pt = &temp_points[i];
        bGPDspoint *next = &temp_points[i + 1];
        bGPDspoint *pt_final = &gps->points[i + 1];

        /* move point */
        interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
      }
      /* free temp memory */
      MEM_SAFE_FREE(temp_points);
    }
  }

  /* Calc geometry data. */
  BKE_gpencil_stroke_geometry_update(gps);
}

/* Remap frame (Time modifier) */
static int gpencil_remap_time_get(Depsgraph *depsgraph, Scene *scene, Object *ob, bGPDlayer *gpl)
{
  const bool is_render = (bool)(DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const bool time_remap = BKE_gpencil_has_time_modifiers(ob);
  int cfra_eval = (int)DEG_get_ctime(depsgraph);

  int remap_cfra = cfra_eval;
  if (time_remap) {
    remap_cfra = gpencil_time_modifier(depsgraph, scene, ob, gpl, cfra_eval, is_render);
  }

  return remap_cfra;
}

/* Get the current frame retimed with time modifiers. */
bGPDframe *BKE_gpencil_frame_retime_get(Depsgraph *depsgraph,
                                        Scene *scene,
                                        Object *ob,
                                        bGPDlayer *gpl)
{
  int remap_cfra = gpencil_remap_time_get(depsgraph, scene, ob, gpl);
  bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, remap_cfra, GP_GETFRAME_USE_PREV);

  return gpf;
}

static void gpencil_assign_object_eval(Object *object)
{
  BLI_assert(object->id.tag & LIB_TAG_COPIED_ON_WRITE);

  bGPdata *gpd_eval = object->runtime.gpd_eval;

  gpd_eval->id.tag |= LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT;

  if (object->id.tag & LIB_TAG_COPIED_ON_WRITE) {
    object->data = gpd_eval;
  }
}

/* Helper: Copy active frame from original datablock to evaluated datablock for modifiers. */
static void gpencil_copy_activeframe_to_eval(
    Depsgraph *depsgraph, Scene *scene, Object *ob, bGPdata *gpd_orig, bGPdata *gpd_eval)
{

  bGPDlayer *gpl_eval = gpd_eval->layers.first;
  LISTBASE_FOREACH (bGPDlayer *, gpl_orig, &gpd_orig->layers) {

    if (gpl_eval != NULL) {
      int remap_cfra = gpencil_remap_time_get(depsgraph, scene, ob, gpl_orig);

      bGPDframe *gpf_orig = BKE_gpencil_layer_frame_get(
          gpl_orig, remap_cfra, GP_GETFRAME_USE_PREV);

      if (gpf_orig != NULL) {
        int gpf_index = BLI_findindex(&gpl_orig->frames, gpf_orig);
        bGPDframe *gpf_eval = BLI_findlink(&gpl_eval->frames, gpf_index);

        if (gpf_eval != NULL) {
          /* Delete old strokes. */
          BKE_gpencil_free_strokes(gpf_eval);
          /* Copy again strokes. */
          BKE_gpencil_frame_copy_strokes(gpf_orig, gpf_eval);

          gpf_eval->runtime.gpf_orig = (bGPDframe *)gpf_orig;
          BKE_gpencil_frame_original_pointers_update(gpf_orig, gpf_eval);
        }
      }

      gpl_eval = gpl_eval->next;
    }
  }
}

static bGPdata *gpencil_copy_for_eval(bGPdata *gpd)
{
  int flags = LIB_ID_COPY_LOCALIZE;

  bGPdata *result;
  BKE_id_copy_ex(NULL, &gpd->id, (ID **)&result, flags);
  return result;
}

void BKE_gpencil_prepare_eval_data(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  bGPdata *gpd_eval = (bGPdata *)ob->data;
  Object *ob_orig = (Object *)DEG_get_original_id(&ob->id);
  bGPdata *gpd_orig = (bGPdata *)ob_orig->data;

  /* Need check if some layer is parented. */
  bool do_parent = false;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_orig->layers) {
    if (gpl->parent != NULL) {
      do_parent = true;
      break;
    }
  }

  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_eval);
  const bool do_modifiers = (bool)((!is_multiedit) && (ob->greasepencil_modifiers.first != NULL) &&
                                   (!GPENCIL_SIMPLIFY_MODIF(scene)));
  if ((!do_modifiers) && (!do_parent)) {
    return;
  }
  DEG_debug_print_eval(depsgraph, __func__, gpd_eval->id.name, gpd_eval);

  /* If only one user, don't need a new copy, just update data of the frame. */
  if (gpd_orig->id.us == 1) {
    ob->runtime.gpd_eval = NULL;
    gpencil_copy_activeframe_to_eval(depsgraph, scene, ob, ob_orig->data, gpd_eval);
    return;
  }

  /* Copy full Datablock to evaluated version. */
  ob->runtime.gpd_orig = gpd_orig;
  if (ob->runtime.gpd_eval != NULL) {
    BKE_gpencil_eval_delete(ob->runtime.gpd_eval);
    ob->runtime.gpd_eval = NULL;
    ob->data = ob->runtime.gpd_orig;
  }
  ob->runtime.gpd_eval = gpencil_copy_for_eval(ob->runtime.gpd_orig);
  gpencil_assign_object_eval(ob);
  BKE_gpencil_update_orig_pointers(ob_orig, (Object *)ob);
}

/* Calculate gpencil modifiers */
void BKE_gpencil_modifiers_calc(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_edit = GPENCIL_ANY_EDIT_MODE(gpd);
  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  const bool is_render = (bool)(DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  const bool do_modifiers = (bool)((!is_multiedit) && (ob->greasepencil_modifiers.first != NULL) &&
                                   (!GPENCIL_SIMPLIFY_MODIF(scene)));
  if (!do_modifiers) {
    return;
  }

  /* Init general modifiers data. */
  BKE_gpencil_lattice_init(ob);

  const bool time_remap = BKE_gpencil_has_time_modifiers(ob);

  LISTBASE_FOREACH (GpencilModifierData *, md, &ob->greasepencil_modifiers) {

    if (GPENCIL_MODIFIER_ACTIVE(md, is_render)) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(md->type);

      if ((GPENCIL_MODIFIER_EDIT(md, is_edit)) && (!is_render)) {
        continue;
      }

      /* Apply geometry modifiers (add new geometry). */
      if (mti && mti->generateStrokes) {
        mti->generateStrokes(md, depsgraph, ob);
      }

      /* Apply deform modifiers and Time remap (only change geometry). */
      if ((time_remap) || (mti && mti->deformStroke)) {
        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          bGPDframe *gpf = BKE_gpencil_frame_retime_get(depsgraph, scene, ob, gpl);
          if (gpf == NULL) {
            continue;
          }

          if (mti->deformStroke) {
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
              mti->deformStroke(md, depsgraph, ob, gpl, gpf, gps);
            }
          }
        }
      }
    }
  }

  /* Clear any lattice data. */
  BKE_gpencil_lattice_clear(ob);
}
