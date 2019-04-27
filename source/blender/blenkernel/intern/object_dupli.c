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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <limits.h>
#include <stdlib.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLI_math.h"
#include "BLI_rand.h"

#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"

#include "BKE_animsys.h"
#include "BKE_collection.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_iterators.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_editmesh.h"
#include "BKE_anim.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "BLI_strict_flags.h"
#include "BLI_hash.h"

/* Dupli-Geometry */

typedef struct DupliContext {
  Depsgraph *depsgraph;
  /** XXX child objects are selected from this group if set, could be nicer. */
  Collection *collection;
  /** Only to check if the object is in edit-mode. */
  Object *obedit;

  Scene *scene;
  ViewLayer *view_layer;
  Object *object;
  float space_mat[4][4];

  int persistent_id[MAX_DUPLI_RECUR];
  int level;

  const struct DupliGenerator *gen;

  /** Result containers. */
  ListBase *duplilist; /* legacy doubly-linked list */
} DupliContext;

typedef struct DupliGenerator {
  short type; /* dupli type */
  void (*make_duplis)(const DupliContext *ctx);
} DupliGenerator;

static const DupliGenerator *get_dupli_generator(const DupliContext *ctx);

/* create initial context for root object */
static void init_context(
    DupliContext *r_ctx, Depsgraph *depsgraph, Scene *scene, Object *ob, float space_mat[4][4])
{
  r_ctx->depsgraph = depsgraph;
  r_ctx->scene = scene;
  r_ctx->view_layer = DEG_get_evaluated_view_layer(depsgraph);
  r_ctx->collection = NULL;

  r_ctx->object = ob;
  r_ctx->obedit = OBEDIT_FROM_OBACT(ob);
  if (space_mat) {
    copy_m4_m4(r_ctx->space_mat, space_mat);
  }
  else {
    unit_m4(r_ctx->space_mat);
  }
  r_ctx->level = 0;

  r_ctx->gen = get_dupli_generator(r_ctx);

  r_ctx->duplilist = NULL;
}

/* create sub-context for recursive duplis */
static void copy_dupli_context(
    DupliContext *r_ctx, const DupliContext *ctx, Object *ob, float mat[4][4], int index)
{
  *r_ctx = *ctx;

  /* XXX annoying, previously was done by passing an ID* argument,
   * this at least is more explicit. */
  if (ctx->gen->type == OB_DUPLICOLLECTION) {
    r_ctx->collection = ctx->object->instance_collection;
  }

  r_ctx->object = ob;
  if (mat) {
    mul_m4_m4m4(r_ctx->space_mat, (float(*)[4])ctx->space_mat, mat);
  }
  r_ctx->persistent_id[r_ctx->level] = index;
  ++r_ctx->level;

  r_ctx->gen = get_dupli_generator(r_ctx);
}

/* generate a dupli instance
 * mat is transform of the object relative to current context (including object obmat)
 */
static DupliObject *make_dupli(const DupliContext *ctx, Object *ob, float mat[4][4], int index)
{
  DupliObject *dob;
  int i;

  /* add a DupliObject instance to the result container */
  if (ctx->duplilist) {
    dob = MEM_callocN(sizeof(DupliObject), "dupli object");
    BLI_addtail(ctx->duplilist, dob);
  }
  else {
    return NULL;
  }

  dob->ob = ob;
  mul_m4_m4m4(dob->mat, (float(*)[4])ctx->space_mat, mat);
  dob->type = ctx->gen->type;

  /* set persistent id, which is an array with a persistent index for each level
   * (particle number, vertex number, ..). by comparing this we can find the same
   * dupli object between frames, which is needed for motion blur. last level
   * goes first in the array. */
  dob->persistent_id[0] = index;
  for (i = 1; i < ctx->level + 1; i++) {
    dob->persistent_id[i] = ctx->persistent_id[ctx->level - i];
  }
  /* fill rest of values with INT_MAX which index will never have as value */
  for (; i < MAX_DUPLI_RECUR; i++) {
    dob->persistent_id[i] = INT_MAX;
  }

  /* metaballs never draw in duplis, they are instead merged into one by the basis
   * mball outside of the group. this does mean that if that mball is not in the
   * scene, they will not show up at all, limitation that should be solved once. */
  if (ob->type == OB_MBALL) {
    dob->no_draw = true;
  }

  /* random number */
  /* the logic here is designed to match Cycles */
  dob->random_id = BLI_hash_string(dob->ob->id.name + 2);

  if (dob->persistent_id[0] != INT_MAX) {
    for (i = 0; i < MAX_DUPLI_RECUR * 2; i++) {
      dob->random_id = BLI_hash_int_2d(dob->random_id, (unsigned int)dob->persistent_id[i]);
    }
  }
  else {
    dob->random_id = BLI_hash_int_2d(dob->random_id, 0);
  }

  if (ctx->object != ob) {
    dob->random_id ^= BLI_hash_int(BLI_hash_string(ctx->object->id.name + 2));
  }

  return dob;
}

/* recursive dupli objects
 * space_mat is the local dupli space (excluding dupli object obmat!)
 */
static void make_recursive_duplis(const DupliContext *ctx,
                                  Object *ob,
                                  float space_mat[4][4],
                                  int index)
{
  /* simple preventing of too deep nested collections with MAX_DUPLI_RECUR */
  if (ctx->level < MAX_DUPLI_RECUR) {
    DupliContext rctx;
    copy_dupli_context(&rctx, ctx, ob, space_mat, index);
    if (rctx.gen) {
      rctx.gen->make_duplis(&rctx);
    }
  }
}

/* ---- Child Duplis ---- */

typedef void (*MakeChildDuplisFunc)(const DupliContext *ctx, void *userdata, Object *child);

static bool is_child(const Object *ob, const Object *parent)
{
  const Object *ob_parent = ob->parent;
  while (ob_parent) {
    if (ob_parent == parent) {
      return true;
    }
    ob_parent = ob_parent->parent;
  }
  return false;
}

/* create duplis from every child in scene or collection */
static void make_child_duplis(const DupliContext *ctx,
                              void *userdata,
                              MakeChildDuplisFunc make_child_duplis_cb)
{
  Object *parent = ctx->object;

  if (ctx->collection) {
    eEvaluationMode mode = DEG_get_mode(ctx->depsgraph);
    FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (ctx->collection, ob, mode) {
      if ((ob != ctx->obedit) && is_child(ob, parent)) {
        DupliContext pctx;
        copy_dupli_context(&pctx, ctx, ctx->object, NULL, _base_id);

        /* metaballs have a different dupli handling */
        if (ob->type != OB_MBALL) {
          ob->flag |= OB_DONE; /* doesn't render */
        }
        make_child_duplis_cb(&pctx, userdata, ob);
      }
    }
    FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
  }
  else {
    int baseid = 0;
    ViewLayer *view_layer = ctx->view_layer;
    for (Base *base = view_layer->object_bases.first; base; base = base->next, baseid++) {
      Object *ob = base->object;
      if ((ob != ctx->obedit) && is_child(ob, parent)) {
        DupliContext pctx;
        copy_dupli_context(&pctx, ctx, ctx->object, NULL, baseid);

        /* metaballs have a different dupli handling */
        if (ob->type != OB_MBALL) {
          ob->flag |= OB_DONE; /* doesn't render */
        }

        make_child_duplis_cb(&pctx, userdata, ob);
      }
    }
  }
}

/*---- Implementations ----*/

/* OB_DUPLICOLLECTION */
static void make_duplis_collection(const DupliContext *ctx)
{
  Object *ob = ctx->object;
  Collection *collection;
  float collection_mat[4][4];

  if (ob->instance_collection == NULL) {
    return;
  }
  collection = ob->instance_collection;

  /* combine collection offset and obmat */
  unit_m4(collection_mat);
  sub_v3_v3(collection_mat[3], collection->instance_offset);
  mul_m4_m4m4(collection_mat, ob->obmat, collection_mat);
  /* don't access 'ob->obmat' from now on. */

  eEvaluationMode mode = DEG_get_mode(ctx->depsgraph);
  FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (collection, cob, mode) {
    if (cob != ob) {
      float mat[4][4];

      /* collection dupli offset, should apply after everything else */
      mul_m4_m4m4(mat, collection_mat, cob->obmat);

      make_dupli(ctx, cob, mat, _base_id);

      /* recursion */
      make_recursive_duplis(ctx, cob, collection_mat, _base_id);
    }
  }
  FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
}

static const DupliGenerator gen_dupli_collection = {
    OB_DUPLICOLLECTION,    /* type */
    make_duplis_collection /* make_duplis */
};

/* OB_DUPLIVERTS */
typedef struct VertexDupliData {
  Mesh *me_eval;
  BMEditMesh *edit_mesh;
  int totvert;
  float (*orco)[3];
  bool use_rotation;

  const DupliContext *ctx;
  Object *inst_ob; /* object to instantiate (argument for vertex map callback) */
  float child_imat[4][4];
} VertexDupliData;

static void get_duplivert_transform(const float co[3],
                                    const short no[3],
                                    bool use_rotation,
                                    short axis,
                                    short upflag,
                                    float mat[4][4])
{
  float quat[4];
  const float size[3] = {1.0f, 1.0f, 1.0f};

  if (use_rotation) {
    /* construct rotation matrix from normals */
    float nor_f[3];
    nor_f[0] = (float)-no[0];
    nor_f[1] = (float)-no[1];
    nor_f[2] = (float)-no[2];
    vec_to_quat(quat, nor_f, axis, upflag);
  }
  else {
    unit_qt(quat);
  }

  loc_quat_size_to_mat4(mat, co, quat, size);
}

static void vertex_dupli(const VertexDupliData *vdd,
                         int index,
                         const float co[3],
                         const short no[3])
{
  Object *inst_ob = vdd->inst_ob;
  DupliObject *dob;
  float obmat[4][4], space_mat[4][4];

  /* obmat is transform to vertex */
  get_duplivert_transform(co, no, vdd->use_rotation, inst_ob->trackflag, inst_ob->upflag, obmat);
  /* make offset relative to inst_ob using relative child transform */
  mul_mat3_m4_v3((float(*)[4])vdd->child_imat, obmat[3]);
  /* apply obmat _after_ the local vertex transform */
  mul_m4_m4m4(obmat, inst_ob->obmat, obmat);

  /* space matrix is constructed by removing obmat transform,
   * this yields the worldspace transform for recursive duplis
   */
  mul_m4_m4m4(space_mat, obmat, inst_ob->imat);

  dob = make_dupli(vdd->ctx, vdd->inst_ob, obmat, index);

  if (vdd->orco) {
    copy_v3_v3(dob->orco, vdd->orco[index]);
  }

  /* recursion */
  make_recursive_duplis(vdd->ctx, vdd->inst_ob, space_mat, index);
}

static void make_child_duplis_verts(const DupliContext *ctx, void *userdata, Object *child)
{
  VertexDupliData *vdd = userdata;
  Mesh *me_eval = vdd->me_eval;

  vdd->inst_ob = child;
  invert_m4_m4(child->imat, child->obmat);
  /* relative transform from parent to child space */
  mul_m4_m4m4(vdd->child_imat, child->imat, ctx->object->obmat);

  const MVert *mvert = me_eval->mvert;
  const int *origindex = CustomData_get_layer(&me_eval->vdata, CD_ORIGINDEX);

  for (int i = 0, j = 0; i < me_eval->totvert; i++) {
    if (origindex == NULL || origindex[i] != ORIGINDEX_NONE) {
      vertex_dupli(vdd, j++, mvert[i].co, mvert[i].no);
    }
  }
}

static void make_duplis_verts(const DupliContext *ctx)
{
  Object *parent = ctx->object;
  VertexDupliData vdd;

  vdd.ctx = ctx;
  vdd.use_rotation = parent->transflag & OB_DUPLIROT;

  /* gather mesh info */
  {
    vdd.edit_mesh = BKE_editmesh_from_object(parent);

    /* We do not need any render-specific handling anymore, depsgraph takes care of that. */
    /* NOTE: Do direct access to the evaluated mesh: this function is used
     * during meta balls evaluation. But even without those all the objects
     * which are needed for correct instancing are already evaluated. */
    if (vdd.edit_mesh != NULL) {
      vdd.me_eval = vdd.edit_mesh->mesh_eval_cage;
    }
    else {
      vdd.me_eval = parent->runtime.mesh_eval;
    }

    if (vdd.me_eval == NULL) {
      return;
    }

    vdd.orco = CustomData_get_layer(&vdd.me_eval->vdata, CD_ORCO);
    vdd.totvert = vdd.me_eval->totvert;
  }

  make_child_duplis(ctx, &vdd, make_child_duplis_verts);

  vdd.me_eval = NULL;
}

static const DupliGenerator gen_dupli_verts = {
    OB_DUPLIVERTS,    /* type */
    make_duplis_verts /* make_duplis */
};

/* OB_DUPLIVERTS - FONT */
static Object *find_family_object(
    Main *bmain, const char *family, size_t family_len, unsigned int ch, GHash *family_gh)
{
  Object **ob_pt;
  Object *ob;
  void *ch_key = POINTER_FROM_UINT(ch);

  if ((ob_pt = (Object **)BLI_ghash_lookup_p(family_gh, ch_key))) {
    ob = *ob_pt;
  }
  else {
    char ch_utf8[7];
    size_t ch_utf8_len;

    ch_utf8_len = BLI_str_utf8_from_unicode(ch, ch_utf8);
    ch_utf8[ch_utf8_len] = '\0';
    ch_utf8_len += 1; /* compare with null terminator */

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (STREQLEN(ob->id.name + 2 + family_len, ch_utf8, ch_utf8_len)) {
        if (STREQLEN(ob->id.name + 2, family, family_len)) {
          break;
        }
      }
    }

    /* inserted value can be NULL, just to save searches in future */
    BLI_ghash_insert(family_gh, ch_key, ob);
  }

  return ob;
}

static void make_duplis_font(const DupliContext *ctx)
{
  Object *par = ctx->object;
  GHash *family_gh;
  Object *ob;
  Curve *cu;
  struct CharTrans *ct, *chartransdata = NULL;
  float vec[3], obmat[4][4], pmat[4][4], fsize, xof, yof;
  int text_len, a;
  size_t family_len;
  const wchar_t *text = NULL;
  bool text_free = false;

  /* font dupliverts not supported inside collections */
  if (ctx->collection) {
    return;
  }

  copy_m4_m4(pmat, par->obmat);

  /* in par the family name is stored, use this to find the other objects */

  BKE_vfont_to_curve_ex(
      par, par->data, FO_DUPLI, NULL, &text, &text_len, &text_free, &chartransdata);

  if (text == NULL || chartransdata == NULL) {
    return;
  }

  cu = par->data;
  fsize = cu->fsize;
  xof = cu->xof;
  yof = cu->yof;

  ct = chartransdata;

  /* cache result */
  family_len = strlen(cu->family);
  family_gh = BLI_ghash_int_new_ex(__func__, 256);

  /* advance matching BLI_strncpy_wchar_from_utf8 */
  for (a = 0; a < text_len; a++, ct++) {

    /* XXX That G.main is *really* ugly, but not sure what to do here...
     * Definitively don't think it would be safe to put back Main *bmain pointer
     * in DupliContext as done in 2.7x? */
    ob = find_family_object(G.main, cu->family, family_len, (unsigned int)text[a], family_gh);
    if (ob) {
      vec[0] = fsize * (ct->xof - xof);
      vec[1] = fsize * (ct->yof - yof);
      vec[2] = 0.0;

      mul_m4_v3(pmat, vec);

      copy_m4_m4(obmat, par->obmat);

      if (UNLIKELY(ct->rot != 0.0f)) {
        float rmat[4][4];

        zero_v3(obmat[3]);
        axis_angle_to_mat4_single(rmat, 'Z', -ct->rot);
        mul_m4_m4m4(obmat, obmat, rmat);
      }

      copy_v3_v3(obmat[3], vec);

      make_dupli(ctx, ob, obmat, a);
    }
  }

  if (text_free) {
    MEM_freeN((void *)text);
  }

  BLI_ghash_free(family_gh, NULL, NULL);

  MEM_freeN(chartransdata);
}

static const DupliGenerator gen_dupli_verts_font = {
    OB_DUPLIVERTS,   /* type */
    make_duplis_font /* make_duplis */
};

/* OB_DUPLIFACES */
typedef struct FaceDupliData {
  Mesh *me_eval;
  int totface;
  MPoly *mpoly;
  MLoop *mloop;
  MVert *mvert;
  float (*orco)[3];
  MLoopUV *mloopuv;
  bool use_scale;
} FaceDupliData;

static void get_dupliface_transform(
    MPoly *mpoly, MLoop *mloop, MVert *mvert, bool use_scale, float scale_fac, float mat[4][4])
{
  float loc[3], quat[4], scale, size[3];
  float f_no[3];

  /* location */
  BKE_mesh_calc_poly_center(mpoly, mloop, mvert, loc);
  /* rotation */
  {
    const float *v1, *v2, *v3;
    BKE_mesh_calc_poly_normal(mpoly, mloop, mvert, f_no);
    v1 = mvert[mloop[0].v].co;
    v2 = mvert[mloop[1].v].co;
    v3 = mvert[mloop[2].v].co;
    tri_to_quat_ex(quat, v1, v2, v3, f_no);
  }
  /* scale */
  if (use_scale) {
    float area = BKE_mesh_calc_poly_area(mpoly, mloop, mvert);
    scale = sqrtf(area) * scale_fac;
  }
  else {
    scale = 1.0f;
  }
  size[0] = size[1] = size[2] = scale;

  loc_quat_size_to_mat4(mat, loc, quat, size);
}

static void make_child_duplis_faces(const DupliContext *ctx, void *userdata, Object *inst_ob)
{
  FaceDupliData *fdd = userdata;
  MPoly *mpoly = fdd->mpoly, *mp;
  MLoop *mloop = fdd->mloop;
  MVert *mvert = fdd->mvert;
  float(*orco)[3] = fdd->orco;
  MLoopUV *mloopuv = fdd->mloopuv;
  int a, totface = fdd->totface;
  float child_imat[4][4];
  DupliObject *dob;

  invert_m4_m4(inst_ob->imat, inst_ob->obmat);
  /* relative transform from parent to child space */
  mul_m4_m4m4(child_imat, inst_ob->imat, ctx->object->obmat);

  for (a = 0, mp = mpoly; a < totface; a++, mp++) {
    MLoop *loopstart = mloop + mp->loopstart;
    float space_mat[4][4], obmat[4][4];

    if (UNLIKELY(mp->totloop < 3)) {
      continue;
    }

    /* obmat is transform to face */
    get_dupliface_transform(
        mp, loopstart, mvert, fdd->use_scale, ctx->object->instance_faces_scale, obmat);
    /* make offset relative to inst_ob using relative child transform */
    mul_mat3_m4_v3(child_imat, obmat[3]);

    /* XXX ugly hack to ensure same behavior as in master
     * this should not be needed, parentinv is not consistent
     * outside of parenting.
     */
    {
      float imat[3][3];
      copy_m3_m4(imat, inst_ob->parentinv);
      mul_m4_m3m4(obmat, imat, obmat);
    }

    /* apply obmat _after_ the local face transform */
    mul_m4_m4m4(obmat, inst_ob->obmat, obmat);

    /* space matrix is constructed by removing obmat transform,
     * this yields the worldspace transform for recursive duplis
     */
    mul_m4_m4m4(space_mat, obmat, inst_ob->imat);

    dob = make_dupli(ctx, inst_ob, obmat, a);

    const float w = 1.0f / (float)mp->totloop;
    if (orco) {
      for (int j = 0; j < mp->totloop; j++) {
        madd_v3_v3fl(dob->orco, orco[loopstart[j].v], w);
      }
    }
    if (mloopuv) {
      for (int j = 0; j < mp->totloop; j++) {
        madd_v2_v2fl(dob->uv, mloopuv[mp->loopstart + j].uv, w);
      }
    }

    /* recursion */
    make_recursive_duplis(ctx, inst_ob, space_mat, a);
  }
}

static void make_duplis_faces(const DupliContext *ctx)
{
  Object *parent = ctx->object;
  FaceDupliData fdd;

  fdd.use_scale = ((parent->transflag & OB_DUPLIFACES_SCALE) != 0);

  /* gather mesh info */
  {
    BMEditMesh *em = BKE_editmesh_from_object(parent);

    /* We do not need any render-smecific handling anymore, depsgraph takes care of that. */
    /* NOTE: Do direct access to the evaluated mesh: this function is used
     * during meta balls evaluation. But even without those all the objects
     * which are needed for correct instancing are already evaluated. */
    if (em != NULL) {
      fdd.me_eval = em->mesh_eval_cage;
    }
    else {
      fdd.me_eval = parent->runtime.mesh_eval;
    }

    if (fdd.me_eval == NULL) {
      return;
    }

    fdd.orco = CustomData_get_layer(&fdd.me_eval->vdata, CD_ORCO);
    const int uv_idx = CustomData_get_render_layer(&fdd.me_eval->ldata, CD_MLOOPUV);
    fdd.mloopuv = CustomData_get_layer_n(&fdd.me_eval->ldata, CD_MLOOPUV, uv_idx);

    fdd.totface = fdd.me_eval->totpoly;
    fdd.mpoly = fdd.me_eval->mpoly;
    fdd.mloop = fdd.me_eval->mloop;
    fdd.mvert = fdd.me_eval->mvert;
  }

  make_child_duplis(ctx, &fdd, make_child_duplis_faces);

  fdd.me_eval = NULL;
}

static const DupliGenerator gen_dupli_faces = {
    OB_DUPLIFACES,    /* type */
    make_duplis_faces /* make_duplis */
};

/* OB_DUPLIPARTS */
static void make_duplis_particle_system(const DupliContext *ctx, ParticleSystem *psys)
{
  Scene *scene = ctx->scene;
  Object *par = ctx->object;
  eEvaluationMode mode = DEG_get_mode(ctx->depsgraph);
  bool for_render = mode == DAG_EVAL_RENDER;

  Object *ob = NULL, **oblist = NULL;
  DupliObject *dob;
  ParticleDupliWeight *dw;
  ParticleSettings *part;
  ParticleData *pa;
  ChildParticle *cpa = NULL;
  ParticleKey state;
  ParticleCacheKey *cache;
  float ctime, scale = 1.0f;
  float tmat[4][4], mat[4][4], pamat[4][4], size = 0.0;
  int a, b, hair = 0;
  int totpart, totchild;

  int no_draw_flag = PARS_UNEXIST;

  if (psys == NULL) {
    return;
  }

  part = psys->part;

  if (part == NULL) {
    return;
  }

  if (!psys_check_enabled(par, psys, for_render)) {
    return;
  }

  if (!for_render) {
    no_draw_flag |= PARS_NO_DISP;
  }

  ctime = DEG_get_ctime(
      ctx->depsgraph); /* NOTE: in old animsys, used parent object's timeoffset... */

  totpart = psys->totpart;
  totchild = psys->totchild;

  if ((for_render || part->draw_as == PART_DRAW_REND) &&
      ELEM(part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
    ParticleSimulationData sim = {NULL};
    sim.depsgraph = ctx->depsgraph;
    sim.scene = scene;
    sim.ob = par;
    sim.psys = psys;
    sim.psmd = psys_get_modifier(par, psys);
    /* make sure emitter imat is in global coordinates instead of render view coordinates */
    invert_m4_m4(par->imat, par->obmat);

    /* first check for loops (particle system object used as dupli object) */
    if (part->ren_as == PART_DRAW_OB) {
      if (ELEM(part->instance_object, NULL, par)) {
        return;
      }
    }
    else { /*PART_DRAW_GR */
      if (part->instance_collection == NULL) {
        return;
      }

      const ListBase dup_collection_objects = BKE_collection_object_cache_get(
          part->instance_collection);
      if (BLI_listbase_is_empty(&dup_collection_objects)) {
        return;
      }

      if (BLI_findptr(&dup_collection_objects, par, offsetof(Base, object))) {
        return;
      }
    }

    /* if we have a hair particle system, use the path cache */
    if (part->type == PART_HAIR) {
      if (psys->flag & PSYS_HAIR_DONE) {
        hair = (totchild == 0 || psys->childcache) && psys->pathcache;
      }
      if (!hair) {
        return;
      }

      /* we use cache, update totchild according to cached data */
      totchild = psys->totchildcache;
      totpart = psys->totcached;
    }

    RNG *rng = BLI_rng_new_srandom(31415926u + (unsigned int)psys->seed);

    psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

    /* gather list of objects or single object */
    int totcollection = 0;

    if (part->ren_as == PART_DRAW_GR) {
      if (part->draw & PART_DRAW_COUNT_GR) {
        psys_find_group_weights(part);

        for (dw = part->instance_weights.first; dw; dw = dw->next) {
          FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (
              part->instance_collection, object, mode) {
            if (dw->ob == object) {
              totcollection += dw->count;
              break;
            }
          }
          FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
        }
      }
      else {
        FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (
            part->instance_collection, object, mode) {
          (void)object;
          totcollection++;
        }
        FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
      }

      oblist = MEM_callocN((size_t)totcollection * sizeof(Object *), "dupcollection object list");

      if (part->draw & PART_DRAW_COUNT_GR) {
        a = 0;
        for (dw = part->instance_weights.first; dw; dw = dw->next) {
          FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (
              part->instance_collection, object, mode) {
            if (dw->ob == object) {
              for (b = 0; b < dw->count; b++, a++) {
                oblist[a] = dw->ob;
              }
              break;
            }
          }
          FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
        }
      }
      else {
        a = 0;
        FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (
            part->instance_collection, object, mode) {
          oblist[a] = object;
          a++;
        }
        FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
      }
    }
    else {
      ob = part->instance_object;
    }

    if (totchild == 0 || part->draw & PART_DRAW_PARENT) {
      a = 0;
    }
    else {
      a = totpart;
    }

    for (pa = psys->particles; a < totpart + totchild; a++, pa++) {
      if (a < totpart) {
        /* handle parent particle */
        if (pa->flag & no_draw_flag) {
          continue;
        }

        /* pa_num = pa->num; */ /* UNUSED */
        size = pa->size;
      }
      else {
        /* handle child particle */
        cpa = &psys->child[a - totpart];

        /* pa_num = a; */ /* UNUSED */
        size = psys_get_child_size(psys, cpa, ctime, NULL);
      }

      /* some hair paths might be non-existent so they can't be used for duplication */
      if (hair && psys->pathcache &&
          ((a < totpart && psys->pathcache[a]->segments < 0) ||
           (a >= totpart && psys->childcache[a - totpart]->segments < 0))) {
        continue;
      }

      if (part->ren_as == PART_DRAW_GR) {
        /* prevent divide by zero below [#28336] */
        if (totcollection == 0) {
          continue;
        }

        /* for collections, pick the object based on settings */
        if (part->draw & PART_DRAW_RAND_GR) {
          b = BLI_rng_get_int(rng) % totcollection;
        }
        else {
          b = a % totcollection;
        }

        ob = oblist[b];
      }

      if (hair) {
        /* hair we handle separate and compute transform based on hair keys */
        if (a < totpart) {
          cache = psys->pathcache[a];
          psys_get_dupli_path_transform(&sim, pa, NULL, cache, pamat, &scale);
        }
        else {
          cache = psys->childcache[a - totpart];
          psys_get_dupli_path_transform(&sim, NULL, cpa, cache, pamat, &scale);
        }

        copy_v3_v3(pamat[3], cache->co);
        pamat[3][3] = 1.0f;
      }
      else {
        /* first key */
        state.time = ctime;
        if (psys_get_particle_state(&sim, a, &state, 0) == 0) {
          continue;
        }
        else {
          float tquat[4];
          normalize_qt_qt(tquat, state.rot);
          quat_to_mat4(pamat, tquat);
          copy_v3_v3(pamat[3], state.co);
          pamat[3][3] = 1.0f;
        }
      }

      if (part->ren_as == PART_DRAW_GR && psys->part->draw & PART_DRAW_WHOLE_GR) {
        b = 0;
        FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (
            part->instance_collection, object, mode) {
          copy_m4_m4(tmat, oblist[b]->obmat);

          /* apply particle scale */
          mul_mat3_m4_fl(tmat, size * scale);
          mul_v3_fl(tmat[3], size * scale);

          /* collection dupli offset, should apply after everything else */
          if (!is_zero_v3(part->instance_collection->instance_offset)) {
            sub_v3_v3(tmat[3], part->instance_collection->instance_offset);
          }

          /* individual particle transform */
          mul_m4_m4m4(mat, pamat, tmat);

          dob = make_dupli(ctx, object, mat, a);
          dob->particle_system = psys;

          psys_get_dupli_texture(psys, part, sim.psmd, pa, cpa, dob->uv, dob->orco);

          b++;
        }
        FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
      }
      else {
        float obmat[4][4];
        copy_m4_m4(obmat, ob->obmat);

        float vec[3];
        copy_v3_v3(vec, obmat[3]);
        zero_v3(obmat[3]);

        /* Particle rotation uses x-axis as the aligned axis,
         * so pre-rotate the object accordingly. */
        if ((part->draw & PART_DRAW_ROTATE_OB) == 0) {
          float xvec[3], q[4], size_mat[4][4], original_size[3];

          mat4_to_size(original_size, obmat);
          size_to_mat4(size_mat, original_size);

          xvec[0] = -1.f;
          xvec[1] = xvec[2] = 0;
          vec_to_quat(q, xvec, ob->trackflag, ob->upflag);
          quat_to_mat4(obmat, q);
          obmat[3][3] = 1.0f;

          /* add scaling if requested */
          if ((part->draw & PART_DRAW_NO_SCALE_OB) == 0) {
            mul_m4_m4m4(obmat, obmat, size_mat);
          }
        }
        else if (part->draw & PART_DRAW_NO_SCALE_OB) {
          /* remove scaling */
          float size_mat[4][4], original_size[3];

          mat4_to_size(original_size, obmat);
          size_to_mat4(size_mat, original_size);
          invert_m4(size_mat);

          mul_m4_m4m4(obmat, obmat, size_mat);
        }

        mul_m4_m4m4(tmat, pamat, obmat);
        mul_mat3_m4_fl(tmat, size * scale);

        copy_m4_m4(mat, tmat);

        if (part->draw & PART_DRAW_GLOBAL_OB) {
          add_v3_v3v3(mat[3], mat[3], vec);
        }

        dob = make_dupli(ctx, ob, mat, a);
        dob->particle_system = psys;
        psys_get_dupli_texture(psys, part, sim.psmd, pa, cpa, dob->uv, dob->orco);
      }
    }

    BLI_rng_free(rng);
  }

  /* clean up */
  if (oblist) {
    MEM_freeN(oblist);
  }

  if (psys->lattice_deform_data) {
    end_latt_deform(psys->lattice_deform_data);
    psys->lattice_deform_data = NULL;
  }
}

static void make_duplis_particles(const DupliContext *ctx)
{
  ParticleSystem *psys;
  int psysid;

  /* particle system take up one level in id, the particles another */
  for (psys = ctx->object->particlesystem.first, psysid = 0; psys; psys = psys->next, psysid++) {
    /* particles create one more level for persistent psys index */
    DupliContext pctx;
    copy_dupli_context(&pctx, ctx, ctx->object, NULL, psysid);
    make_duplis_particle_system(&pctx, psys);
  }
}

static const DupliGenerator gen_dupli_particles = {
    OB_DUPLIPARTS,        /* type */
    make_duplis_particles /* make_duplis */
};

/* ------------- */

/* select dupli generator from given context */
static const DupliGenerator *get_dupli_generator(const DupliContext *ctx)
{
  int transflag = ctx->object->transflag;
  int restrictflag = ctx->object->restrictflag;

  if ((transflag & OB_DUPLI) == 0) {
    return NULL;
  }

  /* Should the dupli's be generated for this object? - Respect restrict flags */
  if (DEG_get_mode(ctx->depsgraph) == DAG_EVAL_RENDER ? (restrictflag & OB_RESTRICT_RENDER) :
                                                        (restrictflag & OB_RESTRICT_VIEW)) {
    return NULL;
  }

  if (transflag & OB_DUPLIPARTS) {
    return &gen_dupli_particles;
  }
  else if (transflag & OB_DUPLIVERTS) {
    if (ctx->object->type == OB_MESH) {
      return &gen_dupli_verts;
    }
    else if (ctx->object->type == OB_FONT) {
      return &gen_dupli_verts_font;
    }
  }
  else if (transflag & OB_DUPLIFACES) {
    if (ctx->object->type == OB_MESH) {
      return &gen_dupli_faces;
    }
  }
  else if (transflag & OB_DUPLICOLLECTION) {
    return &gen_dupli_collection;
  }

  return NULL;
}

/* ---- ListBase dupli container implementation ---- */

/* Returns a list of DupliObject */
ListBase *object_duplilist(Depsgraph *depsgraph, Scene *sce, Object *ob)
{
  ListBase *duplilist = MEM_callocN(sizeof(ListBase), "duplilist");
  DupliContext ctx;
  init_context(&ctx, depsgraph, sce, ob, NULL);
  if (ctx.gen) {
    ctx.duplilist = duplilist;
    ctx.gen->make_duplis(&ctx);
  }

  return duplilist;
}

void free_object_duplilist(ListBase *lb)
{
  BLI_freelistN(lb);
  MEM_freeN(lb);
}
