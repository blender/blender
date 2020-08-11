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
#include <stddef.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLI_alloca.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"

#include "BKE_collection.h"
#include "BKE_duplilist.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
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

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "BLI_hash.h"
#include "BLI_strict_flags.h"

/* -------------------------------------------------------------------- */
/** \name Internal Duplicate Context
 * \{ */

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

/**
 * Create initial context for root object.
 */
static void init_context(DupliContext *r_ctx,
                         Depsgraph *depsgraph,
                         Scene *scene,
                         Object *ob,
                         const float space_mat[4][4])
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

/**
 * Create sub-context for recursive duplis.
 */
static void copy_dupli_context(
    DupliContext *r_ctx, const DupliContext *ctx, Object *ob, const float mat[4][4], int index)
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

/**
 * Generate a dupli instance.
 *
 * \param mat: is transform of the object relative to current context (including #Object.obmat).
 */
static DupliObject *make_dupli(const DupliContext *ctx,
                               Object *ob,
                               const float mat[4][4],
                               int index)
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
    for (i = 0; i < MAX_DUPLI_RECUR; i++) {
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

/**
 * Recursive dupli objects.
 *
 * \param space_mat: is the local dupli space (excluding dupli #Object.obmat).
 */
static void make_recursive_duplis(const DupliContext *ctx,
                                  Object *ob,
                                  const float space_mat[4][4],
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Child Duplicates (Used by Other Functions)
 * \{ */

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

/**
 * Create duplis from every child in scene or collection.
 */
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Data Access Utilities
 * \{ */

static Mesh *mesh_data_from_duplicator_object(Object *ob,
                                              BMEditMesh **r_em,
                                              const float (**r_vert_coords)[3],
                                              const float (**r_vert_normals)[3])
{
  /* Gather mesh info. */
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  Mesh *me_eval;

  *r_em = NULL;
  *r_vert_coords = NULL;
  if (r_vert_normals != NULL) {
    *r_vert_normals = NULL;
  }

  /* We do not need any render-specific handling anymore, depsgraph takes care of that. */
  /* NOTE: Do direct access to the evaluated mesh: this function is used
   * during meta balls evaluation. But even without those all the objects
   * which are needed for correct instancing are already evaluated. */
  if (em != NULL) {
    /* Note that this will only show deformation if #eModifierMode_OnCage is enabled.
     * We could change this but it matches 2.7x behavior. */
    me_eval = em->mesh_eval_cage;
    if ((me_eval == NULL) || (me_eval->runtime.wrapper_type == ME_WRAPPER_TYPE_BMESH)) {
      *r_em = em;
      if (me_eval != NULL) {
        EditMeshData *emd = me_eval->runtime.edit_data;
        if ((emd != NULL) && (emd->vertexCos != NULL)) {
          *r_vert_coords = emd->vertexCos;
          if (r_vert_normals != NULL) {
            BKE_editmesh_cache_ensure_vert_normals(em, emd);
            *r_vert_normals = emd->vertexNos;
          }
        }
      }
    }
  }
  else {
    me_eval = BKE_object_get_evaluated_mesh(ob);
  }
  return me_eval;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dupli-Collection Implementation (#OB_DUPLICOLLECTION)
 * \{ */

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dupli-Vertices Implementation (#OB_DUPLIVERTS for Geometry)
 * \{ */

typedef struct VertexDupliData_Mesh {
  Mesh *me_eval;

  int totvert;
  MVert *mvert;

  float (*orco)[3];

  bool use_rotation;
} VertexDupliData_Mesh;

typedef struct VertexDupliData_BMesh {
  BMEditMesh *em;

  /* Can be NULL. */
  const float (*vert_coords)[3];
  const float (*vert_normals)[3];

  bool has_orco;
  bool use_rotation;
} VertexDupliData_BMesh;

static void get_duplivert_transform(const float co[3],
                                    const float no[3],
                                    const bool use_rotation,
                                    short axis,
                                    short upflag,
                                    float r_mat[4][4])
{
  float quat[4];
  const float size[3] = {1.0f, 1.0f, 1.0f};

  if (use_rotation) {
    /* Construct rotation matrix from normals. */
    float no_flip[3];
    negate_v3_v3(no_flip, no);
    vec_to_quat(quat, no_flip, axis, upflag);
  }
  else {
    unit_qt(quat);
  }

  loc_quat_size_to_mat4(r_mat, co, quat, size);
}

/**
 * \param no: The direction, doesn't need to be normalized.
 */
static DupliObject *vertex_dupli(const DupliContext *ctx,
                                 Object *child,
                                 const float child_imat[4][4],
                                 int index,
                                 float space_mat[4][4])
{
  float obmat[4][4];

  /* Make offset relative to child using relative child transform. */
  mul_mat3_m4_v3(child_imat, space_mat[3]);

  /* Apply `obmat` _after_ the local vertex transform. */
  mul_m4_m4m4(obmat, child->obmat, space_mat);

  DupliObject *dob = make_dupli(ctx, child, obmat, index);

  /* Recursion. */
  make_recursive_duplis(ctx, child, space_mat, index);

  return dob;
}

static void make_child_duplis_verts_from_mesh(const DupliContext *ctx,
                                              void *userdata,
                                              Object *child)
{
  VertexDupliData_Mesh *vdd = userdata;

  const MVert *mvert = vdd->mvert;
  const int totvert = vdd->totvert;

  invert_m4_m4(child->imat, child->obmat);
  /* Relative transform from parent to child space. */
  float child_imat[4][4];
  mul_m4_m4m4(child_imat, child->imat, ctx->object->obmat);

  const MVert *mv = mvert;
  for (int i = 0; i < totvert; i++, mv++) {
    const float no[3] = {mv->no[0], mv->no[1], mv->no[2]};
    /* space_mat is transform to vertex. */
    float space_mat[4][4];
    get_duplivert_transform(
        mv->co, no, vdd->use_rotation, child->trackflag, child->upflag, space_mat);
    DupliObject *dob = vertex_dupli(ctx, child, child_imat, i, space_mat);
    if (vdd->orco) {
      copy_v3_v3(dob->orco, vdd->orco[i]);
    }
  }
}

static void make_child_duplis_verts_from_bmesh(const DupliContext *ctx,
                                               void *userdata,
                                               Object *child)
{
  VertexDupliData_BMesh *vdd = userdata;
  BMEditMesh *em = vdd->em;

  invert_m4_m4(child->imat, child->obmat);
  /* Relative transform from parent to child space. */
  float child_imat[4][4];
  mul_m4_m4m4(child_imat, child->imat, ctx->object->obmat);

  BMVert *v;
  BMIter iter;
  int i;

  const float(*vert_coords)[3] = vdd->vert_coords;
  const float(*vert_normals)[3] = vdd->vert_normals;

  BM_ITER_MESH_INDEX (v, &iter, em->bm, BM_VERTS_OF_MESH, i) {
    const float *co, *no;
    if (vert_coords != NULL) {
      co = vert_coords[i];
      no = vert_normals[i];
    }
    else {
      co = v->co;
      no = v->no;
    }

    /* space_mat is transform to vertex. */
    float space_mat[4][4];
    get_duplivert_transform(co, no, vdd->use_rotation, child->trackflag, child->upflag, space_mat);
    DupliObject *dob = vertex_dupli(ctx, child, child_imat, i, space_mat);
    if (vdd->has_orco) {
      copy_v3_v3(dob->orco, v->co);
    }
  }
}

static void make_duplis_verts(const DupliContext *ctx)
{
  Object *parent = ctx->object;

  const bool use_rotation = parent->transflag & OB_DUPLIROT;

  /* Gather mesh info. */
  BMEditMesh *em = NULL;
  const float(*vert_coords)[3] = NULL;
  const float(*vert_normals)[3] = NULL;
  Mesh *me_eval = mesh_data_from_duplicator_object(parent, &em, &vert_coords, &vert_normals);
  if (em == NULL && me_eval == NULL) {
    return;
  }

  if (em != NULL) {
    VertexDupliData_BMesh vdd = {
        .em = em,
        .vert_coords = vert_coords,
        .vert_normals = vert_normals,
        .has_orco = (vert_coords != NULL),
        .use_rotation = use_rotation,
    };
    make_child_duplis(ctx, &vdd, make_child_duplis_verts_from_bmesh);
  }
  else {
    VertexDupliData_Mesh vdd = {
        .use_rotation = use_rotation,
    };
    vdd.orco = CustomData_get_layer(&me_eval->vdata, CD_ORCO);
    vdd.totvert = me_eval->totvert;
    vdd.mvert = me_eval->mvert;
    make_child_duplis(ctx, &vdd, make_child_duplis_verts_from_mesh);
  }
}

static const DupliGenerator gen_dupli_verts = {
    OB_DUPLIVERTS,    /* type */
    make_duplis_verts /* make_duplis */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dupli-Vertices Implementation (#OB_DUPLIVERTS for 3D Text)
 * \{ */

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
  const char32_t *text = NULL;
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

  /* Safety check even if it might fail badly when called for original object. */
  const bool is_eval_curve = DEG_is_evaluated_id(&cu->id);

  /* Advance matching BLI_str_utf8_as_utf32. */
  for (a = 0; a < text_len; a++, ct++) {

    /* XXX That G.main is *really* ugly, but not sure what to do here...
     * Definitively don't think it would be safe to put back Main *bmain pointer
     * in DupliContext as done in 2.7x? */
    ob = find_family_object(G.main, cu->family, family_len, (unsigned int)text[a], family_gh);

    if (is_eval_curve) {
      /* Workaround for the above hack. */
      ob = DEG_get_evaluated_object(ctx->depsgraph, ob);
    }

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dupli-Vertices Implementation (#OB_DUPLIVERTS for #PointCloud)
 * \{ */

static void make_child_duplis_pointcloud(const DupliContext *ctx,
                                         void *UNUSED(userdata),
                                         Object *child)
{
  const Object *parent = ctx->object;
  const PointCloud *pointcloud = parent->data;
  const float(*co)[3] = pointcloud->co;
  const float *radius = pointcloud->radius;
  const float(*rotation)[4] = NULL; /* TODO: add optional rotation attribute. */
  const float(*orco)[3] = NULL;     /* TODO: add optional texture coordinate attribute. */

  /* Relative transform from parent to child space. */
  float child_imat[4][4];
  mul_m4_m4m4(child_imat, child->imat, parent->obmat);

  for (int i = 0; i < pointcloud->totpoint; i++) {
    /* Transform matrix from point position, radius and rotation. */
    float quat[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float size[3] = {1.0f, 1.0f, 1.0f};
    if (radius) {
      copy_v3_fl(size, radius[i]);
    }
    if (rotation) {
      copy_v4_v4(quat, rotation[i]);
    }

    float space_mat[4][4];
    loc_quat_size_to_mat4(space_mat, co[i], quat, size);

    /* Make offset relative to child object using relative child transform,
     * and apply object matrix after local vertex transform. */
    mul_mat3_m4_v3(child_imat, space_mat[3]);

    /* Create dupli object. */
    float obmat[4][4];
    mul_m4_m4m4(obmat, child->obmat, space_mat);
    DupliObject *dob = make_dupli(ctx, child, obmat, i);
    if (orco) {
      copy_v3_v3(dob->orco, orco[i]);
    }

    /* Recursion. */
    make_recursive_duplis(ctx, child, space_mat, i);
  }
}

static void make_duplis_pointcloud(const DupliContext *ctx)
{
  make_child_duplis(ctx, NULL, make_child_duplis_pointcloud);
}

static const DupliGenerator gen_dupli_verts_pointcloud = {
    OB_DUPLIVERTS,         /* type */
    make_duplis_pointcloud /* make_duplis */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dupli-Faces Implementation (#OB_DUPLIFACES)
 * \{ */

typedef struct FaceDupliData_Mesh {
  Mesh *me_eval;

  int totface;
  MPoly *mpoly;
  MLoop *mloop;
  MVert *mvert;

  float (*orco)[3];
  MLoopUV *mloopuv;
  bool use_scale;
} FaceDupliData_Mesh;

typedef struct FaceDupliData_BMesh {
  BMEditMesh *em;

  bool has_orco, has_uvs;
  int cd_loop_uv_offset;
  /* Can be NULL. */
  const float (*vert_coords)[3];
  bool use_scale;
} FaceDupliData_BMesh;

static void get_dupliface_transform_from_coords(const float coords[][3],
                                                const int coords_len,
                                                const bool use_scale,
                                                const float scale_fac,
                                                float r_mat[4][4])
{
  float loc[3], quat[4], scale, size[3];

  /* location */
  {
    const float w = 1.0f / (float)coords_len;
    zero_v3(loc);
    for (int i = 0; i < coords_len; i++) {
      madd_v3_v3fl(loc, coords[i], w);
    }
  }
  /* rotation */
  {
    float f_no[3];
    cross_poly_v3(f_no, coords, (uint)coords_len);
    normalize_v3(f_no);
    tri_to_quat_ex(quat, coords[0], coords[1], coords[2], f_no);
  }
  /* scale */
  if (use_scale) {
    const float area = area_poly_v3(coords, (uint)coords_len);
    scale = sqrtf(area) * scale_fac;
  }
  else {
    scale = 1.0f;
  }
  size[0] = size[1] = size[2] = scale;

  loc_quat_size_to_mat4(r_mat, loc, quat, size);
}

static void get_dupliface_transform_from_mesh(const MPoly *mpoly,
                                              const MLoop *mloopstart,
                                              const MVert *mvert,
                                              const bool use_scale,
                                              const float scale_fac,
                                              float r_mat[4][4])
{
  const MLoop *l_iter = mloopstart;
  float(*coords)[3] = BLI_array_alloca(coords, (size_t)mpoly->totloop);
  for (int i = 0; i < mpoly->totloop; i++, l_iter++) {
    copy_v3_v3(coords[i], mvert[l_iter->v].co);
  }
  get_dupliface_transform_from_coords(coords, mpoly->totloop, use_scale, scale_fac, r_mat);
}

static void get_dupliface_transform_from_bmesh(BMFace *f,
                                               const float (*vert_coords)[3],
                                               const bool use_scale,
                                               const float scale_fac,
                                               float r_mat[4][4])
{
  BMLoop *l_first, *l_iter;
  float(*coords)[3] = BLI_array_alloca(coords, (size_t)f->len);
  int i = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  if (vert_coords != NULL) {
    do {
      copy_v3_v3(coords[i], vert_coords[BM_elem_index_get(l_iter->v)]);
    } while ((void)(i++), (l_iter = l_iter->next) != l_first);
  }
  else {
    do {
      copy_v3_v3(coords[i], l_iter->v->co);
    } while ((void)(i++), (l_iter = l_iter->next) != l_first);
  }
  get_dupliface_transform_from_coords(coords, f->len, use_scale, scale_fac, r_mat);
}

static DupliObject *face_dupli(const DupliContext *ctx,
                               Object *child,
                               const float child_imat[4][4],
                               const int index,
                               float obmat[4][4])
{
  float space_mat[4][4];

  /* Make offset relative to child using relative child transform. */
  mul_mat3_m4_v3(child_imat, obmat[3]);

  /* XXX ugly hack to ensure same behavior as in master this should not be needed,
   * #Object.parentinv is not consistent outside of parenting. */
  {
    float imat[3][3];
    copy_m3_m4(imat, child->parentinv);
    mul_m4_m3m4(obmat, imat, obmat);
  }

  /* Apply `obmat` _after_ the local face transform. */
  mul_m4_m4m4(obmat, child->obmat, obmat);

  /* Space matrix is constructed by removing \a obmat transform,
   * this yields the world-space transform for recursive duplis. */
  mul_m4_m4m4(space_mat, obmat, child->imat);

  DupliObject *dob = make_dupli(ctx, child, obmat, index);

  /* Recursion. */
  make_recursive_duplis(ctx, child, space_mat, index);

  return dob;
}

static void make_child_duplis_faces_from_mesh(const DupliContext *ctx,
                                              void *userdata,
                                              Object *child)
{
  FaceDupliData_Mesh *fdd = userdata;
  const MPoly *mpoly = fdd->mpoly, *mp;
  const MLoop *mloop = fdd->mloop;
  const MVert *mvert = fdd->mvert;
  const float(*orco)[3] = fdd->orco;
  const MLoopUV *mloopuv = fdd->mloopuv;
  const int totface = fdd->totface;
  int a;

  float child_imat[4][4];

  invert_m4_m4(child->imat, child->obmat);
  /* Relative transform from parent to child space. */
  mul_m4_m4m4(child_imat, child->imat, ctx->object->obmat);
  const float scale_fac = ctx->object->instance_faces_scale;

  for (a = 0, mp = mpoly; a < totface; a++, mp++) {
    const MLoop *loopstart = mloop + mp->loopstart;
    float obmat[4][4];

    /* `obmat` is transform to face. */
    get_dupliface_transform_from_mesh(mp, loopstart, mvert, fdd->use_scale, scale_fac, obmat);
    DupliObject *dob = face_dupli(ctx, child, child_imat, a, obmat);

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
  }
}

static void make_child_duplis_faces_from_bmesh(const DupliContext *ctx,
                                               void *userdata,
                                               Object *child)
{
  FaceDupliData_BMesh *fdd = userdata;
  BMEditMesh *em = fdd->em;
  float child_imat[4][4];
  int a;
  BMFace *f;
  BMIter iter;

  const float(*vert_coords)[3] = fdd->vert_coords;

  BLI_assert((vert_coords == NULL) || (em->bm->elem_index_dirty & BM_VERT) == 0);

  invert_m4_m4(child->imat, child->obmat);
  /* Relative transform from parent to child space. */
  mul_m4_m4m4(child_imat, child->imat, ctx->object->obmat);
  const float scale_fac = ctx->object->instance_faces_scale;

  BM_ITER_MESH_INDEX (f, &iter, em->bm, BM_FACES_OF_MESH, a) {
    float obmat[4][4];

    /* `obmat` is transform to face. */
    get_dupliface_transform_from_bmesh(f, vert_coords, fdd->use_scale, scale_fac, obmat);
    DupliObject *dob = face_dupli(ctx, child, child_imat, a, obmat);

    if (fdd->has_orco) {
      const float w = 1.0f / (float)f->len;
      BMLoop *l_first, *l_iter;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        madd_v3_v3fl(dob->orco, l_iter->v->co, w);
      } while ((l_iter = l_iter->next) != l_first);
    }
    if (fdd->has_uvs) {
      BM_face_uv_calc_center_median(f, fdd->cd_loop_uv_offset, dob->uv);
    }
  }
}

static void make_duplis_faces(const DupliContext *ctx)
{
  Object *parent = ctx->object;
  const bool use_scale = ((parent->transflag & OB_DUPLIFACES_SCALE) != 0);

  /* Gather mesh info. */
  BMEditMesh *em = NULL;
  const float(*vert_coords)[3] = NULL;
  Mesh *me_eval = mesh_data_from_duplicator_object(parent, &em, &vert_coords, NULL);
  if (em == NULL && me_eval == NULL) {
    return;
  }

  if (em != NULL) {
    FaceDupliData_BMesh fdd = {
        .em = em,
        .use_scale = use_scale,
        .has_orco = (vert_coords != NULL),
        .vert_coords = vert_coords,
    };
    const int uv_idx = CustomData_get_render_layer(&em->bm->ldata, CD_MLOOPUV);
    if (uv_idx != -1) {
      fdd.has_uvs = true;
      fdd.cd_loop_uv_offset = CustomData_get_n_offset(&em->bm->ldata, CD_MLOOPUV, uv_idx);
    }
    make_child_duplis(ctx, &fdd, make_child_duplis_faces_from_bmesh);
  }
  else {
    FaceDupliData_Mesh fdd = {
        .use_scale = use_scale,
        .me_eval = me_eval,
    };
    fdd.orco = CustomData_get_layer(&fdd.me_eval->vdata, CD_ORCO);
    const int uv_idx = CustomData_get_render_layer(&fdd.me_eval->ldata, CD_MLOOPUV);
    fdd.mloopuv = CustomData_get_layer_n(&fdd.me_eval->ldata, CD_MLOOPUV, uv_idx);

    fdd.totface = me_eval->totpoly;
    fdd.mpoly = me_eval->mpoly;
    fdd.mloop = me_eval->mloop;
    fdd.mvert = me_eval->mvert;
    make_child_duplis(ctx, &fdd, make_child_duplis_faces_from_mesh);
  }
}

static const DupliGenerator gen_dupli_faces = {
    OB_DUPLIFACES,    /* type */
    make_duplis_faces /* make_duplis */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dupli-Particles Implementation (#OB_DUPLIPARTS)
 * \{ */

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

  /* NOTE: in old animsys, used parent object's timeoffset... */
  ctime = DEG_get_ctime(ctx->depsgraph);

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

    const bool use_whole_collection = part->draw & PART_DRAW_WHOLE_GR;
    const bool use_collection_count = part->draw & PART_DRAW_COUNT_GR && !use_whole_collection;
    if (part->ren_as == PART_DRAW_GR) {
      if (use_collection_count) {
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

      if (use_collection_count) {
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
        if (part->draw & PART_DRAW_RAND_GR && !use_whole_collection) {
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

        float tquat[4];
        normalize_qt_qt(tquat, state.rot);
        quat_to_mat4(pamat, tquat);
        copy_v3_v3(pamat[3], state.co);
        pamat[3][3] = 1.0f;
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
    BKE_lattice_deform_data_destroy(psys->lattice_deform_data);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dupli-Generator Selector For The Given Context
 * \{ */

static const DupliGenerator *get_dupli_generator(const DupliContext *ctx)
{
  int transflag = ctx->object->transflag;
  int restrictflag = ctx->object->restrictflag;

  if ((transflag & OB_DUPLI) == 0) {
    return NULL;
  }

  /* Should the dupli's be generated for this object? - Respect restrict flags */
  if (DEG_get_mode(ctx->depsgraph) == DAG_EVAL_RENDER ? (restrictflag & OB_RESTRICT_RENDER) :
                                                        (restrictflag & OB_RESTRICT_VIEWPORT)) {
    return NULL;
  }

  if (transflag & OB_DUPLIPARTS) {
    return &gen_dupli_particles;
  }
  if (transflag & OB_DUPLIVERTS) {
    if (ctx->object->type == OB_MESH) {
      return &gen_dupli_verts;
    }
    if (ctx->object->type == OB_FONT) {
      return &gen_dupli_verts_font;
    }
    if (ctx->object->type == OB_POINTCLOUD) {
      return &gen_dupli_verts_pointcloud;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dupli-Container Implementation
 * \{ */

/**
 * \return a #ListBase of #DupliObject.
 */
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

/** \} */
