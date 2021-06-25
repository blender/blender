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

void bm_vert_task(void *__restrict userdata, const int n, const TaskParallelTLS *__restrict tls)
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

void bm_edge_task(void *__restrict userdata, const int n, const TaskParallelTLS *__restrict tls)
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

void bm_loop_task(void *__restrict userdata, const int n, const TaskParallelTLS *__restrict tls)
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

void bm_face_task(void *__restrict userdata, const int n, const TaskParallelTLS *__restrict tls)
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

  MVert *mvert;
  MEdge *medge;
  MLoop *mloop;
  MPoly *mp;
  KeyBlock *actkey, *block;
  BMVert *v, **vtable = NULL;
  BMEdge *e, **etable = NULL;
  BMFace *f, **ftable = NULL;
  float(*keyco)[3] = NULL;
  int totloops, i;
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
  BMLoop *l;

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
#endif
