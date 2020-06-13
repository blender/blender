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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_edgehash.h"
#include "BLI_kdtree.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_lattice.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

static void initData(ModifierData *md)
{
  ExplodeModifierData *emd = (ExplodeModifierData *)md;

  emd->facepa = NULL;
  emd->flag |= eExplodeFlag_Unborn + eExplodeFlag_Alive + eExplodeFlag_Dead;
}
static void freeData(ModifierData *md)
{
  ExplodeModifierData *emd = (ExplodeModifierData *)md;

  MEM_SAFE_FREE(emd->facepa);
}
static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
  const ExplodeModifierData *emd = (const ExplodeModifierData *)md;
#endif
  ExplodeModifierData *temd = (ExplodeModifierData *)target;

  BKE_modifier_copydata_generic(md, target, flag);

  temd->facepa = NULL;
}
static bool dependsOnTime(ModifierData *UNUSED(md))
{
  return true;
}
static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *md,
                             CustomData_MeshMasks *r_cddata_masks)
{
  ExplodeModifierData *emd = (ExplodeModifierData *)md;

  if (emd->vgroup) {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static void createFacepa(ExplodeModifierData *emd, ParticleSystemModifierData *psmd, Mesh *mesh)
{
  ParticleSystem *psys = psmd->psys;
  MFace *fa = NULL, *mface = NULL;
  MVert *mvert = NULL;
  ParticleData *pa;
  KDTree_3d *tree;
  RNG *rng;
  float center[3], co[3];
  int *facepa = NULL, *vertpa = NULL, totvert = 0, totface = 0, totpart = 0;
  int i, p, v1, v2, v3, v4 = 0;
  const bool invert_vgroup = (emd->flag & eExplodeFlag_INVERT_VGROUP) != 0;

  mvert = mesh->mvert;
  mface = mesh->mface;
  totvert = mesh->totvert;
  totface = mesh->totface;
  totpart = psmd->psys->totpart;

  rng = BLI_rng_new_srandom(psys->seed);

  if (emd->facepa) {
    MEM_freeN(emd->facepa);
  }
  facepa = emd->facepa = MEM_calloc_arrayN(totface, sizeof(int), "explode_facepa");

  vertpa = MEM_calloc_arrayN(totvert, sizeof(int), "explode_vertpa");

  /* initialize all faces & verts to no particle */
  for (i = 0; i < totface; i++) {
    facepa[i] = totpart;
  }
  for (i = 0; i < totvert; i++) {
    vertpa[i] = totpart;
  }

  /* set protected verts */
  if (emd->vgroup) {
    MDeformVert *dvert = CustomData_get_layer(&mesh->vdata, CD_MDEFORMVERT);
    if (dvert) {
      const int defgrp_index = emd->vgroup - 1;
      for (i = 0; i < totvert; i++, dvert++) {
        float val = BLI_rng_get_float(rng);
        val = (1.0f - emd->protect) * val + emd->protect * 0.5f;
        const float weight = invert_vgroup ? 1.0f - BKE_defvert_find_weight(dvert, defgrp_index) :
                                             BKE_defvert_find_weight(dvert, defgrp_index);
        if (val < weight) {
          vertpa[i] = -1;
        }
      }
    }
  }

  /* make tree of emitter locations */
  tree = BLI_kdtree_3d_new(totpart);
  for (p = 0, pa = psys->particles; p < totpart; p++, pa++) {
    psys_particle_on_emitter(psmd,
                             psys->part->from,
                             pa->num,
                             pa->num_dmcache,
                             pa->fuv,
                             pa->foffset,
                             co,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
    BLI_kdtree_3d_insert(tree, p, co);
  }
  BLI_kdtree_3d_balance(tree);

  /* set face-particle-indexes to nearest particle to face center */
  for (i = 0, fa = mface; i < totface; i++, fa++) {
    add_v3_v3v3(center, mvert[fa->v1].co, mvert[fa->v2].co);
    add_v3_v3(center, mvert[fa->v3].co);
    if (fa->v4) {
      add_v3_v3(center, mvert[fa->v4].co);
      mul_v3_fl(center, 0.25);
    }
    else {
      mul_v3_fl(center, 1.0f / 3.0f);
    }

    p = BLI_kdtree_3d_find_nearest(tree, center, NULL);

    v1 = vertpa[fa->v1];
    v2 = vertpa[fa->v2];
    v3 = vertpa[fa->v3];
    if (fa->v4) {
      v4 = vertpa[fa->v4];
    }

    if (v1 >= 0 && v2 >= 0 && v3 >= 0 && (fa->v4 == 0 || v4 >= 0)) {
      facepa[i] = p;
    }

    if (v1 >= 0) {
      vertpa[fa->v1] = p;
    }
    if (v2 >= 0) {
      vertpa[fa->v2] = p;
    }
    if (v3 >= 0) {
      vertpa[fa->v3] = p;
    }
    if (fa->v4 && v4 >= 0) {
      vertpa[fa->v4] = p;
    }
  }

  if (vertpa) {
    MEM_freeN(vertpa);
  }
  BLI_kdtree_3d_free(tree);

  BLI_rng_free(rng);
}

static int edgecut_get(EdgeHash *edgehash, uint v1, uint v2)
{
  return POINTER_AS_INT(BLI_edgehash_lookup(edgehash, v1, v2));
}

static const short add_faces[24] = {
    0, 0, 0, 2, 0, 1, 2, 2, 0, 2, 1, 2, 2, 2, 2, 3, 0, 0, 0, 1, 0, 1, 1, 2,
};

static MFace *get_dface(Mesh *mesh, Mesh *split, int cur, int i, MFace *mf)
{
  MFace *df = &split->mface[cur];
  CustomData_copy_data(&mesh->fdata, &split->fdata, i, cur, 1);
  *df = *mf;
  return df;
}

#define SET_VERTS(a, b, c, d) \
  { \
    v[0] = mf->v##a; \
    uv[0] = a - 1; \
    v[1] = mf->v##b; \
    uv[1] = b - 1; \
    v[2] = mf->v##c; \
    uv[2] = c - 1; \
    v[3] = mf->v##d; \
    uv[3] = d - 1; \
  } \
  (void)0

#define GET_ES(v1, v2) edgecut_get(eh, v1, v2)
#define INT_UV(uvf, c0, c1) mid_v2_v2v2(uvf, mf->uv[c0], mf->uv[c1])

static void remap_faces_3_6_9_12(Mesh *mesh,
                                 Mesh *split,
                                 MFace *mf,
                                 int *facepa,
                                 int *vertpa,
                                 int i,
                                 EdgeHash *eh,
                                 int cur,
                                 int v1,
                                 int v2,
                                 int v3,
                                 int v4)
{
  MFace *df1 = get_dface(mesh, split, cur, i, mf);
  MFace *df2 = get_dface(mesh, split, cur + 1, i, mf);
  MFace *df3 = get_dface(mesh, split, cur + 2, i, mf);

  facepa[cur] = vertpa[v1];
  df1->v1 = v1;
  df1->v2 = GET_ES(v1, v2);
  df1->v3 = GET_ES(v2, v3);
  df1->v4 = v3;
  df1->flag |= ME_FACE_SEL;

  facepa[cur + 1] = vertpa[v2];
  df2->v1 = GET_ES(v1, v2);
  df2->v2 = v2;
  df2->v3 = GET_ES(v2, v3);
  df2->v4 = 0;
  df2->flag &= ~ME_FACE_SEL;

  facepa[cur + 2] = vertpa[v1];
  df3->v1 = v1;
  df3->v2 = v3;
  df3->v3 = v4;
  df3->v4 = 0;
  df3->flag &= ~ME_FACE_SEL;
}

static void remap_uvs_3_6_9_12(
    Mesh *mesh, Mesh *split, int numlayer, int i, int cur, int c0, int c1, int c2, int c3)
{
  MTFace *mf, *df1, *df2, *df3;
  int l;

  for (l = 0; l < numlayer; l++) {
    mf = CustomData_get_layer_n(&split->fdata, CD_MTFACE, l);
    df1 = mf + cur;
    df2 = df1 + 1;
    df3 = df1 + 2;
    mf = CustomData_get_layer_n(&mesh->fdata, CD_MTFACE, l);
    mf += i;

    copy_v2_v2(df1->uv[0], mf->uv[c0]);
    INT_UV(df1->uv[1], c0, c1);
    INT_UV(df1->uv[2], c1, c2);
    copy_v2_v2(df1->uv[3], mf->uv[c2]);

    INT_UV(df2->uv[0], c0, c1);
    copy_v2_v2(df2->uv[1], mf->uv[c1]);
    INT_UV(df2->uv[2], c1, c2);

    copy_v2_v2(df3->uv[0], mf->uv[c0]);
    copy_v2_v2(df3->uv[1], mf->uv[c2]);
    copy_v2_v2(df3->uv[2], mf->uv[c3]);
  }
}

static void remap_faces_5_10(Mesh *mesh,
                             Mesh *split,
                             MFace *mf,
                             int *facepa,
                             int *vertpa,
                             int i,
                             EdgeHash *eh,
                             int cur,
                             int v1,
                             int v2,
                             int v3,
                             int v4)
{
  MFace *df1 = get_dface(mesh, split, cur, i, mf);
  MFace *df2 = get_dface(mesh, split, cur + 1, i, mf);

  facepa[cur] = vertpa[v1];
  df1->v1 = v1;
  df1->v2 = v2;
  df1->v3 = GET_ES(v2, v3);
  df1->v4 = GET_ES(v1, v4);
  df1->flag |= ME_FACE_SEL;

  facepa[cur + 1] = vertpa[v3];
  df2->v1 = GET_ES(v1, v4);
  df2->v2 = GET_ES(v2, v3);
  df2->v3 = v3;
  df2->v4 = v4;
  df2->flag |= ME_FACE_SEL;
}

static void remap_uvs_5_10(
    Mesh *mesh, Mesh *split, int numlayer, int i, int cur, int c0, int c1, int c2, int c3)
{
  MTFace *mf, *df1, *df2;
  int l;

  for (l = 0; l < numlayer; l++) {
    mf = CustomData_get_layer_n(&split->fdata, CD_MTFACE, l);
    df1 = mf + cur;
    df2 = df1 + 1;
    mf = CustomData_get_layer_n(&mesh->fdata, CD_MTFACE, l);
    mf += i;

    copy_v2_v2(df1->uv[0], mf->uv[c0]);
    copy_v2_v2(df1->uv[1], mf->uv[c1]);
    INT_UV(df1->uv[2], c1, c2);
    INT_UV(df1->uv[3], c0, c3);

    INT_UV(df2->uv[0], c0, c3);
    INT_UV(df2->uv[1], c1, c2);
    copy_v2_v2(df2->uv[2], mf->uv[c2]);
    copy_v2_v2(df2->uv[3], mf->uv[c3]);
  }
}

static void remap_faces_15(Mesh *mesh,
                           Mesh *split,
                           MFace *mf,
                           int *facepa,
                           int *vertpa,
                           int i,
                           EdgeHash *eh,
                           int cur,
                           int v1,
                           int v2,
                           int v3,
                           int v4)
{
  MFace *df1 = get_dface(mesh, split, cur, i, mf);
  MFace *df2 = get_dface(mesh, split, cur + 1, i, mf);
  MFace *df3 = get_dface(mesh, split, cur + 2, i, mf);
  MFace *df4 = get_dface(mesh, split, cur + 3, i, mf);

  facepa[cur] = vertpa[v1];
  df1->v1 = v1;
  df1->v2 = GET_ES(v1, v2);
  df1->v3 = GET_ES(v1, v3);
  df1->v4 = GET_ES(v1, v4);
  df1->flag |= ME_FACE_SEL;

  facepa[cur + 1] = vertpa[v2];
  df2->v1 = GET_ES(v1, v2);
  df2->v2 = v2;
  df2->v3 = GET_ES(v2, v3);
  df2->v4 = GET_ES(v1, v3);
  df2->flag |= ME_FACE_SEL;

  facepa[cur + 2] = vertpa[v3];
  df3->v1 = GET_ES(v1, v3);
  df3->v2 = GET_ES(v2, v3);
  df3->v3 = v3;
  df3->v4 = GET_ES(v3, v4);
  df3->flag |= ME_FACE_SEL;

  facepa[cur + 3] = vertpa[v4];
  df4->v1 = GET_ES(v1, v4);
  df4->v2 = GET_ES(v1, v3);
  df4->v3 = GET_ES(v3, v4);
  df4->v4 = v4;
  df4->flag |= ME_FACE_SEL;
}

static void remap_uvs_15(
    Mesh *mesh, Mesh *split, int numlayer, int i, int cur, int c0, int c1, int c2, int c3)
{
  MTFace *mf, *df1, *df2, *df3, *df4;
  int l;

  for (l = 0; l < numlayer; l++) {
    mf = CustomData_get_layer_n(&split->fdata, CD_MTFACE, l);
    df1 = mf + cur;
    df2 = df1 + 1;
    df3 = df1 + 2;
    df4 = df1 + 3;
    mf = CustomData_get_layer_n(&mesh->fdata, CD_MTFACE, l);
    mf += i;

    copy_v2_v2(df1->uv[0], mf->uv[c0]);
    INT_UV(df1->uv[1], c0, c1);
    INT_UV(df1->uv[2], c0, c2);
    INT_UV(df1->uv[3], c0, c3);

    INT_UV(df2->uv[0], c0, c1);
    copy_v2_v2(df2->uv[1], mf->uv[c1]);
    INT_UV(df2->uv[2], c1, c2);
    INT_UV(df2->uv[3], c0, c2);

    INT_UV(df3->uv[0], c0, c2);
    INT_UV(df3->uv[1], c1, c2);
    copy_v2_v2(df3->uv[2], mf->uv[c2]);
    INT_UV(df3->uv[3], c2, c3);

    INT_UV(df4->uv[0], c0, c3);
    INT_UV(df4->uv[1], c0, c2);
    INT_UV(df4->uv[2], c2, c3);
    copy_v2_v2(df4->uv[3], mf->uv[c3]);
  }
}

static void remap_faces_7_11_13_14(Mesh *mesh,
                                   Mesh *split,
                                   MFace *mf,
                                   int *facepa,
                                   int *vertpa,
                                   int i,
                                   EdgeHash *eh,
                                   int cur,
                                   int v1,
                                   int v2,
                                   int v3,
                                   int v4)
{
  MFace *df1 = get_dface(mesh, split, cur, i, mf);
  MFace *df2 = get_dface(mesh, split, cur + 1, i, mf);
  MFace *df3 = get_dface(mesh, split, cur + 2, i, mf);

  facepa[cur] = vertpa[v1];
  df1->v1 = v1;
  df1->v2 = GET_ES(v1, v2);
  df1->v3 = GET_ES(v2, v3);
  df1->v4 = GET_ES(v1, v4);
  df1->flag |= ME_FACE_SEL;

  facepa[cur + 1] = vertpa[v2];
  df2->v1 = GET_ES(v1, v2);
  df2->v2 = v2;
  df2->v3 = GET_ES(v2, v3);
  df2->v4 = 0;
  df2->flag &= ~ME_FACE_SEL;

  facepa[cur + 2] = vertpa[v4];
  df3->v1 = GET_ES(v1, v4);
  df3->v2 = GET_ES(v2, v3);
  df3->v3 = v3;
  df3->v4 = v4;
  df3->flag |= ME_FACE_SEL;
}

static void remap_uvs_7_11_13_14(
    Mesh *mesh, Mesh *split, int numlayer, int i, int cur, int c0, int c1, int c2, int c3)
{
  MTFace *mf, *df1, *df2, *df3;
  int l;

  for (l = 0; l < numlayer; l++) {
    mf = CustomData_get_layer_n(&split->fdata, CD_MTFACE, l);
    df1 = mf + cur;
    df2 = df1 + 1;
    df3 = df1 + 2;
    mf = CustomData_get_layer_n(&mesh->fdata, CD_MTFACE, l);
    mf += i;

    copy_v2_v2(df1->uv[0], mf->uv[c0]);
    INT_UV(df1->uv[1], c0, c1);
    INT_UV(df1->uv[2], c1, c2);
    INT_UV(df1->uv[3], c0, c3);

    INT_UV(df2->uv[0], c0, c1);
    copy_v2_v2(df2->uv[1], mf->uv[c1]);
    INT_UV(df2->uv[2], c1, c2);

    INT_UV(df3->uv[0], c0, c3);
    INT_UV(df3->uv[1], c1, c2);
    copy_v2_v2(df3->uv[2], mf->uv[c2]);
    copy_v2_v2(df3->uv[3], mf->uv[c3]);
  }
}

static void remap_faces_19_21_22(Mesh *mesh,
                                 Mesh *split,
                                 MFace *mf,
                                 int *facepa,
                                 int *vertpa,
                                 int i,
                                 EdgeHash *eh,
                                 int cur,
                                 int v1,
                                 int v2,
                                 int v3)
{
  MFace *df1 = get_dface(mesh, split, cur, i, mf);
  MFace *df2 = get_dface(mesh, split, cur + 1, i, mf);

  facepa[cur] = vertpa[v1];
  df1->v1 = v1;
  df1->v2 = GET_ES(v1, v2);
  df1->v3 = GET_ES(v1, v3);
  df1->v4 = 0;
  df1->flag &= ~ME_FACE_SEL;

  facepa[cur + 1] = vertpa[v2];
  df2->v1 = GET_ES(v1, v2);
  df2->v2 = v2;
  df2->v3 = v3;
  df2->v4 = GET_ES(v1, v3);
  df2->flag |= ME_FACE_SEL;
}

static void remap_uvs_19_21_22(
    Mesh *mesh, Mesh *split, int numlayer, int i, int cur, int c0, int c1, int c2)
{
  MTFace *mf, *df1, *df2;
  int l;

  for (l = 0; l < numlayer; l++) {
    mf = CustomData_get_layer_n(&split->fdata, CD_MTFACE, l);
    df1 = mf + cur;
    df2 = df1 + 1;
    mf = CustomData_get_layer_n(&mesh->fdata, CD_MTFACE, l);
    mf += i;

    copy_v2_v2(df1->uv[0], mf->uv[c0]);
    INT_UV(df1->uv[1], c0, c1);
    INT_UV(df1->uv[2], c0, c2);

    INT_UV(df2->uv[0], c0, c1);
    copy_v2_v2(df2->uv[1], mf->uv[c1]);
    copy_v2_v2(df2->uv[2], mf->uv[c2]);
    INT_UV(df2->uv[3], c0, c2);
  }
}

static void remap_faces_23(Mesh *mesh,
                           Mesh *split,
                           MFace *mf,
                           int *facepa,
                           int *vertpa,
                           int i,
                           EdgeHash *eh,
                           int cur,
                           int v1,
                           int v2,
                           int v3)
{
  MFace *df1 = get_dface(mesh, split, cur, i, mf);
  MFace *df2 = get_dface(mesh, split, cur + 1, i, mf);
  MFace *df3 = get_dface(mesh, split, cur + 2, i, mf);

  facepa[cur] = vertpa[v1];
  df1->v1 = v1;
  df1->v2 = GET_ES(v1, v2);
  df1->v3 = GET_ES(v2, v3);
  df1->v4 = GET_ES(v1, v3);
  df1->flag |= ME_FACE_SEL;

  facepa[cur + 1] = vertpa[v2];
  df2->v1 = GET_ES(v1, v2);
  df2->v2 = v2;
  df2->v3 = GET_ES(v2, v3);
  df2->v4 = 0;
  df2->flag &= ~ME_FACE_SEL;

  facepa[cur + 2] = vertpa[v3];
  df3->v1 = GET_ES(v1, v3);
  df3->v2 = GET_ES(v2, v3);
  df3->v3 = v3;
  df3->v4 = 0;
  df3->flag &= ~ME_FACE_SEL;
}

static void remap_uvs_23(
    Mesh *mesh, Mesh *split, int numlayer, int i, int cur, int c0, int c1, int c2)
{
  MTFace *mf, *df1, *df2;
  int l;

  for (l = 0; l < numlayer; l++) {
    mf = CustomData_get_layer_n(&split->fdata, CD_MTFACE, l);
    df1 = mf + cur;
    df2 = df1 + 1;
    mf = CustomData_get_layer_n(&mesh->fdata, CD_MTFACE, l);
    mf += i;

    copy_v2_v2(df1->uv[0], mf->uv[c0]);
    INT_UV(df1->uv[1], c0, c1);
    INT_UV(df1->uv[2], c1, c2);
    INT_UV(df1->uv[3], c0, c2);

    INT_UV(df2->uv[0], c0, c1);
    copy_v2_v2(df2->uv[1], mf->uv[c1]);
    INT_UV(df2->uv[2], c1, c2);

    INT_UV(df2->uv[0], c0, c2);
    INT_UV(df2->uv[1], c1, c2);
    copy_v2_v2(df2->uv[2], mf->uv[c2]);
  }
}

static Mesh *cutEdges(ExplodeModifierData *emd, Mesh *mesh)
{
  Mesh *split_m;
  MFace *mf = NULL, *df1 = NULL;
  MFace *mface = mesh->mface;
  MVert *dupve, *mv;
  EdgeHash *edgehash;
  EdgeHashIterator *ehi;
  int totvert = mesh->totvert;
  int totface = mesh->totface;

  int *facesplit = MEM_calloc_arrayN(totface, sizeof(int), "explode_facesplit");
  int *vertpa = MEM_calloc_arrayN(totvert, sizeof(int), "explode_vertpa2");
  int *facepa = emd->facepa;
  int *fs, totesplit = 0, totfsplit = 0, curdupface = 0;
  int i, v1, v2, v3, v4, esplit, v[4] = {0, 0, 0, 0}, /* To quite gcc barking... */
      uv[4] = {0, 0, 0, 0};                           /* To quite gcc barking... */
  int numlayer;
  uint ed_v1, ed_v2;

  edgehash = BLI_edgehash_new(__func__);

  /* recreate vertpa from facepa calculation */
  for (i = 0, mf = mface; i < totface; i++, mf++) {
    vertpa[mf->v1] = facepa[i];
    vertpa[mf->v2] = facepa[i];
    vertpa[mf->v3] = facepa[i];
    if (mf->v4) {
      vertpa[mf->v4] = facepa[i];
    }
  }

  /* mark edges for splitting and how to split faces */
  for (i = 0, mf = mface, fs = facesplit; i < totface; i++, mf++, fs++) {
    v1 = vertpa[mf->v1];
    v2 = vertpa[mf->v2];
    v3 = vertpa[mf->v3];

    if (v1 != v2) {
      BLI_edgehash_reinsert(edgehash, mf->v1, mf->v2, NULL);
      (*fs) |= 1;
    }

    if (v2 != v3) {
      BLI_edgehash_reinsert(edgehash, mf->v2, mf->v3, NULL);
      (*fs) |= 2;
    }

    if (mf->v4) {
      v4 = vertpa[mf->v4];

      if (v3 != v4) {
        BLI_edgehash_reinsert(edgehash, mf->v3, mf->v4, NULL);
        (*fs) |= 4;
      }

      if (v1 != v4) {
        BLI_edgehash_reinsert(edgehash, mf->v1, mf->v4, NULL);
        (*fs) |= 8;
      }

      /* mark center vertex as a fake edge split */
      if (*fs == 15) {
        BLI_edgehash_reinsert(edgehash, mf->v1, mf->v3, NULL);
      }
    }
    else {
      (*fs) |= 16; /* mark face as tri */

      if (v1 != v3) {
        BLI_edgehash_reinsert(edgehash, mf->v1, mf->v3, NULL);
        (*fs) |= 4;
      }
    }
  }

  /* count splits & create indexes for new verts */
  ehi = BLI_edgehashIterator_new(edgehash);
  totesplit = totvert;
  for (; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
    BLI_edgehashIterator_setValue(ehi, POINTER_FROM_INT(totesplit));
    totesplit++;
  }
  BLI_edgehashIterator_free(ehi);

  /* count new faces due to splitting */
  for (i = 0, fs = facesplit; i < totface; i++, fs++) {
    totfsplit += add_faces[*fs];
  }

  split_m = BKE_mesh_new_nomain_from_template(mesh, totesplit, 0, totface + totfsplit, 0, 0);

  numlayer = CustomData_number_of_layers(&split_m->fdata, CD_MTFACE);

  /* copy new faces & verts (is it really this painful with custom data??) */
  for (i = 0; i < totvert; i++) {
    MVert source;
    MVert *dest;
    source = mesh->mvert[i];
    dest = &split_m->mvert[i];

    CustomData_copy_data(&mesh->vdata, &split_m->vdata, i, i, 1);
    *dest = source;
  }

  /* override original facepa (original pointer is saved in caller function) */

  /* BMESH_TODO, (totfsplit * 2) over allocation is used since the quads are
   * later interpreted as tri's, for this to work right I think we probably
   * have to stop using tessface - campbell */

  facepa = MEM_calloc_arrayN((totface + (totfsplit * 2)), sizeof(int), "explode_facepa");
  // memcpy(facepa, emd->facepa, totface*sizeof(int));
  emd->facepa = facepa;

  /* create new verts */
  ehi = BLI_edgehashIterator_new(edgehash);
  for (; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
    BLI_edgehashIterator_getKey(ehi, &ed_v1, &ed_v2);
    esplit = POINTER_AS_INT(BLI_edgehashIterator_getValue(ehi));
    mv = &split_m->mvert[ed_v2];
    dupve = &split_m->mvert[esplit];

    CustomData_copy_data(&split_m->vdata, &split_m->vdata, ed_v2, esplit, 1);

    *dupve = *mv;

    mv = &split_m->mvert[ed_v1];

    mid_v3_v3v3(dupve->co, dupve->co, mv->co);
  }
  BLI_edgehashIterator_free(ehi);

  /* create new faces */
  curdupface = 0;  //=totface;
  // curdupin=totesplit;
  for (i = 0, fs = facesplit; i < totface; i++, fs++) {
    mf = &mesh->mface[i];

    switch (*fs) {
      case 3:
      case 10:
      case 11:
      case 15:
        SET_VERTS(1, 2, 3, 4);
        break;
      case 5:
      case 6:
      case 7:
        SET_VERTS(2, 3, 4, 1);
        break;
      case 9:
      case 13:
        SET_VERTS(4, 1, 2, 3);
        break;
      case 12:
      case 14:
        SET_VERTS(3, 4, 1, 2);
        break;
      case 21:
      case 23:
        SET_VERTS(1, 2, 3, 4);
        break;
      case 19:
        SET_VERTS(2, 3, 1, 4);
        break;
      case 22:
        SET_VERTS(3, 1, 2, 4);
        break;
    }

    switch (*fs) {
      case 3:
      case 6:
      case 9:
      case 12:
        remap_faces_3_6_9_12(
            mesh, split_m, mf, facepa, vertpa, i, edgehash, curdupface, v[0], v[1], v[2], v[3]);
        if (numlayer) {
          remap_uvs_3_6_9_12(mesh, split_m, numlayer, i, curdupface, uv[0], uv[1], uv[2], uv[3]);
        }
        break;
      case 5:
      case 10:
        remap_faces_5_10(
            mesh, split_m, mf, facepa, vertpa, i, edgehash, curdupface, v[0], v[1], v[2], v[3]);
        if (numlayer) {
          remap_uvs_5_10(mesh, split_m, numlayer, i, curdupface, uv[0], uv[1], uv[2], uv[3]);
        }
        break;
      case 15:
        remap_faces_15(
            mesh, split_m, mf, facepa, vertpa, i, edgehash, curdupface, v[0], v[1], v[2], v[3]);
        if (numlayer) {
          remap_uvs_15(mesh, split_m, numlayer, i, curdupface, uv[0], uv[1], uv[2], uv[3]);
        }
        break;
      case 7:
      case 11:
      case 13:
      case 14:
        remap_faces_7_11_13_14(
            mesh, split_m, mf, facepa, vertpa, i, edgehash, curdupface, v[0], v[1], v[2], v[3]);
        if (numlayer) {
          remap_uvs_7_11_13_14(mesh, split_m, numlayer, i, curdupface, uv[0], uv[1], uv[2], uv[3]);
        }
        break;
      case 19:
      case 21:
      case 22:
        remap_faces_19_21_22(
            mesh, split_m, mf, facepa, vertpa, i, edgehash, curdupface, v[0], v[1], v[2]);
        if (numlayer) {
          remap_uvs_19_21_22(mesh, split_m, numlayer, i, curdupface, uv[0], uv[1], uv[2]);
        }
        break;
      case 23:
        remap_faces_23(
            mesh, split_m, mf, facepa, vertpa, i, edgehash, curdupface, v[0], v[1], v[2]);
        if (numlayer) {
          remap_uvs_23(mesh, split_m, numlayer, i, curdupface, uv[0], uv[1], uv[2]);
        }
        break;
      case 0:
      case 16:
        df1 = get_dface(mesh, split_m, curdupface, i, mf);
        facepa[curdupface] = vertpa[mf->v1];

        if (df1->v4) {
          df1->flag |= ME_FACE_SEL;
        }
        else {
          df1->flag &= ~ME_FACE_SEL;
        }
        break;
    }

    curdupface += add_faces[*fs] + 1;
  }

  for (i = 0; i < curdupface; i++) {
    mf = &split_m->mface[i];
    test_index_face(mf, &split_m->fdata, i, ((mf->flag & ME_FACE_SEL) ? 4 : 3));
  }

  BLI_edgehash_free(edgehash, NULL);
  MEM_freeN(facesplit);
  MEM_freeN(vertpa);

  BKE_mesh_calc_edges_tessface(split_m);
  BKE_mesh_convert_mfaces_to_mpolys(split_m);

  return split_m;
}
static Mesh *explodeMesh(ExplodeModifierData *emd,
                         ParticleSystemModifierData *psmd,
                         const ModifierEvalContext *ctx,
                         Scene *scene,
                         Mesh *to_explode)
{
  Mesh *explode, *mesh = to_explode;
  MFace *mf = NULL, *mface;
  /* ParticleSettings *part=psmd->psys->part; */ /* UNUSED */
  ParticleSimulationData sim = {NULL};
  ParticleData *pa = NULL, *pars = psmd->psys->particles;
  ParticleKey state, birth;
  EdgeHash *vertpahash;
  EdgeHashIterator *ehi;
  float *vertco = NULL, imat[4][4];
  float rot[4];
  float cfra;
  /* float timestep; */
  const int *facepa = emd->facepa;
  int totdup = 0, totvert = 0, totface = 0, totpart = 0, delface = 0;
  int i, v, u;
  uint ed_v1, ed_v2, mindex = 0;
  MTFace *mtface = NULL, *mtf;

  totface = mesh->totface;
  totvert = mesh->totvert;
  mface = mesh->mface;
  totpart = psmd->psys->totpart;

  sim.depsgraph = ctx->depsgraph;
  sim.scene = scene;
  sim.ob = ctx->object;
  sim.psys = psmd->psys;
  sim.psmd = psmd;

  /* timestep = psys_get_timestep(&sim); */

  cfra = BKE_scene_frame_get(scene);

  /* hash table for vertice <-> particle relations */
  vertpahash = BLI_edgehash_new(__func__);

  for (i = 0; i < totface; i++) {
    if (facepa[i] != totpart) {
      pa = pars + facepa[i];

      if ((pa->alive == PARS_UNBORN && (emd->flag & eExplodeFlag_Unborn) == 0) ||
          (pa->alive == PARS_ALIVE && (emd->flag & eExplodeFlag_Alive) == 0) ||
          (pa->alive == PARS_DEAD && (emd->flag & eExplodeFlag_Dead) == 0)) {
        delface++;
        continue;
      }
    }
    else {
      pa = NULL;
    }

    /* do mindex + totvert to ensure the vertex index to be the first
     * with BLI_edgehashIterator_getKey */
    if (pa == NULL || cfra < pa->time) {
      mindex = totvert + totpart;
    }
    else {
      mindex = totvert + facepa[i];
    }

    mf = &mface[i];

    /* set face vertices to exist in particle group */
    BLI_edgehash_reinsert(vertpahash, mf->v1, mindex, NULL);
    BLI_edgehash_reinsert(vertpahash, mf->v2, mindex, NULL);
    BLI_edgehash_reinsert(vertpahash, mf->v3, mindex, NULL);
    if (mf->v4) {
      BLI_edgehash_reinsert(vertpahash, mf->v4, mindex, NULL);
    }
  }

  /* make new vertice indexes & count total vertices after duplication */
  ehi = BLI_edgehashIterator_new(vertpahash);
  for (; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
    BLI_edgehashIterator_setValue(ehi, POINTER_FROM_INT(totdup));
    totdup++;
  }
  BLI_edgehashIterator_free(ehi);

  /* the final duplicated vertices */
  explode = BKE_mesh_new_nomain_from_template(mesh, totdup, 0, totface - delface, 0, 0);

  mtface = CustomData_get_layer_named(&explode->fdata, CD_MTFACE, emd->uvname);

  /* getting back to object space */
  invert_m4_m4(imat, ctx->object->obmat);

  psmd->psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

  /* duplicate & displace vertices */
  ehi = BLI_edgehashIterator_new(vertpahash);
  for (; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
    MVert source;
    MVert *dest;

    /* get particle + vertex from hash */
    BLI_edgehashIterator_getKey(ehi, &ed_v1, &ed_v2);
    ed_v2 -= totvert;
    v = POINTER_AS_INT(BLI_edgehashIterator_getValue(ehi));

    source = mesh->mvert[ed_v1];
    dest = &explode->mvert[v];

    CustomData_copy_data(&mesh->vdata, &explode->vdata, ed_v1, v, 1);

    *dest = source;

    if (ed_v2 != totpart) {
      /* get particle */
      pa = pars + ed_v2;

      psys_get_birth_coords(&sim, pa, &birth, 0, 0);

      state.time = cfra;
      psys_get_particle_state(&sim, ed_v2, &state, 1);

      vertco = explode->mvert[v].co;
      mul_m4_v3(ctx->object->obmat, vertco);

      sub_v3_v3(vertco, birth.co);

      /* apply rotation, size & location */
      sub_qt_qtqt(rot, state.rot, birth.rot);
      mul_qt_v3(rot, vertco);

      if (emd->flag & eExplodeFlag_PaSize) {
        mul_v3_fl(vertco, pa->size);
      }

      add_v3_v3(vertco, state.co);

      mul_m4_v3(imat, vertco);
    }
    else {
      pa = NULL;
    }
  }
  BLI_edgehashIterator_free(ehi);

  /*map new vertices to faces*/
  for (i = 0, u = 0; i < totface; i++) {
    MFace source;
    int orig_v4;

    if (facepa[i] != totpart) {
      pa = pars + facepa[i];

      if (pa->alive == PARS_UNBORN && (emd->flag & eExplodeFlag_Unborn) == 0) {
        continue;
      }
      if (pa->alive == PARS_ALIVE && (emd->flag & eExplodeFlag_Alive) == 0) {
        continue;
      }
      if (pa->alive == PARS_DEAD && (emd->flag & eExplodeFlag_Dead) == 0) {
        continue;
      }
    }
    else {
      pa = NULL;
    }

    source = mesh->mface[i];
    mf = &explode->mface[u];

    orig_v4 = source.v4;

    /* Same as above in the first loop over mesh's faces. */
    if (pa == NULL || cfra < pa->time) {
      mindex = totvert + totpart;
    }
    else {
      mindex = totvert + facepa[i];
    }

    source.v1 = edgecut_get(vertpahash, source.v1, mindex);
    source.v2 = edgecut_get(vertpahash, source.v2, mindex);
    source.v3 = edgecut_get(vertpahash, source.v3, mindex);
    if (source.v4) {
      source.v4 = edgecut_get(vertpahash, source.v4, mindex);
    }

    CustomData_copy_data(&mesh->fdata, &explode->fdata, i, u, 1);

    *mf = source;

    /* override uv channel for particle age */
    if (mtface) {
      float age = (pa != NULL) ? (cfra - pa->time) / pa->lifetime : 0.0f;
      /* Clamp to this range to avoid flipping to the other side of the coordinates. */
      CLAMP(age, 0.001f, 0.999f);

      mtf = mtface + u;

      mtf->uv[0][0] = mtf->uv[1][0] = mtf->uv[2][0] = mtf->uv[3][0] = age;
      mtf->uv[0][1] = mtf->uv[1][1] = mtf->uv[2][1] = mtf->uv[3][1] = 0.5f;
    }

    test_index_face(mf, &explode->fdata, u, (orig_v4 ? 4 : 3));
    u++;
  }

  /* cleanup */
  BLI_edgehash_free(vertpahash, NULL);

  /* finalization */
  BKE_mesh_calc_edges_tessface(explode);
  BKE_mesh_convert_mfaces_to_mpolys(explode);
  explode->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  if (psmd->psys->lattice_deform_data) {
    BKE_lattice_deform_data_destroy(psmd->psys->lattice_deform_data);
    psmd->psys->lattice_deform_data = NULL;
  }

  return explode;
}

static ParticleSystemModifierData *findPrecedingParticlesystem(Object *ob, ModifierData *emd)
{
  ModifierData *md;
  ParticleSystemModifierData *psmd = NULL;

  for (md = ob->modifiers.first; emd != md; md = md->next) {
    if (md->type == eModifierType_ParticleSystem) {
      psmd = (ParticleSystemModifierData *)md;
    }
  }
  return psmd;
}
static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  ExplodeModifierData *emd = (ExplodeModifierData *)md;
  ParticleSystemModifierData *psmd = findPrecedingParticlesystem(ctx->object, md);

  if (psmd) {
    ParticleSystem *psys = psmd->psys;

    if (psys == NULL || psys->totpart == 0) {
      return mesh;
    }
    if (psys->part == NULL || psys->particles == NULL) {
      return mesh;
    }
    if (psmd->mesh_final == NULL) {
      return mesh;
    }

    BKE_mesh_tessface_ensure(mesh); /* BMESH - UNTIL MODIFIER IS UPDATED FOR MPoly */

    /* 1. find faces to be exploded if needed */
    if (emd->facepa == NULL || psmd->flag & eParticleSystemFlag_Pars ||
        emd->flag & eExplodeFlag_CalcFaces ||
        MEM_allocN_len(emd->facepa) / sizeof(int) != mesh->totface) {
      if (psmd->flag & eParticleSystemFlag_Pars) {
        psmd->flag &= ~eParticleSystemFlag_Pars;
      }
      if (emd->flag & eExplodeFlag_CalcFaces) {
        emd->flag &= ~eExplodeFlag_CalcFaces;
      }
      createFacepa(emd, psmd, mesh);
    }
    /* 2. create new mesh */
    Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
    if (emd->flag & eExplodeFlag_EdgeCut) {
      int *facepa = emd->facepa;
      Mesh *split_m = cutEdges(emd, mesh);
      Mesh *explode = explodeMesh(emd, psmd, ctx, scene, split_m);

      MEM_freeN(emd->facepa);
      emd->facepa = facepa;
      BKE_id_free(NULL, split_m);
      return explode;
    }
    else {
      return explodeMesh(emd, psmd, ctx, scene, mesh);
    }
  }
  return mesh;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *row;
  uiLayout *layout = panel->layout;
  int toggles_flag = UI_ITEM_R_TOGGLE | UI_ITEM_R_FORCE_BLANK_DECORATE;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  PointerRNA obj_data_ptr = RNA_pointer_get(&ob_ptr, "data");
  bool has_vertex_group = RNA_string_length(&ptr, "vertex_group") != 0;

  uiLayoutSetPropSep(layout, true);

  uiItemPointerR(layout, &ptr, "particle_uv", &obj_data_ptr, "uv_layers", NULL, ICON_NONE);

  row = uiLayoutRowWithHeading(layout, true, IFACE_("Show"));
  uiItemR(row, &ptr, "show_alive", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, &ptr, "show_dead", toggles_flag, NULL, ICON_NONE);
  uiItemR(row, &ptr, "show_unborn", toggles_flag, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, &ptr, "use_edge_cut", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "use_size", 0, NULL, ICON_NONE);

  modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", NULL);

  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, has_vertex_group);
  uiItemR(row, &ptr, "protect", 0, NULL, ICON_NONE);

  uiItemO(layout, IFACE_("Refresh"), ICON_NONE, "OBJECT_OT_explode_refresh");

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Explode, panel_draw);
}

ModifierTypeInfo modifierType_Explode = {
    /* name */ "Explode",
    /* structName */ "ExplodeModifierData",
    /* structSize */ sizeof(ExplodeModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh,
    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ freeData,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ NULL,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
};
