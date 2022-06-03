#include "DNA_customdata_types.h"

#include "BLI_index_range.hh"
#include "BLI_mempool.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "bmesh_data_attr.h"

#ifdef USE_BMESH_PAGE_CUSTOMDATA

using blender::IndexRange;
using blender::bmesh::BMAttrDomain;
using blender::bmesh::PageArray;
using blender::bmesh::PageElemRef;

namespace blender {
namespace bmesh {

BMeshAttrList *BMAttr_new()
{
  BMeshAttrList *list = static_cast<BMeshAttrList *>(
      MEM_callocN(sizeof(BMeshAttrList), "BMeshAttrList"));

  for (int i : IndexRange(ATTR_DOMAIN_NUM)) {
    list->domains[i] = new BMAttrDomain(static_cast<eAttrDomain>(i));
  }

  return list;
}

void BMAttr_reset(BMeshAttrList *list)
{
  if (list->arrays) {
    MEM_freeN(static_cast<void *>(list->arrays));
  }

  list->arrays = nullptr;
  list->totarray = 0;

  for (int i : IndexRange(ATTR_DOMAIN_NUM)) {
    delete list->domains[i];
  }

  for (int i : IndexRange(ATTR_DOMAIN_NUM)) {
    list->domains[i] = new BMAttrDomain(static_cast<eAttrDomain>(i));
  }
}

void BMAttr_free(BMeshAttrList *list)
{
  for (int i : IndexRange(ATTR_DOMAIN_NUM)) {
    delete list->domains[i];
  }

  if (list->arrays) {
    MEM_freeN(static_cast<void *>(list->arrays));
  }

  MEM_freeN(static_cast<void *>(list));
}

static void bm_update_page_pointers(BMeshAttrList *list)
{
  for (int i : IndexRange(list->totarray)) {
    PageArray<BM_PAGE_SHIFT> *page_array = reinterpret_cast<PageArray<BM_PAGE_SHIFT> *>(
        list->arrays[i]);
    list->arrays[i]->pages = page_array->pages.data();
  }
}

int BMAttr_allocElem(BMeshAttrList *list, eAttrDomain domain)
{
  bool hadNewPage;

  int ret = static_cast<int>(list->domains[domain]->alloc(hadNewPage));

  if (hadNewPage) {
    bm_update_page_pointers(list);
  }

  return ret;
}

void BMAttr_freeElem(BMeshAttrList *list, eAttrDomain domain, int elem)
{
  bool pageRemoved;

  list->domains[domain]->free(static_cast<PageElemRef>(elem), pageRemoved);

  if (pageRemoved) {
    bm_update_page_pointers(list);
  }
}

int BMAttr_addLayer(BMeshAttrList *list, eAttrDomain domain, eCustomDataType type)
{
  list->totarray++;

  if (!list->arrays) {
    list->arrays = static_cast<BMeshPageArray **>(
        MEM_malloc_arrayN(list->totarray, sizeof(void *), __func__));
  }
  else {
    list->arrays = static_cast<BMeshPageArray **>(
        MEM_reallocN(static_cast<void *>(list->arrays), list->totarray * sizeof(void *)));
  }

  BMeshPageArray *bmarray = list->arrays[list->totarray - 1];
  PageArray<BM_PAGE_SHIFT> *page_array = list->domains[domain]->addLayer(type);

  bmarray->cppClass = reinterpret_cast<void *>(page_array);
  bmarray->pages = page_array->pages.data();

  for (PageElemRef i : IndexRange(list->domains[domain]->totalloc)) {
    page_array->setDefault(i);
  }

  return list->totarray - 1;
}

void BMAttr_init(BMesh *bm)
{
#  ifdef USE_BMESH_PAGE_CUSTOMDATA
  CustomData *domains[ATTR_DOMAIN_NUM] = {nullptr};

  domains[ATTR_DOMAIN_POINT] = &bm->vdata;
  domains[ATTR_DOMAIN_EDGE] = &bm->edata;
  domains[ATTR_DOMAIN_CORNER] = &bm->ldata;
  domains[ATTR_DOMAIN_FACE] = &bm->pdata;

  if (!bm->attr_list) {
    bm->attr_list = BMAttr_new();
  }

  BMAttr_fromCData(bm->attr_list, domains);
#  endif
}

void BMAttr_fromCData(BMeshAttrList *list, CustomData *domains[ATTR_DOMAIN_NUM])
{
  eAttrDomain ds[4] = {ATTR_DOMAIN_POINT, ATTR_DOMAIN_EDGE, ATTR_DOMAIN_CORNER, ATTR_DOMAIN_FACE};

  for (int i : IndexRange(4)) {
    CustomData *cdata = domains[ds[i]];

    cdata->bm_attrs = static_cast<void *>(list);
    cdata->_pad[0] = static_cast<void *>(domains[ds[i]]);

    for (int j : IndexRange(cdata->totlayer)) {
      CustomDataLayer *layer = cdata->layers + j;

      layer->offset = BMAttr_addLayer(list, ds[i], static_cast<eCustomDataType>(layer->type));
    }
  }
}

static eAttrDomain domain_map[] = {
    ATTR_DOMAIN_AUTO,    // 0
    ATTR_DOMAIN_POINT,   // 1
    ATTR_DOMAIN_EDGE,    // 2
    ATTR_DOMAIN_AUTO,    // 3
    ATTR_DOMAIN_CORNER,  // 4
    ATTR_DOMAIN_AUTO,    // 5
    ATTR_DOMAIN_AUTO,    // 6
    ATTR_DOMAIN_AUTO,    // 7
    ATTR_DOMAIN_FACE,    // 8
};

void bmesh_update_attr_refs(BMesh *bm)
{
  CustomData *cdata = &bm->vdata;

  for (int i = 0; i < 4; i++, cdata++) {
    cdata->_pad[0] = static_cast<void *>(domain_map[htype]);
    cdata->bm_attrs = bm->attr_list;
  }
}

void CustomData_bmesh_init_pool(CustomData *data, int totelem, const char htype)
{
  CustomData_bmesh_init_pool_ex(data, totelem, htype, __func__);
}

void CustomData_bmesh_init_pool_ex(CustomData *data,
                                   int totelem,
                                   const char htype,
                                   const char *memtag)
{
  // store domain in _pad[0] for the purposes of this test
  data->_pad[0] = static_cast<void *>(domain_map[htype]);

  if (data->pool) {
    BLI_mempool_destroy(data->pool);
  }

  data->pool = BLI_mempool_create(sizeof(BMeshPageRef), 0, 1024, 0);
}

static void CustomData_bmesh_alloc_block(CustomData *data, void **block)
{
  BMeshAttrList *list = static_cast<BMeshAttrList *>(data->bm_attrs);
  BMeshPageRef *ref = static_cast<BMeshPageRef *>(BLI_mempool_calloc(data->pool));

  eAttrDomain domain = static_cast<eAttrDomain>(POINTER_AS_UINT(data->_pad[0]));

  ref->attrs = list;
  ref->idx = BMAttr_allocElem(list, domain);
  ref->domain = domain;

  *block = static_cast<void *>(ref);
}

static void CustomData_bmesh_set_default_n(CustomData *data, void **block, int n)
{
  if (ELEM(data->layers[n].type, CD_TOOLFLAGS, CD_MESH_ID)) {
    /* do not do toolflags or mesh ids */
    return;
  }

  BMeshAttrList *list = static_cast<BMeshAttrList *>(data->bm_attrs);
  BMeshPageRef *ref = static_cast<BMeshPageRef *>(*block);

  list->domains[ref->domain]->arrays[n]->setDefault(static_cast<PageElemRef>(ref->idx));
}

static void CustomData_bmesh_set_default(CustomData *data, void **block)
{
  if (!*block) {
    CustomData_bmesh_alloc_block(data, block, n);
  }

  BMeshAttrList *list = static_cast<BMeshAttrList *>(data->bm_attrs);
  BMeshPageRef *ref = static_cast<BMeshPageRef *>(*block);
  eAttrDomain domain = static_cast<eAttrDomain>(POINTER_AS_UINT(data->_pad[0]));

  list->domains[domain].setDefault(static_cast<PageElemRef>(ref->idx));
}

static void alloc_block(CustomData *data, BMeshAttrList *list, eAttrDomain domain, void **block)
{
  CustomData_bmesh_alloc_block(data, block);
  BMeshPageRef *ref = static_cast<BMeshPageRef *> * block;

  ref->attrs = list;
  ref->idx = BMAttr_allocElem(list, domain);
}

void CustomData_bmesh_interp(CustomData *data,
                             const void **src_blocks,
                             const float *weights,
                             const float *sub_weights,
                             int count,
                             void *dst_block)
{
  BMeshPageRef *ref = static_cast<BMeshPageRef *> dst_block;
  eAttrDomain domain = static_cast<eAttrDomain>(ref->domain);

  PageElemRef *elems = BLI_array_alloca(elems, count);

  for (int i : IndexRange(count)) {
    BMeshPageRef *ref2 = static_cast<BMeshPageRef *> src_blocks[i];
    elems[i] = ref2->idx;
  }

  ref->attrs->domains[domain]->interp(
      static_cast<PageElemRef>(ref->idx), count, elems, weights, sub_weights);
}

void CustomData_to_bmesh_block(const CustomData *source,
                               CustomData *_dest,
                               int src_index,
                               void **dest_block,
                               bool use_default_init)
{
  if (*dest_block == nullptr) {
    CustomData_bmesh_alloc_block(dest, dest_block);
  }

  BMeshPageRef *block = static_cast<BMeshPageRef *> dest_block;
  BMeshAttrList *list = static_cast<BMeshAttrList *> dest->bm_attrs;
  auto *dest = list->domains[ref->domain];

  /* copies a layer at a time */
  int dest_i = 0;
  for (int src_i = 0; src_i < source->totlayer; src_i++) {
    auto *array = list->domains[ref->domain]->arrays[dest_i];

    /* find the first dest layer with type >= the source type
     * (this should work because layers are ordered by type)
     */
    while (dest_i < dest->arrays.size() &&
           dest->arrays[dest_i].type < source->layers[src_i].type) {
      if (use_default_init) {
        CustomData_bmesh_set_default_n(dest, dest_block, dest_i);
      }
      dest_i++;
    }

    /* if there are no more dest layers, we're done */
    if (dest_i >= dest->arrays.size()) {
      break;
    }

    /* if we found a matching layer, copy the data */
    if (dest->arrays[dest_i].type == source->layers[src_i].type) {
      const void *src_data = source->layers[src_i].data;
      void *dest_data = dest->getElemPtr(static_cast<PageElemRef>(ref->idx));

      const LayerTypeInfo *typeInfo = layerType_getInfo(dest->arrays[dest_i].type);
      const size_t src_offset = (size_t)src_index * typeInfo->size;

      if (typeInfo->copy) {
        typeInfo->copy(POINTER_OFFSET(src_data, src_offset), dest_data, 1);
      }
      else {
        memcpy(dest_data, POINTER_OFFSET(src_data, src_offset), typeInfo->size);
      }

      /* if there are multiple source & dest layers of the same type,
       * we don't want to copy all source layers to the same dest, so
       * increment dest_i
       */
      dest_i++;
    }
  }

  if (use_default_init) {
    while (dest_i < dest->arrays.size()) {
      CustomData_bmesh_set_default_n(dest, dest_block, dest_i);
      dest_i++;
    }
  }
}

void CustomData_from_bmesh_block(const CustomData *source,
                                 CustomData *dest,
                                 void *src_block,
                                 int dest_index)
{
  /* copies a layer at a time */
  int dest_i = 0;
  for (int src_i = 0; src_i < source->totlayer; src_i++) {
    if (source->layers[src_i].flag & CD_FLAG_NOCOPY) {
      continue;
    }

    /* find the first dest layer with type >= the source type
     * (this should work because layers are ordered by type)
     */
    while (dest_i < dest->totlayer && dest->layers[dest_i].type < source->layers[src_i].type) {
      dest_i++;
    }

    /* if there are no more dest layers, we're done */
    if (dest_i >= dest->totlayer) {
      return;
    }

    /* if we found a matching layer, copy the data */
    if (dest->layers[dest_i].type == source->layers[src_i].type) {
      const LayerTypeInfo *typeInfo = layerType_getInfo(dest->layers[dest_i].type);
      int offset = source->layers[src_i].offset;
      const void *src_data = POINTER_OFFSET(src_block, offset);
      void *dst_data = POINTER_OFFSET(dest->layers[dest_i].data,
                                      (size_t)dest_index * typeInfo->size);

      if (typeInfo->copy) {
        typeInfo->copy(src_data, dst_data, 1);
      }
      else {
        memcpy(dst_data, src_data, typeInfo->size);
      }

      /* if there are multiple source & dest layers of the same type,
       * we don't want to copy all source layers to the same dest, so
       * increment dest_i
       */
      dest_i++;
    }
  }
}

}  // namespace bmesh
}  // namespace blender
#endif
