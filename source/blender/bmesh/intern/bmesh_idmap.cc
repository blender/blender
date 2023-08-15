#include "MEM_guardedalloc.h"

#include "BLI_assert.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "BKE_customdata.h"

#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"

#include "bmesh_idmap.h"
#include <cstdarg>
#include <cstdio>

using namespace blender;

#define FREELIST_HASHMAP_THRESHOLD_HIGH 1024
#define FREELIST_HASHMAP_THRESHOLD_LOW 700

#ifdef DEBUG_BM_IDMAP
static void bm_idmap_debug_check_init(BMesh *bm)
{
  /* Disable mempool allocation so we can use
   * element pointers as backup IDs.
   */
  BLI_mempool_ignore_free(bm->vpool);
  BLI_mempool_ignore_free(bm->epool);
  BLI_mempool_ignore_free(bm->lpool);
  BLI_mempool_ignore_free(bm->fpool);
}
#endif

static void idmap_log_message(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

#ifdef DEBUG_BM_IDMAP
static void _idmap_debug_insert(const char *func, BMIdMap *idmap, BMElem *elem, int id)
{
  if (id == BM_ID_NONE) {
    idmap_log_message("%s: Tried to assign a null id\n", func);
  }

  idmap->elem2id->add(elem, id);
  idmap->id2elem->add(id, elem);
}

#  define idmap_debug_insert(idmap, elem, id) _idmap_debug_insert(__func__, idmap, elem, id)
#endif

BMIdMap *BM_idmap_new(BMesh *bm, int elem_mask)
{
  BMIdMap *idmap = MEM_new<BMIdMap>("BMIdMap");

#ifdef DEBUG_BM_IDMAP
  bm_idmap_debug_check_init(bm);
  idmap->elem2id = new blender::Map<BMElem *, int>;
  idmap->id2elem = new blender::Map<int, BMElem *>;
#endif

  for (int i = 0; i < ARRAY_SIZE(idmap->cd_id_off); i++) {
    idmap->cd_id_off[i] = -1;
  }

  idmap->flag = elem_mask;
  idmap->bm = bm;
  idmap->maxid = BM_ID_NONE + 1;

  BM_idmap_check_attributes(idmap);

  return idmap;
}

static const char elem_names[9][16] = {
    "corrupted",  // 0
    "vertex",     // 1
    "edge",       // 2
    "corrupted",  // 3
    "loop",       // 4
    "corrupted",  // 5
    "corrupted",  // 6
    "corrupted",  // 7
    "face",       // 8
};

#ifdef DEBUG_BM_IDMAP
static const char *get_type_name(char htype)
{
  if (htype <= 0 || htype >= 9) {
    return "corrupted";
  }

  return elem_names[int(htype)];
}
#endif

template<typename T> static constexpr char get_elem_type()
{
  if constexpr (std::is_same_v<T, BMVert>) {
    return BM_VERT;
  }
  else if constexpr (std::is_same_v<T, BMEdge>) {
    return BM_EDGE;
  }
  else if constexpr (std::is_same_v<T, BMLoop>) {
    return BM_LOOP;
  }
  else if constexpr (std::is_same_v<T, BMFace>) {
    return BM_FACE;
  }
}

#ifdef DEBUG_BM_IDMAP
static bool _idmap_check_elem(const char *func, BMIdMap *idmap, BMElem *elem)
{
  int id = BM_idmap_get_id(idmap, elem);
  bool exists = idmap->elem2id->contains(elem);

  if (!elem || !ELEM(elem->head.htype, BM_VERT, BM_EDGE, BM_LOOP, BM_FACE)) {
    idmap_log_message("%s: bad call to idmap_check_elem; %p\n", func, elem);
    return false;
  }

  if (id == BM_ID_NONE && !exists) {
    return true;
  }

  if (id != BM_ID_NONE && !exists) {
    idmap_log_message("%s: elem %p(%d, a %s) has an id but isn't in map\n",
                      func,
                      elem,
                      id,
                      get_type_name(elem->head.htype));
    if (idmap->id2elem->contains(id)) {
      BMElem *elem2 = idmap->id2elem->lookup(id);
      idmap_log_message(
          "  another elem %p (a %s) has the id\n", elem2, get_type_name(elem2->head.htype));
    }
    return false;
  }

  int id2 = idmap->elem2id->contains(elem) ? idmap->elem2id->lookup(elem) : -1;
  if (id2 != id) {
    idmap_log_message("%s: elem %p (a %s) has id %d; expected %d\n",
                      func,
                      elem,
                      get_type_name(elem->head.htype),
                      id,
                      id2);
  }

  return true;
}

#  define idmap_check_elem(idmap, elem) _idmap_check_elem(__func__, idmap, elem)
#else
#  define idmap_check_elem(idmap, elem) \
    do { \
    } while (0)
#endif

static void idmap_grow_map(BMIdMap *idmap, int newid)
{
  if (idmap->map_size > newid) {
    return;
  }

  int newsize = (newid + 1);
  newsize += newsize >> 1;

  if (idmap->map) {
    idmap->map = (BMElem **)MEM_recallocN((void *)idmap->map, sizeof(void *) * newsize);
  }
  else {
    idmap->map = (BMElem **)MEM_calloc_arrayN(newsize, sizeof(void *), "bm idmap");
  }

  idmap->map_size = newsize;
}

void BM_idmap_check_ids(BMIdMap *idmap)
{
  BMIter iter;
  BMVert *v;
  BMEdge *e;
  BMFace *f;

#ifdef DEBUG_BM_IDMAP
  bm_idmap_debug_check_init(idmap->bm);
  delete idmap->id2elem;
  delete idmap->elem2id;

  idmap->elem2id = new blender::Map<BMElem *, int>;
  idmap->id2elem = new blender::Map<int, BMElem *>;
#endif

  BM_idmap_check_attributes(idmap);

  idmap->freelist.clear();
  if (idmap->free_idx_map) {
    MEM_delete(idmap->free_idx_map);
    idmap->free_idx_map = nullptr;
  }

  int max_id = 1;

  if (idmap->flag & BM_VERT) {
    BM_ITER_MESH (v, &iter, idmap->bm, BM_VERTS_OF_MESH) {
      int id = BM_ELEM_CD_GET_INT(v, idmap->cd_id_off[BM_VERT]);

      max_id = max_ii(max_id, id);
    }
  }
  if (idmap->flag & BM_EDGE) {
    BM_ITER_MESH (e, &iter, idmap->bm, BM_EDGES_OF_MESH) {
      int id = BM_ELEM_CD_GET_INT(e, idmap->cd_id_off[BM_EDGE]);

      max_id = max_ii(max_id, id);
    }
  }
  if (idmap->flag & (BM_FACE | BM_LOOP)) {
    BM_ITER_MESH (f, &iter, idmap->bm, BM_FACES_OF_MESH) {
      if (idmap->flag & BM_FACE) {
        int id = BM_ELEM_CD_GET_INT(f, idmap->cd_id_off[BM_FACE]);
        max_id = max_ii(max_id, id);
      }

      if (idmap->flag & BM_LOOP) {
        BMLoop *l = f->l_first;
        do {
          int id = BM_ELEM_CD_GET_INT(l, idmap->cd_id_off[BM_LOOP]);
          max_id = max_ii(max_id, id);
        } while ((l = l->next) != f->l_first);
      }
    }
  }

  max_id++;

  if (idmap->map_size >= max_id) {
    memset((void *)idmap->map, 0, sizeof(void *) * idmap->map_size);
  }
  else {
    MEM_SAFE_FREE(idmap->map);
    idmap->map_size = max_id + 1;
    idmap->map = (BMElem **)MEM_calloc_arrayN(max_id + 1, sizeof(BMElem *), "bm idmap->map");
  }

  auto check_elem = [&](auto *elem) {
    int id = BM_ELEM_CD_GET_INT(elem, idmap->cd_id_off[int(elem->head.htype)]);

    if (id == BM_ID_NONE || id < 0 || (id < idmap->map_size && idmap->map[id])) {
      // printf("%s: Allocating new id for %p(%d): %d\n", __func__, elem, id, max_id);
      id = max_id++;
      BM_ELEM_CD_SET_INT(elem, idmap->cd_id_off[int(elem->head.htype)], id);
    }

    idmap_grow_map(idmap, id);
    idmap->map[id] = reinterpret_cast<BMElem *>(elem);

#ifdef DEBUG_BM_IDMAP
    idmap_debug_insert(idmap, reinterpret_cast<BMElem *>(elem), id);
#endif
  };

  if (idmap->flag & BM_VERT) {
    BM_ITER_MESH (v, &iter, idmap->bm, BM_VERTS_OF_MESH) {
      check_elem(v);
    }
  }
  if (idmap->flag & BM_EDGE) {
    BM_ITER_MESH (e, &iter, idmap->bm, BM_EDGES_OF_MESH) {
      check_elem(e);
    }
  }
  if (idmap->flag & (BM_FACE | BM_LOOP)) {
    BM_ITER_MESH (f, &iter, idmap->bm, BM_FACES_OF_MESH) {
      check_elem(f);
      if (idmap->flag & BM_LOOP) {
        BMLoop *l = f->l_first;

        do {
          check_elem(l);
        } while ((l = l->next) != f->l_first);
      }
    }
  }

  idmap->maxid = max_id + 1;
}

void BM_idmap_check_attributes(BMIdMap *idmap)
{
  auto check_attr = [&](int type) {
    if (!(idmap->flag & type)) {
      return;
    }

    CustomData *cdata;
    const char *name;

    switch (type) {
      case BM_VERT:
        name = "vertex_id";
        cdata = &idmap->bm->vdata;
        break;
      case BM_EDGE:
        name = "edge_id";
        cdata = &idmap->bm->edata;
        break;
      case BM_LOOP:
        name = "corner_id";
        cdata = &idmap->bm->ldata;
        break;
      case BM_FACE:
        name = "face_id";
        cdata = &idmap->bm->pdata;
        break;
      default:
        BLI_assert_unreachable();
        return;
    }

    int idx = CustomData_get_named_layer_index(cdata, CD_PROP_INT32, name);

    if (idx == -1) {
      BM_data_layer_add_named(idmap->bm, cdata, CD_PROP_INT32, name);
      idx = CustomData_get_named_layer_index(cdata, CD_PROP_INT32, name);
    }

    cdata->layers[idx].flag |= CD_FLAG_ELEM_NOINTERP | CD_FLAG_ELEM_NOCOPY;

    idmap->cd_id_off[type] = cdata->layers[idx].offset;
  };

  check_attr(BM_VERT);
  check_attr(BM_EDGE);
  check_attr(BM_LOOP);
  check_attr(BM_FACE);
}

void BM_idmap_destroy(BMIdMap *idmap)
{
#ifdef DEBUG_BM_IDMAP
  delete idmap->elem2id;
  delete idmap->id2elem;
#endif

  if (idmap->free_idx_map) {
    MEM_delete(idmap->free_idx_map);
  }

  MEM_SAFE_FREE(idmap->map);
  MEM_delete(idmap);
}

static void check_idx_map(BMIdMap *idmap)
{
  if (idmap->free_idx_map && idmap->freelist.size() < FREELIST_HASHMAP_THRESHOLD_LOW) {
    // idmap_log_message("%s: Deleting free_idx_map\n", __func__);

    MEM_delete(idmap->free_idx_map);
    idmap->free_idx_map = nullptr;
  }
  else if (!idmap->free_idx_map && idmap->freelist.size() < FREELIST_HASHMAP_THRESHOLD_HIGH) {
    // idmap_log_message("%s: Adding free_idx_map\n", __func__);

    idmap->free_idx_map = MEM_new<BMIdMap::FreeIdxMap>("BMIdMap::FreeIdxMap");

    for (int i : IndexRange(idmap->freelist.size())) {
      idmap->free_idx_map->add(idmap->freelist[i], i);
    }
  }
}

int BM_idmap_alloc(BMIdMap *idmap, BMElem *elem)
{
  int id = BM_ID_NONE;

#ifdef DEBUG_BM_IDMAP
  int id2 = BM_ELEM_CD_GET_INT(elem, idmap->cd_id_off[int(elem->head.htype)]);

  if (idmap->elem2id->contains(elem)) {
    int id3 = idmap->elem2id->lookup(elem);

    if (id2 == id3) {
      idmap_log_message("%s: elem %p already had id %d\n", __func__, elem, id3);
    }
    else {
      idmap_log_message(
          "%s: elem %p already has an id (%d), but its attribute has the wrong one (%d)\n",
          __func__,
          elem,
          id3,
          id2);
    }

    idmap->elem2id->remove(elem);
  }

  if (idmap->id2elem->contains(id)) {
    idmap->id2elem->remove(id);
  }

#endif

  while (idmap->freelist.size()) {
    id = idmap->freelist.pop_last();

    if (id == BM_ID_NONE) {
      continue;
    }

    if (idmap->free_idx_map) {
      idmap->free_idx_map->remove(id);
    }

    break;
  }

  if (id == BM_ID_NONE) {
    id = idmap->maxid++;
  }

  idmap_grow_map(idmap, id);
  idmap->map[id] = elem;

  BM_ELEM_CD_SET_INT(elem, idmap->cd_id_off[int(elem->head.htype)], id);

#ifdef DEBUG_BM_IDMAP
  idmap_debug_insert(idmap, elem, id);
#endif

  return id;
}

void BM_idmap_assign(BMIdMap *idmap, BMElem *elem, int id)
{
  /* Remove id from freelist. */
  if (idmap->free_idx_map) {
    const int *val;

    if ((val = idmap->free_idx_map->lookup_ptr(id))) {
      idmap->freelist[*val] = BM_ID_NONE;
      idmap->free_idx_map->remove(id);
    }
  }
  else {
    for (int i : IndexRange(idmap->freelist.size())) {
      if (idmap->freelist[i] == id) {
        idmap->freelist[i] = BM_ID_NONE;
      }
    }
  }

  BM_ELEM_CD_SET_INT(elem, idmap->cd_id_off[int(elem->head.htype)], id);

  idmap_grow_map(idmap, id);
  idmap->map[id] = elem;

  check_idx_map(idmap);

#ifdef DEBUG_BM_IDMAP
  if (idmap->elem2id->contains(elem) && idmap->elem2id->lookup(elem) == id) {
    return;
  }

  if (idmap->elem2id->contains(elem)) {
    int id2 = idmap->elem2id->lookup(elem);

    idmap_log_message("%s: elem %p already had id %d, new id: %d\n", __func__, elem, id2, id);
    idmap->elem2id->remove(elem);
  }

  if (idmap->id2elem->contains(id)) {
    BMElem *elem2 = idmap->id2elem->lookup(id);
    if (elem2 != elem) {
      idmap_log_message("%s: elem %p (a %s) took over id from elem %p (a %s)\n",
                        __func__,
                        elem,
                        get_type_name(elem->head.htype),
                        elem2,
                        get_type_name(elem2->head.htype));
    }
  }

  idmap_debug_insert(idmap, elem, id);
  idmap_check_elem(idmap, elem);
#endif
}

ATTR_NO_OPT void BM_idmap_release(BMIdMap *idmap, BMElem *elem, bool clear_id)
{
#ifdef DEBUG_BM_IDMAP
  idmap_check_elem(idmap, elem);

  if (idmap->elem2id->contains(elem)) {
    int id2 = idmap->elem2id->lookup(elem);

    if (idmap->id2elem->contains(id2)) {
      idmap->id2elem->remove(id2);
    }
    idmap->elem2id->remove(elem);
  }
#endif

  int id = BM_ELEM_CD_GET_INT(elem, idmap->cd_id_off[int(elem->head.htype)]);

  if (id == BM_ID_NONE) {
    idmap_log_message("%s: unassigned id!\n", __func__);
    return;
  };
  if (id < 0 || id >= idmap->map_size || (idmap->map[id] && idmap->map[id] != elem)) {
    idmap_log_message("%s: id corruptions\n", __func__);
  }
  else {
    idmap->map[id] = nullptr;
  }

  idmap->freelist.append(id);

  if (idmap->free_idx_map) {
    idmap->free_idx_map->add(id, idmap->freelist.size() - 1);
  }

  check_idx_map(idmap);

  if (clear_id) {
    BM_ELEM_CD_SET_INT(elem, idmap->cd_id_off[int(elem->head.htype)], BM_ID_NONE);
  }
}

int BM_idmap_check_assign(BMIdMap *idmap, BMElem *elem)
{
  int id = BM_ELEM_CD_GET_INT(elem, idmap->cd_id_off[int(elem->head.htype)]);

  if (id == BM_ID_NONE) {
    id = BM_idmap_alloc(idmap, elem);
  }

#ifdef DEBUG_BM_IDMAP
  idmap_check_elem(idmap, elem);
#endif

  return id;
}
