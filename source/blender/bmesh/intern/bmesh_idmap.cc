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
#include <cstdio>

#define BM_ID_NONE 0 //-1

using namespace blender;

#define FREELIST_HASHMAP_THRESHOLD_HIGH 1024
#define FREELIST_HASHMAP_THRESHOLD_LOW 700

BMIdMap *BM_idmap_new(BMesh *bm, int elem_mask)
{
  BMIdMap *idmap = MEM_new<BMIdMap>("BMIdMap");

  for (int i = 0; i < ARRAY_SIZE(idmap->cd_id_off); i++) {
    idmap->cd_id_off[i] = -1;
  }

  idmap->flag = elem_mask;
  idmap->bm = bm;
  idmap->maxid = 1;

  BM_idmap_check_attributes(idmap);

  return idmap;
}

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

  BM_idmap_check_attributes(idmap);

  idmap->freelist.clear();
  if (idmap->free_idx_map) {
    MEM_delete<BMIdMap::FreeIdxMap>(idmap->free_idx_map);
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
    int id = BM_ELEM_CD_GET_INT(elem, idmap->cd_id_off[(int)elem->head.htype]);

    if (id < 0 || id >= idmap->map_size || idmap->map[id]) {
      id = max_id++;
      BM_ELEM_CD_SET_INT(elem, idmap->cd_id_off[(int)elem->head.htype], id);
    }

    idmap_grow_map(idmap, id);
    idmap->map[id] = reinterpret_cast<BMElem *>(elem);
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
  if (idmap->flag & BM_VERT) {
    BM_ITER_MESH (v, &iter, idmap->bm, BM_VERTS_OF_MESH) {
      check_elem(v);
    }
  }

  idmap->maxid = max_id;
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

    if (!cdata->layers[idx].default_data) {
      cdata->layers[idx].default_data = MEM_cnew<MIntProperty>("MIntProperty");
    }

    cdata->layers[idx].flag |= CD_FLAG_ELEM_NOINTERP | CD_FLAG_ELEM_NOCOPY;

    int *default_data = static_cast<int *>(cdata->layers[idx].default_data);
    *default_data = BM_ID_NONE;

    idmap->cd_id_off[type] = cdata->layers[idx].offset;
  };

  check_attr(BM_VERT);
  check_attr(BM_EDGE);
  check_attr(BM_LOOP);
  check_attr(BM_FACE);
}

void BM_idmap_destroy(BMIdMap *idmap)
{
  MEM_SAFE_FREE(idmap->map);
  MEM_delete<BMIdMap>(idmap);
}

static void check_idx_map(BMIdMap *idmap)
{
  if (idmap->free_idx_map && idmap->freelist.size() < FREELIST_HASHMAP_THRESHOLD_LOW) {
    // printf("%s: Deleting free_idx_map\n", __func__);

    MEM_delete<BMIdMap::FreeIdxMap>(idmap->free_idx_map);
    idmap->free_idx_map = nullptr;
  }
  else if (!idmap->free_idx_map && idmap->freelist.size() < FREELIST_HASHMAP_THRESHOLD_HIGH) {
    // printf("%s: Adding free_idx_map\n", __func__);

    idmap->free_idx_map = MEM_new<BMIdMap::FreeIdxMap>("BMIdMap::FreeIdxMap");

    for (int i : IndexRange(idmap->freelist.size())) {
      idmap->free_idx_map->add(idmap->freelist[i], i);
    }
  }
}

int BM_idmap_alloc(BMIdMap *idmap, BMElem *elem)
{
  int id = BM_ID_NONE;
#ifdef USE_NEW_IDMAP

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

  BM_ELEM_CD_SET_INT(elem, idmap->cd_id_off[elem->head.htype], id);
#endif
  return id;
}

void BM_idmap_assign(BMIdMap *idmap, BMElem *elem, int id)
{
#ifdef USE_NEW_IDMAP
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

  BM_ELEM_CD_SET_INT(elem, idmap->cd_id_off[elem->head.htype], id);

  idmap_grow_map(idmap, id);
  idmap->map[id] = elem;

  check_idx_map(idmap);
#endif
}

void BM_idmap_release(BMIdMap *idmap, BMElem *elem, bool clear_id)
{
#ifdef USE_NEW_IDMAP
  int id = BM_ELEM_CD_GET_INT(elem, idmap->cd_id_off[(int)elem->head.htype]);

  if (id == BM_ID_NONE) {
    printf("%s: unassigned id!\n", __func__);
    return;
  };
  if (id < 0 || id >= idmap->map_size || (idmap->map[id] && idmap->map[id] != elem)) {
    printf("%s: id corruptions\n", __func__);
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
    BM_ELEM_CD_SET_INT(elem, idmap->cd_id_off[elem->head.htype], BM_ID_NONE);
  }
#endif
}

int BM_idmap_check_assign(BMIdMap *idmap, BMElem *elem)
{
  int id = BM_ELEM_CD_GET_INT(elem, idmap->cd_id_off[(int)elem->head.htype]);

  if (id == BM_ID_NONE) {
    return BM_idmap_alloc(idmap, elem);
  }

  return id;
}
