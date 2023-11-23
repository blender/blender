#include "MEM_guardedalloc.h"

#include "BLI_assert.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "BKE_customdata.hh"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "bmesh_idmap.hh"
#include <cstdarg>
#include <cstdio>

using namespace blender;

/* Threshold of size of BMIDMap.freelist where .free_idx_map
 * (a hash map) will be created to find IDs inside the freelist.
 */
#define FREELIST_HASHMAP_THRESHOLD_HIGH 1024
#define FREELIST_HASHMAP_THRESHOLD_LOW 700

const char *BM_idmap_attr_name_get(int htype)
{
  switch (htype) {
    case BM_VERT:
      return "vertex_id";
    case BM_EDGE:
      return "edge_id";
    case BM_LOOP:
      return "corner_id";
    case BM_FACE:
      return "face_id";
    default:
      BLI_assert_unreachable();
      return "error";
  }
}

static void idmap_log_message(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

BMIdMap *BM_idmap_new(BMesh *bm, int elem_mask)
{
  BMIdMap *idmap = MEM_new<BMIdMap>("BMIdMap");

  for (int i = 0; i < ARRAY_SIZE(idmap->cd_id_off); i++) {
    idmap->cd_id_off[i] = -1;
  }

  idmap->flag = elem_mask;
  idmap->bm = bm;
  idmap->maxid = BM_ID_NONE + 1;

  BM_idmap_check_attributes(idmap);

  return idmap;
}

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

static void idmap_grow_map(BMIdMap *idmap, int newid)
{
  if (idmap->map.size() <= newid) {
    idmap->map.resize(newid + 1);
  }
}

void BM_idmap_clear_attributes_mesh(Mesh *me)
{
  CustomData_free_layer_named(&me->vert_data, BM_idmap_attr_name_get(BM_VERT), me->totvert);
  CustomData_free_layer_named(&me->edge_data, BM_idmap_attr_name_get(BM_EDGE), me->totedge);
  CustomData_free_layer_named(&me->loop_data, BM_idmap_attr_name_get(BM_LOOP), me->totloop);
  CustomData_free_layer_named(&me->face_data, BM_idmap_attr_name_get(BM_FACE), me->faces_num);
}

void BM_idmap_clear_attributes(BMesh *bm)
{
  BM_data_layer_free_named(bm, &bm->vdata, BM_idmap_attr_name_get(BM_VERT));
  BM_data_layer_free_named(bm, &bm->edata, BM_idmap_attr_name_get(BM_EDGE));
  BM_data_layer_free_named(bm, &bm->ldata, BM_idmap_attr_name_get(BM_LOOP));
  BM_data_layer_free_named(bm, &bm->pdata, BM_idmap_attr_name_get(BM_FACE));
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

  if (idmap->map.size() <= max_id) {
    idmap->map.resize(max_id);
  }

  /* Zero map. */
  memset(static_cast<void *>(idmap->map.data()), 0, sizeof(void *) * idmap->map.size());

  auto check_elem = [&](auto *elem) {
    int id = BM_ELEM_CD_GET_INT(elem, idmap->cd_id_off[int(elem->head.htype)]);

    if (id == BM_ID_NONE || id < 0 || (id < idmap->map.size() && idmap->map[id])) {
      id = max_id++;
      BM_ELEM_CD_SET_INT(elem, idmap->cd_id_off[int(elem->head.htype)], id);
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

  idmap->maxid = max_id + 1;
}

static bool bm_idmap_check_attr(BMIdMap *idmap, int type)
{
  if (!(idmap->flag & type)) {
    return false;
  }

  CustomData *cdata;
  const char *name = BM_idmap_attr_name_get(type);

  switch (type) {
    case BM_VERT:
      cdata = &idmap->bm->vdata;
      break;
    case BM_EDGE:
      cdata = &idmap->bm->edata;
      break;
    case BM_LOOP:
      cdata = &idmap->bm->ldata;
      break;
    case BM_FACE:
      cdata = &idmap->bm->pdata;
      break;
    default:
      BLI_assert_unreachable();
      return false;
  }

  int idx = CustomData_get_named_layer_index(cdata, CD_PROP_INT32, name);
  bool exists = idx != -1;

  if (!exists) {
    BM_data_layer_add_named(idmap->bm, cdata, CD_PROP_INT32, name);
    idx = CustomData_get_named_layer_index(cdata, CD_PROP_INT32, name);
  }

  cdata->layers[idx].flag |= CD_FLAG_ELEM_NOINTERP | CD_FLAG_ELEM_NOCOPY;
  idmap->cd_id_off[type] = cdata->layers[idx].offset;

  return !exists;
}

bool BM_idmap_check_attributes(BMIdMap *idmap)
{
  bool ret = false;

  ret |= bm_idmap_check_attr(idmap, BM_VERT);
  ret |= bm_idmap_check_attr(idmap, BM_EDGE);
  ret |= bm_idmap_check_attr(idmap, BM_LOOP);
  ret |= bm_idmap_check_attr(idmap, BM_FACE);

  return ret;
}

void BM_idmap_destroy(BMIdMap *idmap)
{
  if (idmap->free_idx_map) {
    MEM_delete(idmap->free_idx_map);
  }

  MEM_delete(idmap);
}

static void check_idx_map(BMIdMap *idmap)
{
  if (idmap->free_idx_map && idmap->freelist.size() < FREELIST_HASHMAP_THRESHOLD_LOW) {
    MEM_delete(idmap->free_idx_map);
    idmap->free_idx_map = nullptr;
  }
  else if (!idmap->free_idx_map && idmap->freelist.size() < FREELIST_HASHMAP_THRESHOLD_HIGH) {
    idmap->free_idx_map = MEM_new<BMIdMap::FreeIdxMap>("BMIdMap::FreeIdxMap");

    for (int i : IndexRange(idmap->freelist.size())) {
      idmap->free_idx_map->add(idmap->freelist[i], i);
    }
  }
}

template<typename T> int BM_idmap_alloc(BMIdMap *idmap, T *elem)
{
  int id = BM_ID_NONE;

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
  idmap->map[id] = reinterpret_cast<BMElem *>(elem);

  BM_ELEM_CD_SET_INT(elem, idmap->cd_id_off[int(elem->head.htype)], id);

  return id;
}

template<typename T> void BM_idmap_assign(BMIdMap *idmap, T *elem, int id)
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
  idmap->map[id] = reinterpret_cast<BMElem *>(elem);

  check_idx_map(idmap);
}

template<typename T> void BM_idmap_release(BMIdMap *idmap, T *elem, bool clear_id)
{
  int id = BM_ELEM_CD_GET_INT(elem, idmap->cd_id_off[int(elem->head.htype)]);

  if (id == BM_ID_NONE) {
    idmap_log_message("%s: unassigned id!\n", __func__);
    return;
  };
  if (id < 0 || id >= idmap->map.size() ||
      (idmap->map[id] && idmap->map[id] != reinterpret_cast<BMElem *>(elem)))
  {
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

template<typename T> int BM_idmap_check_assign(BMIdMap *idmap, T *elem)
{
  int id = BM_ELEM_CD_GET_INT(elem, idmap->cd_id_off[int(elem->head.htype)]);

  if (id == BM_ID_NONE) {
    id = BM_idmap_alloc(idmap, (BMElem *)elem);
  }

  return id;
}

/* Instantiate templates. */
template void BM_idmap_assign(BMIdMap *idmap, BMElem *elem, int id);
template void BM_idmap_assign(BMIdMap *idmap, BMVert *elem, int id);
template void BM_idmap_assign(BMIdMap *idmap, BMEdge *elem, int id);
template void BM_idmap_assign(BMIdMap *idmap, BMLoop *elem, int id);
template void BM_idmap_assign(BMIdMap *idmap, BMFace *elem, int id);

template int BM_idmap_check_assign(BMIdMap *idmap, BMElem *elem);
template int BM_idmap_check_assign(BMIdMap *idmap, BMVert *elem);
template int BM_idmap_check_assign(BMIdMap *idmap, BMEdge *elem);
template int BM_idmap_check_assign(BMIdMap *idmap, BMLoop *elem);
template int BM_idmap_check_assign(BMIdMap *idmap, BMFace *elem);

template int BM_idmap_alloc(BMIdMap *idmap, BMElem *elem);
template int BM_idmap_alloc(BMIdMap *idmap, BMVert *elem);
template int BM_idmap_alloc(BMIdMap *idmap, BMEdge *elem);
template int BM_idmap_alloc(BMIdMap *idmap, BMLoop *elem);
template int BM_idmap_alloc(BMIdMap *idmap, BMFace *elem);

template void BM_idmap_release(BMIdMap *idmap, BMElem *elem, bool clear_id);
template void BM_idmap_release(BMIdMap *idmap, BMVert *elem, bool clear_id);
template void BM_idmap_release(BMIdMap *idmap, BMEdge *elem, bool clear_id);
template void BM_idmap_release(BMIdMap *idmap, BMLoop *elem, bool clear_id);
template void BM_idmap_release(BMIdMap *idmap, BMFace *elem, bool clear_id);
