#if 1
#  include "DNA_key_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_modifier_types.h"
#  include "DNA_object_types.h"

#  include "MEM_guardedalloc.h"

#  include "BLI_alloca.h"
#  include "BLI_compiler_attrs.h"
#  include "BLI_listbase.h"
#  include "BLI_math_vector.h"
#  include "BLI_task.h"
#  include "BLI_threads.h"

#  include "BKE_customdata.h"
#  include "BKE_mesh.h"
#  include "BKE_mesh_runtime.h"
#  include "BKE_multires.h"

#  include "BKE_key.h"
#  include "BKE_main.h"

#  include "DEG_depsgraph_query.h"

#  include "bmesh.h"
#  include "intern/bmesh_private.h" /* For element checking. */

#  include "BLI_task.h"

#  include "atomic_ops.h"

#  define ECHUNK 512
#  define VCHUNK 512
#  define FCHUNK 512
#  define LCHUNK 1024

typedef struct BMThreadData {
  BMesh *bm;
  Object *ob;
  const Mesh *me;

  struct BMeshFromMeshParams *params;

  void **vdata, **edata, **ldata, **fdata;
  int totdv, totde, totdl, totdf;
  int vsize, esize, lsize, fsize;

  int vchunk, echunk, lchunk, fchunk;

  BMVert **verts;
  BMEdge **edges;
  BMLoop **loops;
  BMFace **faces;

  float (**shape_key_table)[3];
  int tot_shape_keys;

  int cd_vert_bweight;
  int cd_edge_bweight;
  int cd_crease;

  int cdvsize, cdesize, cdlsize, cdfsize;

  // chunk sizes
  int totcv, totce, totcl, totcf;
} BMThreadData;

#  define ELEM_NEXT(type, ptr, size) ((type *)(((char *)ptr) + size))

static void bm_vert_task(void *__restrict userdata,
                         const int n,
                         const TaskParallelTLS *__restrict tls)
{
  BMThreadData *data = userdata;
  BMesh *bm = data->bm;
  const Mesh *me = data->me;

  int starti = n * VCHUNK;

  int ilen = starti + VCHUNK > bm->totvert ? bm->totvert - starti : VCHUNK;
  MVert *mv = me->mvert + starti;
  BMVert *v = data->verts[n];
  char *cdblock = data->vdata ? (char *)data->vdata[n] : NULL;

  for (int i = 0; i < ilen; i++, mv++) {
    if (cdblock) {
      v->head.data = (void *)cdblock;
      cdblock += data->cdvsize;
    }
    else {
      v->head.data = NULL;
    }

    v->head.htype = BM_VERT;
    v->head.hflag = BM_vert_flag_from_mflag(mv->flag);
    v->head.api_flag = 0;

    copy_v3_v3(v->co, mv->co);
    normal_short_to_float_v3(v->no, mv->no);

    v->e = NULL;
    v->head.index = i + starti;
    v = ELEM_NEXT(BMVert, v, data->vsize);
  }

  if (data->vdata) {
    v = data->verts[n];
    for (int i = 0; i < ilen; i++) {
      CustomData_to_bmesh_block(&me->vdata, &bm->vdata, i + starti, &v->head.data, true);
      v = ELEM_NEXT(BMVert, v, data->vsize);
    }
  }
}

static void bm_edge_task(void *__restrict userdata,
                         const int n,
                         const TaskParallelTLS *__restrict tls)
{
  BMThreadData *data = userdata;
  BMesh *bm = data->bm;
  const Mesh *me = data->me;

  int starti = n * ECHUNK;

  int ilen = starti + ECHUNK > bm->totedge ? bm->totedge - starti : ECHUNK;
  MEdge *med = me->medge + starti;
  BMEdge *e = data->edges[n];
  char *cdblock = data->edata ? (char *)data->edata[n] : NULL;

  for (int i = 0; i < ilen; i++, med++) {
    if (cdblock) {
      e->head.data = (void *)cdblock;
      cdblock += data->cdesize;
    }
    else {
      e->head.data = NULL;
    }

    e->head.htype = BM_EDGE;
    e->head.hflag = BM_edge_flag_from_mflag(med->flag);
    e->head.api_flag = 0;

    e->v1 = &data->verts[med->v1 / VCHUNK][med->v1 % VCHUNK];
    e->v2 = &data->verts[med->v2 / VCHUNK][med->v2 % VCHUNK];

    e->l = NULL;
    e->v1_disk_link.next = e->v1_disk_link.prev = NULL;
    e->v2_disk_link.next = e->v2_disk_link.prev = NULL;

    e = ELEM_NEXT(BMEdge, e, data->esize);
  }

  if (data->edata) {
    e = data->edges[n];
    for (int i = 0; i < ilen; i++) {
      CustomData_to_bmesh_block(&me->edata, &bm->edata, i + starti, &e->head.data, true);
      e = ELEM_NEXT(BMEdge, e, data->esize);
    }
  }
}

static void bm_loop_task(void *__restrict userdata,
                         const int n,
                         const TaskParallelTLS *__restrict tls)
{
  BMThreadData *data = userdata;
  BMesh *bm = data->bm;
  const Mesh *me = data->me;

  int starti = n * LCHUNK;

  int ilen = starti + LCHUNK > bm->totloop ? bm->totloop - starti : LCHUNK;
  MLoop *ml = me->mloop + starti;
  BMLoop *l = data->loops[n];
  char *cdblock = data->ldata ? (char *)data->ldata[n] : NULL;

  for (int i = 0; i < ilen; i++, ml++) {
    if (cdblock) {
      l->head.data = (void *)cdblock;
      cdblock += data->cdlsize;
    }
    else {
      l->head.data = NULL;
    }

    l->head.htype = BM_LOOP;
    l->head.hflag = 0;
    l->head.api_flag = 0;

    l->v = data->verts[ml->v / VCHUNK] + (ml->v % VCHUNK);
    l->e = data->edges[ml->e / ECHUNK] + (ml->e % ECHUNK);
    l->radial_next = l->radial_prev = l->next = l->prev = NULL;
    l->f = NULL;

    l = ELEM_NEXT(BMLoop, l, data->lsize);
  }

  if (data->ldata) {
    l = data->loops[n];
    for (int i = 0; i < ilen; i++) {
      CustomData_to_bmesh_block(&me->ldata, &bm->ldata, i + starti, &l->head.data, true);
      l = ELEM_NEXT(BMLoop, l, data->lsize);
    }
  }
}

static void bm_face_task(void *__restrict userdata,
                         const int n,
                         const TaskParallelTLS *__restrict tls)
{
  BMThreadData *data = userdata;
  BMesh *bm = data->bm;
  const Mesh *me = data->me;

  int starti = n * FCHUNK;

  int ilen = starti + FCHUNK > bm->totface ? bm->totface - starti : FCHUNK;
  MPoly *mp = me->mpoly + starti;
  BMFace *f = data->faces[n];
  char *cdblock = data->fdata ? (char *)data->fdata[n] : NULL;

  for (int i = 0; i < ilen; i++, mp++) {
    if (cdblock) {
      f->head.data = (void *)cdblock;
      cdblock += data->cdfsize;
    }
    else {
      f->head.data = NULL;
    }

    f->head.htype = BM_FACE;
    f->head.hflag = BM_face_flag_from_mflag(mp->flag);
    f->head.api_flag = 0;

    f->len = mp->totloop;
    f->mat_nr = mp->mat_nr;
    zero_v3(f->no);

    int li = mp->loopstart;
    BMLoop *lastl = NULL;

    for (int j = 0; j < mp->totloop; j++, li++) {
      BMLoop *l = data->loops[li / LCHUNK] + (li % LCHUNK);

      l->f = f;

      if (j == 0) {
        f->l_first = l;
      }
      else {
        lastl->next = l;
        l->prev = lastl;
      }

      lastl = l;
    }

    lastl->next = f->l_first;
    f->l_first->prev = lastl;

    f = ELEM_NEXT(BMFace, f, data->fsize);
  }

  if (data->fdata) {
    f = data->faces[n];
    for (int i = 0; i < ilen; i++) {
      CustomData_to_bmesh_block(&me->pdata, &bm->pdata, i + starti, &f->head.data, true);
      f = ELEM_NEXT(BMFace, f, data->fsize);
    }
  }
}

static void bm_mesh_cd_flag_apply(BMesh *bm, const char cd_flag)
{
  /* CustomData_bmesh_init_pool() must run first */
  BLI_assert(bm->vdata.totlayer == 0 || bm->vdata.pool != NULL);
  BLI_assert(bm->edata.totlayer == 0 || bm->edata.pool != NULL);
  BLI_assert(bm->pdata.totlayer == 0 || bm->pdata.pool != NULL);

  if (cd_flag & ME_CDFLAG_VERT_BWEIGHT) {
    if (!CustomData_has_layer(&bm->vdata, CD_BWEIGHT)) {
      CustomData_add_layer(&bm->vdata, CD_BWEIGHT, CD_ASSIGN, NULL, 0);
    }
  }
  else {
    if (CustomData_has_layer(&bm->vdata, CD_BWEIGHT)) {
      CustomData_free_layer_active(&bm->vdata, CD_BWEIGHT, 0);
    }
  }

  if (cd_flag & ME_CDFLAG_EDGE_BWEIGHT) {
    if (!CustomData_has_layer(&bm->edata, CD_BWEIGHT)) {
      CustomData_add_layer(&bm->edata, CD_BWEIGHT, CD_ASSIGN, NULL, 0);
    }
  }
  else {
    if (CustomData_has_layer(&bm->edata, CD_BWEIGHT)) {
      CustomData_free_layer_active(&bm->edata, CD_BWEIGHT, 0);
    }
  }

  if (cd_flag & ME_CDFLAG_EDGE_CREASE) {
    if (!CustomData_has_layer(&bm->edata, CD_CREASE)) {
      CustomData_add_layer(&bm->edata, CD_CREASE, CD_ASSIGN, NULL, 0);
    }
  }
  else {
    if (CustomData_has_layer(&bm->edata, CD_CREASE)) {
      CustomData_free_layer_active(&bm->edata, CD_CREASE, 0);
    }
  }
}

BMesh *BM_mesh_bm_from_me_threaded(BMesh *bm,
                                   Object *ob,
                                   const Mesh *me,
                                   const struct BMeshFromMeshParams *params)
{
  if (!bm) {
    bm = MEM_callocN(sizeof(BMesh), "BM_mesh_bm_from_me_threaded bm");
  }
  else {
    BM_mesh_data_free(bm);
    memset((void *)bm, 0, sizeof(*bm));
  }
  const bool is_new = true;

  bm->totvert = me->totvert;
  bm->totedge = me->totedge;
  bm->totface = me->totpoly;
  bm->totloop = me->totloop;

  bm->elem_index_dirty = bm->elem_table_dirty = BM_VERT | BM_EDGE | BM_LOOP | BM_FACE;
  bm->spacearr_dirty = BM_SPACEARR_DIRTY_ALL;

  BMVert **verts;
  BMEdge **edges;
  BMLoop **loops;
  BMFace **faces;

  void **vdata = NULL, **edata = NULL, **ldata = NULL, **fdata = NULL;
  int totdv = 0, totde = 0, totdl = 0, totdf = 0;

  int totcv = 0, totce = 0, totcl = 0, totcf = 0;

  BMThreadData data = {0};

  int vsize, esize, lsize, fsize;

  bm->vpool = BLI_mempool_create_for_tasks(sizeof(BMVert),
                                           bm->totvert,
                                           VCHUNK,
                                           (void ***)&verts,
                                           &totcv,
                                           &vsize,
                                           BLI_MEMPOOL_ALLOW_ITER);
  bm->epool = BLI_mempool_create_for_tasks(sizeof(BMEdge),
                                           bm->totedge,
                                           ECHUNK,
                                           (void ***)&edges,
                                           &totce,
                                           &esize,
                                           BLI_MEMPOOL_ALLOW_ITER);
  bm->lpool = BLI_mempool_create_for_tasks(sizeof(BMLoop),
                                           bm->totloop,
                                           LCHUNK,
                                           (void ***)&loops,
                                           &totcl,
                                           &lsize,
                                           BLI_MEMPOOL_ALLOW_ITER);
  bm->fpool = BLI_mempool_create_for_tasks(sizeof(BMFace),
                                           bm->totface,
                                           FCHUNK,
                                           (void ***)&faces,
                                           &totcf,
                                           &fsize,
                                           BLI_MEMPOOL_ALLOW_ITER);

  data.verts = verts;
  data.edges = edges;
  data.loops = loops;
  data.faces = faces;

  data.vsize = vsize;
  data.esize = esize;
  data.lsize = lsize;
  data.fsize = fsize;

  data.totcv = totcv;
  data.totce = totce;
  data.totcl = totcl;
  data.totcf = totcf;

  data.bm = bm;
  data.me = me;

  // bm->vpool = BLI_mem

  KeyBlock *actkey, *block;
  BMEdge *e;
  BMFace *f;
  float(*keyco)[3] = NULL;
  int i;
  CustomData_MeshMasks mask = CD_MASK_BMESH;
  CustomData_MeshMasks_update(&mask, &params->cd_mask_extra);

  MultiresModifierData *mmd = ob ? get_multires_modifier(NULL, ob, true) : NULL;

  if (mmd) {
    bm->multires = *mmd;
    bm->haveMultiResSettings = true;
    bm->multiresSpace = MULTIRES_SPACE_TANGENT;
  }
  else {
    bm->haveMultiResSettings = false;
  }

  CustomData_copy(&me->vdata, &bm->vdata, mask.vmask, CD_ASSIGN, CD_FLAG_NOCOPY);
  CustomData_copy(&me->edata, &bm->edata, mask.emask, CD_ASSIGN, CD_FLAG_NOCOPY);
  CustomData_copy(&me->ldata, &bm->ldata, mask.lmask, CD_ASSIGN, CD_FLAG_NOCOPY);
  CustomData_copy(&me->pdata, &bm->pdata, mask.pmask, CD_ASSIGN, CD_FLAG_NOCOPY);

  CustomData *cds[] = {&bm->vdata, &bm->edata, &bm->ldata, &bm->pdata};

  // clear customdata->layers[X].data pointers
  for (int i = 0; i < 4; i++) {
    CustomData *cd = cds[i];
    for (int j = 0; j < cd->totlayer; j++) {
      cd->layers[j].data = NULL;
    }
  }
  bm_mesh_cd_flag_apply(bm, me->cd_flag);

  data.cd_vert_bweight = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);
  data.cd_edge_bweight = CustomData_get_offset(&bm->edata, CD_BWEIGHT);
  data.cd_crease = CustomData_get_offset(&bm->edata, CD_CREASE);

  if (bm->vdata.totlayer) {
    bm->vdata.pool = BLI_mempool_create_for_tasks(
        bm->vdata.totsize, bm->totvert, VCHUNK, &vdata, &totdv, &data.cdvsize, BLI_MEMPOOL_NOP);
  }
  if (bm->edata.totlayer) {
    bm->edata.pool = BLI_mempool_create_for_tasks(
        bm->edata.totsize, bm->totedge, ECHUNK, &edata, &totde, &data.cdesize, BLI_MEMPOOL_NOP);
  }
  if (bm->ldata.totlayer) {
    bm->ldata.pool = BLI_mempool_create_for_tasks(
        bm->ldata.totsize, bm->totloop, LCHUNK, &ldata, &totdl, &data.cdlsize, BLI_MEMPOOL_NOP);
  }
  if (bm->pdata.totlayer) {
    bm->pdata.pool = BLI_mempool_create_for_tasks(
        bm->pdata.totsize, bm->totface, FCHUNK, &fdata, &totdf, &data.cdfsize, BLI_MEMPOOL_NOP);
  }

  data.vdata = vdata;
  data.edata = edata;
  data.ldata = ldata;
  data.fdata = fdata;

  data.totdv = totdv;
  data.totde = totde;
  data.totdl = totdl;
  data.totdf = totdf;

  /* -------------------------------------------------------------------- */
  /* Shape Key */
  int tot_shape_keys = 0;
  if (me->key != NULL && DEG_is_original_id(&me->id)) {
    /* Evaluated meshes can be topologically inconsistent with their shape keys.
     * Shape keys are also already integrated into the state of the evaluated
     * mesh, so considering them here would kind of apply them twice. */
    tot_shape_keys = BLI_listbase_count(&me->key->block);

    /* Original meshes must never contain a shape-key custom-data layers.
     *
     * This may happen if and object's mesh data is accidentally
     * set to the output from the modifier stack, causing it to be an "original" ID,
     * even though the data isn't fully compatible (hence this assert).
     *
     * This results in:
     * - The newly created #BMesh having twice the number of custom-data layers.
     * - When converting the #BMesh back to a regular mesh,
     *   At least one of the extra shape-key blocks will be created in #Mesh.key
     *   depending on the value of #CustomDataLayer.uid.
     *
     * We could support mixing both kinds of data if there is a compelling use-case for it.
     * At the moment it's simplest to assume all original meshes use the key-block and meshes
     * that are evaluated (through the modifier stack for example) use custom-data layers.
     */
    BLI_assert(!CustomData_has_layer(&me->vdata, CD_SHAPEKEY));
  }
  if (is_new == false) {
    tot_shape_keys = min_ii(tot_shape_keys, CustomData_number_of_layers(&bm->vdata, CD_SHAPEKEY));
  }

  float(**shape_key_table)[3] = tot_shape_keys ?
                                    BLI_array_alloca(shape_key_table, tot_shape_keys) :
                                    NULL;

  if ((params->active_shapekey != 0) && tot_shape_keys > 0) {
    actkey = BLI_findlink(&me->key->block, params->active_shapekey - 1);
  }
  else {
    actkey = NULL;
  }

  if (is_new) {
    if (tot_shape_keys || params->add_key_index) {
      CustomData_add_layer(&bm->vdata, CD_SHAPE_KEYINDEX, CD_ASSIGN, NULL, 0);
    }
  }

  if (tot_shape_keys) {
    if (is_new) {
      /* Check if we need to generate unique ids for the shape-keys.
       * This also exists in the file reading code, but is here for a sanity check. */
      if (!me->key->uidgen) {
        fprintf(stderr,
                "%s had to generate shape key uid's in a situation we shouldn't need to! "
                "(bmesh internal error)\n",
                __func__);

        me->key->uidgen = 1;
        for (block = me->key->block.first; block; block = block->next) {
          block->uid = me->key->uidgen++;
        }
      }
    }

    if (actkey && actkey->totelem == me->totvert) {
      keyco = params->use_shapekey ? actkey->data : NULL;
      if (is_new) {
        bm->shapenr = params->active_shapekey;
      }
    }

    for (i = 0, block = me->key->block.first; i < tot_shape_keys; block = block->next, i++) {
      if (is_new) {
        CustomData_add_layer_named(&bm->vdata, CD_SHAPEKEY, CD_ASSIGN, NULL, 0, block->name);
        int j = CustomData_get_layer_index_n(&bm->vdata, CD_SHAPEKEY, i);
        bm->vdata.layers[j].uid = block->uid;
      }
      shape_key_table[i] = (float(*)[3])block->data;
    }
  }

  data.tot_shape_keys = tot_shape_keys;
  data.shape_key_table = shape_key_table;

  TaskParallelSettings settings;

  BLI_parallel_range_settings_defaults(&settings);
  BLI_task_parallel_range(0, data.totcv, &data, bm_vert_task, &settings);
  BLI_task_parallel_range(0, data.totce, &data, bm_edge_task, &settings);
  BLI_task_parallel_range(0, data.totcl, &data, bm_loop_task, &settings);
  BLI_task_parallel_range(0, data.totcf, &data, bm_face_task, &settings);

  BMIter iter;

  // link edges
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    bmesh_disk_edge_append(e, e->v1);
    bmesh_disk_edge_append(e, e->v2);
  }

  // link radial lists
  i = 0;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l = f->l_first;

    do {
      bmesh_radial_loop_append(l->e, l);

      l = l->next;
    } while (l != f->l_first);

    i++;
  }

  printf("totface: %d\n", i);

  bm->elem_index_dirty = BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_table_dirty = BM_VERT | BM_EDGE | BM_FACE;

  return bm;
}

static void bm_unmark_temp_cdlayers(BMesh *bm)
{
  CustomData_unmark_temporary_nocopy(&bm->vdata);
  CustomData_unmark_temporary_nocopy(&bm->edata);
  CustomData_unmark_temporary_nocopy(&bm->ldata);
  CustomData_unmark_temporary_nocopy(&bm->pdata);
}

static void bm_mark_temp_cdlayers(BMesh *bm)
{
  CustomData_mark_temporary_nocopy(&bm->vdata);
  CustomData_mark_temporary_nocopy(&bm->edata);
  CustomData_mark_temporary_nocopy(&bm->ldata);
  CustomData_mark_temporary_nocopy(&bm->pdata);
}

typedef struct BMToMeTask {
  Mesh *me;
  BMesh *bm;
  Object *ob;
  Main *bmain;
  const struct BMeshToMeshParams *params;
  struct CustomData_MeshMasks mask;
  uint64_t extra2;
} BMToMeTask;

static void me_vert_task(void *__restrict userdata)
{
  BMToMeTask *data = (BMToMeTask *)userdata;
  Mesh *me = data->me;
  BMesh *bm = data->bm;

  CustomData_free(&me->vdata, me->totvert);
  me->totvert = bm->totvert;

  CustomData_copy(&bm->vdata, &me->vdata, data->mask.vmask | data->extra2, CD_CALLOC, me->totvert);

  MVert *mvert = bm->totvert ? MEM_callocN(sizeof(MVert) * bm->totvert, "bm_to_me.vert") : NULL;
  CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, mvert, me->totvert);

  BMVert *v;
  BMIter iter;
  int i = 0;

  const int cd_vert_bweight_offset = CustomData_get_offset(&bm->vdata, CD_BWEIGHT);

  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    copy_v3_v3(mvert->co, v->co);
    normal_float_to_short_v3(mvert->no, v->no);

    mvert->flag = BM_vert_flag_to_mflag(v);

    /* Copy over custom-data. */
    CustomData_from_bmesh_block(&bm->vdata, &me->vdata, v->head.data, i);

    if (cd_vert_bweight_offset != -1) {
      mvert->bweight = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(v, cd_vert_bweight_offset);
    }

    mvert++;
    i++;

    BM_CHECK_ELEMENT(v);
  }
}

BLI_INLINE void bmesh_quick_edgedraw_flag(MEdge *med, BMEdge *e)
{
  /* This is a cheap way to set the edge draw, its not precise and will
   * pick the first 2 faces an edge uses.
   * The dot comparison is a little arbitrary, but set so that a 5 subd
   * IcoSphere won't vanish but subd 6 will (as with pre-bmesh Blender). */

  if (/* (med->flag & ME_EDGEDRAW) && */ /* Assume to be true. */
      (e->l && (e->l != e->l->radial_next)) &&
      (dot_v3v3(e->l->f->no, e->l->radial_next->f->no) > 0.9995f)) {
    med->flag &= ~ME_EDGEDRAW;
  }
  else {
    med->flag |= ME_EDGEDRAW;
  }
}

static void me_edge_task(void *__restrict userdata)
{
  BMToMeTask *data = (BMToMeTask *)userdata;
  Mesh *me = data->me;
  BMesh *bm = data->bm;
  MEdge *med;

  CustomData_free(&me->edata, me->totedge);
  me->totedge = bm->totedge;

  CustomData_copy(&bm->edata, &me->edata, data->mask.emask | data->extra2, CD_CALLOC, me->totvert);

  MEdge *medge = bm->totedge ? MEM_callocN(sizeof(MEdge) * bm->totedge, "bm_to_me.edge") : NULL;
  CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, medge, me->totedge);
  const int cd_edge_bweight_offset = CustomData_get_offset(&bm->edata, CD_BWEIGHT);

  BMEdge *e;
  BMIter iter;
  int i = 0;

  const int cd_edge_crease_offset = CustomData_get_offset(&bm->edata, CD_CREASE);

  med = medge;
  i = 0;
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    med->v1 = BM_elem_index_get(e->v1);
    med->v2 = BM_elem_index_get(e->v2);

    med->flag = BM_edge_flag_to_mflag(e);

    /* Copy over custom-data. */
    CustomData_from_bmesh_block(&bm->edata, &me->edata, e->head.data, i);

    bmesh_quick_edgedraw_flag(med, e);

    if (cd_edge_crease_offset != -1) {
      med->crease = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(e, cd_edge_crease_offset);
    }
    if (cd_edge_bweight_offset != -1) {
      med->bweight = BM_ELEM_CD_GET_FLOAT_AS_UCHAR(e, cd_edge_bweight_offset);
    }

    i++;
    med++;
    BM_CHECK_ELEMENT(e);
  }
}

static void me_face_task(void *__restrict userdata)
{
  BMToMeTask *data = (BMToMeTask *)userdata;
  Mesh *me = data->me;
  BMesh *bm = data->bm;
  MPoly *mpoly;
  MLoop *mloop;

  // set up polys
  CustomData_free(&me->pdata, me->totpoly);
  me->totpoly = bm->totface;

  CustomData_copy(&bm->pdata, &me->pdata, data->mask.pmask | data->extra2, CD_CALLOC, me->totpoly);

  mpoly = bm->totface ? MEM_callocN(sizeof(MPoly) * bm->totface, "bm_to_me.poly") : NULL;
  CustomData_add_layer(&me->pdata, CD_MPOLY, CD_ASSIGN, mpoly, me->totpoly);

  // set up loops
  CustomData_free(&me->ldata, me->totloop);
  me->totloop = bm->totloop;

  CustomData_copy(&bm->ldata, &me->ldata, data->mask.lmask | data->extra2, CD_CALLOC, me->totloop);

  mloop = bm->totloop ? MEM_callocN(sizeof(MLoop) * bm->totloop, "bm_to_me.loop") : NULL;
  CustomData_add_layer(&me->ldata, CD_MLOOP, CD_ASSIGN, mloop, me->totloop);

  // convert

  BMIter iter;
  BMFace *f;

  int i, j;
  i = 0;
  j = 0;
  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l_iter, *l_first;
    mpoly->loopstart = j;
    mpoly->totloop = f->len;
    mpoly->mat_nr = f->mat_nr;
    mpoly->flag = BM_face_flag_to_mflag(f);

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      mloop->e = BM_elem_index_get(l_iter->e);
      mloop->v = BM_elem_index_get(l_iter->v);

      /* Copy over custom-data. */
      CustomData_from_bmesh_block(&bm->ldata, &me->ldata, l_iter->head.data, j);

      j++;
      mloop++;
      BM_CHECK_ELEMENT(l_iter);
      BM_CHECK_ELEMENT(l_iter->e);
      BM_CHECK_ELEMENT(l_iter->v);
    } while ((l_iter = l_iter->next) != l_first);

    if (f == bm->act_face) {
      me->act_face = i;
    }

    /* Copy over custom-data. */
    CustomData_from_bmesh_block(&bm->pdata, &me->pdata, f->head.data, i);

    i++;
    mpoly++;
    BM_CHECK_ELEMENT(f);
  }
}

BMVert **bm_to_mesh_vertex_map(BMesh *bm, int ototvert);
int bm_to_mesh_shape_layer_index_from_kb(BMesh *bm, KeyBlock *currkey);

typedef struct Test {
  BMToMeTask *data;
  int n;
} Test;
static void *test(void *userdata)
{
  Test *test = (Test *)userdata;
  switch (test->n) {
    case 0:
      me_vert_task(test->data);
      break;
    case 1:
      me_edge_task(test->data);
      break;
    case 2:
      me_face_task(test->data);
      break;
  }

  return NULL;
}

void BM_mesh_bm_to_me_threaded(
    Main *bmain, Object *ob, BMesh *bm, Mesh *me, const struct BMeshToMeshParams *params)
{
  BMVert *eve;
  BMIter iter;

  MVert *oldverts = NULL, *mvert = NULL;
  const int ototvert = me->totvert;
  const int cd_shape_keyindex_offset = CustomData_get_offset(&bm->vdata, CD_SHAPE_KEYINDEX);

  int i, j;

  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

  if (me->key && (cd_shape_keyindex_offset != -1)) {
    /* Keep the old verts in case we are working on* a key, which is done at the end. */

    /* Use the array in-place instead of duplicating the array. */
#  if 0
  oldverts = MEM_dupallocN(me->mvert);
#  else
    oldverts = me->mvert;
    me->mvert = NULL;
    CustomData_update_typemap(&me->vdata);
    CustomData_set_layer(&me->vdata, CD_MVERT, NULL);
#  endif
  }

  BMToMeTask taskdata = {.params = params, .bm = bm, .me = me, .ob = ob, .bmain = bmain};

  if (params->copy_temp_cdlayers) {
    bm_unmark_temp_cdlayers(bm);
  }

  // ensure multires space is correct
  if (bm->haveMultiResSettings && bm->multiresSpace != MULTIRES_SPACE_TANGENT) {
    BM_enter_multires_space(ob, bm, MULTIRES_SPACE_TANGENT);
  }

  CustomData_MeshMasks mask = CD_MASK_MESH;
  CustomData_MeshMasks_update(&mask, &params->cd_mask_extra);
  CustomDataMask extra2 = params->copy_mesh_id_layers ? CD_MASK_MESH_ID : 0;
  CustomData *srcdatas[] = {&bm->vdata, &bm->edata, &bm->ldata, &bm->pdata};
  int id_flags[4] = {-1, -1, -1, -1};

  taskdata.mask = mask;
  taskdata.extra2 = extra2;

  // copy id layers? temporarily clear cd_temporary and cd_flag_elem_nocopy flags
  if (params->copy_mesh_id_layers) {

    for (int i = 0; i < 4; i++) {
      int idx = CustomData_get_layer_index(srcdatas[i], CD_MESH_ID);
      if (idx >= 0) {
        id_flags[i] = srcdatas[i]->layers[idx].flag;
        srcdatas[i]->layers[idx].flag &= ~(CD_FLAG_TEMPORARY | CD_FLAG_ELEM_NOCOPY);
      }
    }
  }

  me->cd_flag = BM_mesh_cd_flag_from_bmesh(bm);

#  if 0
  struct TaskGraph *taskgraph = BLI_task_graph_create();
  struct TaskNode *node;

  node = BLI_task_graph_node_create(taskgraph, me_vert_task, &taskdata, NULL);
  BLI_task_graph_node_push_work(node);

  node = BLI_task_graph_node_create(taskgraph, me_edge_task, &taskdata, NULL);
  BLI_task_graph_node_push_work(node);

  node = BLI_task_graph_node_create(taskgraph, me_face_task, &taskdata, NULL);
  BLI_task_graph_node_push_work(node);

  BLI_task_graph_work_and_wait(taskgraph);
  BLI_task_graph_free(taskgraph);
#  else
  ListBase threadpool;
  Test datas[3] = {{&taskdata, 0}, {&taskdata, 1}, {&taskdata, 2}};

  BLI_threadpool_init(&threadpool, test, 3);
  BLI_threadpool_insert(&threadpool, &datas[0]);
  BLI_threadpool_insert(&threadpool, &datas[1]);
  BLI_threadpool_insert(&threadpool, &datas[2]);
  BLI_threadpool_end(&threadpool);

// BLI_threadpool_
#  endif
  // undo changes to source bmesh's id layers' flags
  if (params->copy_mesh_id_layers) {
    for (int i = 0; i < 4; i++) {
      int idx = CustomData_get_layer_index(srcdatas[i], CD_MESH_ID);

      if (id_flags[i] >= 0 && idx >= 0) {
        srcdatas[i]->layers[idx].flag = id_flags[i];
      }
    }
  }

  if (me->fdata.layers) {
    CustomData_free(&me->fdata, me->totface);
  }

  CustomData_reset(&me->fdata);

  /* Will be overwritten with a valid value if 'dotess' is set, otherwise we
   * end up with 'me->totface' and me->mface == NULL which can crash T28625. */
  me->totface = 0;
  me->act_face = -1;

  BKE_mesh_update_customdata_pointers(me, 0);

  /* Patch hook indices and vertex parents. */
  if (params->calc_object_remap && (ototvert > 0)) {
    BLI_assert(bmain != NULL);
    Object *ob;
    ModifierData *md;
    BMVert **vertMap = NULL;

    for (ob = bmain->objects.first; ob; ob = ob->id.next) {
      if ((ob->parent) && (ob->parent->data == me) && ELEM(ob->partype, PARVERT1, PARVERT3)) {

        if (vertMap == NULL) {
          vertMap = bm_to_mesh_vertex_map(bm, ototvert);
        }

        if (ob->par1 < ototvert) {
          eve = vertMap[ob->par1];
          if (eve) {
            ob->par1 = BM_elem_index_get(eve);
          }
        }
        if (ob->par2 < ototvert) {
          eve = vertMap[ob->par2];
          if (eve) {
            ob->par2 = BM_elem_index_get(eve);
          }
        }
        if (ob->par3 < ototvert) {
          eve = vertMap[ob->par3];
          if (eve) {
            ob->par3 = BM_elem_index_get(eve);
          }
        }
      }
      if (ob->data == me) {
        for (md = ob->modifiers.first; md; md = md->next) {
          if (md->type == eModifierType_Hook) {
            HookModifierData *hmd = (HookModifierData *)md;

            if (vertMap == NULL) {
              vertMap = bm_to_mesh_vertex_map(bm, ototvert);
            }

            for (i = j = 0; i < hmd->totindex; i++) {
              if (hmd->indexar[i] < ototvert) {
                eve = vertMap[hmd->indexar[i]];

                if (eve) {
                  hmd->indexar[j++] = BM_elem_index_get(eve);
                }
              }
              else {
                j++;
              }
            }

            hmd->totindex = j;
          }
        }
      }
    }

    if (vertMap) {
      MEM_freeN(vertMap);
    }
  }

  /* This is called again, 'dotess' arg is used there. */
  BKE_mesh_update_customdata_pointers(me, false);

  {
    BMEditSelection *selected;
    me->totselect = BLI_listbase_count(&(bm->selected));

    MEM_SAFE_FREE(me->mselect);
    if (me->totselect != 0) {
      me->mselect = MEM_mallocN(sizeof(MSelect) * me->totselect, "Mesh selection history");
    }

    for (i = 0, selected = bm->selected.first; selected; i++, selected = selected->next) {
      if (selected->htype == BM_VERT) {
        me->mselect[i].type = ME_VSEL;
      }
      else if (selected->htype == BM_EDGE) {
        me->mselect[i].type = ME_ESEL;
      }
      else if (selected->htype == BM_FACE) {
        me->mselect[i].type = ME_FSEL;
      }

      me->mselect[i].index = BM_elem_index_get(selected->ele);
    }
  }

  /* See comment below, this logic is in twice. */

  if (me->key) {
    KeyBlock *currkey;
    KeyBlock *actkey = BLI_findlink(&me->key->block, bm->shapenr - 1);

    float(*ofs)[3] = NULL;

    /* Go through and find any shape-key custom-data layers
     * that might not have corresponding KeyBlocks, and add them if necessary. */
    for (i = 0; i < bm->vdata.totlayer; i++) {
      if (bm->vdata.layers[i].type != CD_SHAPEKEY) {
        continue;
      }

      for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
        if (currkey->uid == bm->vdata.layers[i].uid) {
          break;
        }
      }

      if (!currkey) {
        currkey = BKE_keyblock_add(me->key, bm->vdata.layers[i].name);
        currkey->uid = bm->vdata.layers[i].uid;
      }
    }

    /* Editing the base key should update others. */
    if (/* Only need offsets for relative shape keys. */
        (me->key->type == KEY_RELATIVE) &&

        /* Unlikely, but the active key may not be valid if the
         * BMesh and the mesh are out of sync. */
        (actkey != NULL) &&

        /* Not used here, but 'oldverts' is used later for applying 'ofs'. */
        (oldverts != NULL) &&

        /* Needed for referencing oldverts. */
        (cd_shape_keyindex_offset != -1)) {

      const bool act_is_basis = BKE_keyblock_is_basis(me->key, bm->shapenr - 1);

      /* Active key is a base. */
      if (act_is_basis) {
        const float(*fp)[3] = actkey->data;

        ofs = MEM_callocN(sizeof(float[3]) * bm->totvert, "currkey->data");
        mvert = me->mvert;
        BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
          const int keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);

          /* Could use 'eve->co' or 'mvert->co', they're the same at this point. */
          if (keyi != ORIGINDEX_NONE && keyi < actkey->totelem) {
            sub_v3_v3v3(ofs[i], mvert->co, fp[keyi]);
          }
          else {
            /* If there are new vertices in the mesh, we can't propagate the offset
             * because it will only work for the existing vertices and not the new
             * ones, creating a mess when doing e.g. subdivide + translate. */
            MEM_freeN(ofs);
            ofs = NULL;
            break;
          }

          mvert++;
        }
      }
    }

    for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
      int keyi;
      const float(*ofs_pt)[3] = ofs;
      float *newkey, (*oldkey)[3], *fp;

      const int currkey_uuid = bm_to_mesh_shape_layer_index_from_kb(bm, currkey);
      const int cd_shape_offset = (currkey_uuid == -1) ? -1 :
                                                         CustomData_get_n_offset(&bm->vdata,
                                                                                 CD_SHAPEKEY,
                                                                                 currkey_uuid);
      const bool apply_offset = (cd_shape_offset != -1) && (ofs != NULL) && (currkey != actkey) &&
                                (bm->shapenr - 1 == currkey->relative);

      fp = newkey = MEM_callocN(me->key->elemsize * bm->totvert, "currkey->data");
      oldkey = currkey->data;

      mvert = me->mvert;
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {

        if (currkey == actkey) {
          copy_v3_v3(fp, eve->co);

          if (actkey != me->key->refkey) { /* Important see bug T30771. */
            if (cd_shape_keyindex_offset != -1) {
              if (oldverts) {
                keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset);
                if (keyi != ORIGINDEX_NONE && keyi < currkey->totelem) { /* Valid old vertex. */
                  copy_v3_v3(mvert->co, oldverts[keyi].co);
                }
              }
            }
          }
        }
        else if (cd_shape_offset != -1) {
          /* In most cases this runs. */
          copy_v3_v3(fp, BM_ELEM_CD_GET_VOID_P(eve, cd_shape_offset));
        }
        else if ((oldkey != NULL) && (cd_shape_keyindex_offset != -1) &&
                 ((keyi = BM_ELEM_CD_GET_INT(eve, cd_shape_keyindex_offset)) != ORIGINDEX_NONE) &&
                 (keyi < currkey->totelem)) {
          /* Old method of reconstructing keys via vertices original key indices,
           * currently used if the new method above fails
           * (which is theoretically possible in certain cases of undo). */
          copy_v3_v3(fp, oldkey[keyi]);
        }
        else {
          /* Fail! fill in with dummy value. */
          copy_v3_v3(fp, mvert->co);
        }

        /* Propagate edited basis offsets to other shapes. */
        if (apply_offset) {
          add_v3_v3(fp, *ofs_pt++);
          /* Apply back new coordinates shape-keys that have offset into BMesh.
           * Otherwise, in case we call again #BM_mesh_bm_to_me on same BMesh,
           * we'll apply diff from previous call to #BM_mesh_bm_to_me,
           * to shape-key values from *original creation of the BMesh*. See T50524. */
          copy_v3_v3(BM_ELEM_CD_GET_VOID_P(eve, cd_shape_offset), fp);
        }

        fp += 3;
        mvert++;
      }

      currkey->totelem = bm->totvert;
      if (currkey->data) {
        MEM_freeN(currkey->data);
      }
      currkey->data = newkey;
    }

    if (ofs) {
      MEM_freeN(ofs);
    }
  }

  /* Run this even when shape keys aren't used since it may be used for hooks or vertex parents. */
  if (params->update_shapekey_indices) {
    /* We have written a new shape key, if this mesh is _not_ going to be freed,
     * update the shape key indices to match the newly updated. */
    if (cd_shape_keyindex_offset != -1) {
      BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
        BM_ELEM_CD_SET_INT(eve, cd_shape_keyindex_offset, i);
      }
    }
  }

  me->cd_flag = BM_mesh_cd_flag_from_bmesh(bm);

  if (oldverts != NULL) {
    MEM_freeN(oldverts);
  }

  /* Topology could be changed, ensure #CD_MDISPS are ok. */
  multires_topology_changed(me);

  /* To be removed as soon as COW is enabled by default. */
  BKE_mesh_runtime_clear_geometry(me);

  if (params->copy_temp_cdlayers) {
    bm_mark_temp_cdlayers(bm);
  }
}
#endif
