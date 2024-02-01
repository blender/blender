/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

/**
 * DRW Instance Data Manager
 * This is a special memory manager that keeps memory blocks ready to send as VBO data in one
 * continuous allocation. This way we avoid feeding #GPUBatch each instance data one by one and
 * unnecessary memcpy. Since we lose which memory block was used each #DRWShadingGroup we need to
 * redistribute them in the same order/size to avoid to realloc each frame. This is why
 * #DRWInstanceDatas are sorted in a list for each different data size.
 */

#include "draw_instance_data.h"
#include "draw_manager.h"

#include "DRW_engine.hh"
#include "DRW_render.hh" /* For DRW_shgroup_get_instance_count() */

#include "GPU_material.hh"

#include "DNA_particle_types.h"

#include "BKE_duplilist.h"

#include "RNA_access.hh"
#include "RNA_path.hh"

#include "BLI_bitmap.h"
#include "BLI_memblock.h"
#include "BLI_mempool.h"
#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

struct DRWInstanceData {
  DRWInstanceData *next;
  bool used;        /* If this data is used or not. */
  size_t data_size; /* Size of one instance data. */
  BLI_mempool *mempool;
};

struct DRWInstanceDataList {
  DRWInstanceDataList *next, *prev;
  /* Linked lists for all possible data pool size */
  DRWInstanceData *idata_head[MAX_INSTANCE_DATA_SIZE];
  DRWInstanceData *idata_tail[MAX_INSTANCE_DATA_SIZE];

  BLI_memblock *pool_instancing;
  BLI_memblock *pool_batching;
  BLI_memblock *pool_buffers;
};

struct DRWTempBufferHandle {
  GPUVertBuf *buf;
  /** Format pointer for reuse. */
  GPUVertFormat *format;
  /** Touched vertex length for resize. */
  int *vert_len;
};

struct DRWTempInstancingHandle {
  /** Copy of geom but with the per-instance attributes. */
  GPUBatch *batch;
  /** Batch containing instancing attributes. */
  GPUBatch *instancer;
  /** Call-buffer to be used instead of instancer. */
  GPUVertBuf *buf;
  /** Original non-instanced batch pointer. */
  GPUBatch *geom;
};

static ListBase g_idatalists = {nullptr, nullptr};

static void instancing_batch_references_add(GPUBatch *batch)
{
  for (int i = 0; i < GPU_BATCH_VBO_MAX_LEN && batch->verts[i]; i++) {
    GPU_vertbuf_handle_ref_add(batch->verts[i]);
  }
  for (int i = 0; i < GPU_BATCH_INST_VBO_MAX_LEN && batch->inst[i]; i++) {
    GPU_vertbuf_handle_ref_add(batch->inst[i]);
  }
}

static void instancing_batch_references_remove(GPUBatch *batch)
{
  for (int i = 0; i < GPU_BATCH_VBO_MAX_LEN && batch->verts[i]; i++) {
    GPU_vertbuf_handle_ref_remove(batch->verts[i]);
  }
  for (int i = 0; i < GPU_BATCH_INST_VBO_MAX_LEN && batch->inst[i]; i++) {
    GPU_vertbuf_handle_ref_remove(batch->inst[i]);
  }
}

/* -------------------------------------------------------------------- */
/** \name Instance Buffer Management
 * \{ */

GPUVertBuf *DRW_temp_buffer_request(DRWInstanceDataList *idatalist,
                                    GPUVertFormat *format,
                                    int *vert_len)
{
  BLI_assert(format != nullptr);
  BLI_assert(vert_len != nullptr);

  DRWTempBufferHandle *handle = static_cast<DRWTempBufferHandle *>(
      BLI_memblock_alloc(idatalist->pool_buffers));

  if (handle->format != format) {
    handle->format = format;
    GPU_VERTBUF_DISCARD_SAFE(handle->buf);

    GPUVertBuf *vert = GPU_vertbuf_calloc();
    GPU_vertbuf_init_with_format_ex(vert, format, GPU_USAGE_DYNAMIC);
    GPU_vertbuf_data_alloc(vert, DRW_BUFFER_VERTS_CHUNK);

    handle->buf = vert;
  }
  handle->vert_len = vert_len;
  return handle->buf;
}

GPUBatch *DRW_temp_batch_instance_request(DRWInstanceDataList *idatalist,
                                          GPUVertBuf *buf,
                                          GPUBatch *instancer,
                                          GPUBatch *geom)
{
  /* Do not call this with a batch that is already an instancing batch. */
  BLI_assert(geom->inst[0] == nullptr);
  /* Only call with one of them. */
  BLI_assert((instancer != nullptr) != (buf != nullptr));

  DRWTempInstancingHandle *handle = static_cast<DRWTempInstancingHandle *>(
      BLI_memblock_alloc(idatalist->pool_instancing));
  if (handle->batch == nullptr) {
    handle->batch = GPU_batch_calloc();
  }

  GPUBatch *batch = handle->batch;
  bool instancer_compat = buf ? ((batch->inst[0] == buf) &&
                                 (GPU_vertbuf_get_status(buf) & GPU_VERTBUF_DATA_UPLOADED)) :
                                ((batch->inst[0] == instancer->verts[0]) &&
                                 (batch->inst[1] == instancer->verts[1]));
  bool is_compatible = (batch->prim_type == geom->prim_type) && instancer_compat &&
                       (batch->flag & GPU_BATCH_BUILDING) == 0 && (batch->elem == geom->elem);
  for (int i = 0; i < GPU_BATCH_VBO_MAX_LEN && is_compatible; i++) {
    if (batch->verts[i] != geom->verts[i]) {
      is_compatible = false;
    }
  }

  if (!is_compatible) {
    instancing_batch_references_remove(batch);
    GPU_batch_clear(batch);
    /* Save args and init later. */
    batch->flag = GPU_BATCH_BUILDING;
    handle->buf = buf;
    handle->instancer = instancer;
    handle->geom = geom;
  }
  return batch;
}

GPUBatch *DRW_temp_batch_request(DRWInstanceDataList *idatalist,
                                 GPUVertBuf *buf,
                                 GPUPrimType prim_type)
{
  GPUBatch **batch_ptr = static_cast<GPUBatch **>(BLI_memblock_alloc(idatalist->pool_batching));
  if (*batch_ptr == nullptr) {
    *batch_ptr = GPU_batch_calloc();
  }

  GPUBatch *batch = *batch_ptr;
  bool is_compatible = (batch->verts[0] == buf) && (batch->prim_type == prim_type) &&
                       (GPU_vertbuf_get_status(buf) & GPU_VERTBUF_DATA_UPLOADED);
  if (!is_compatible) {
    GPU_batch_clear(batch);
    GPU_batch_init(batch, prim_type, buf, nullptr);
  }
  return batch;
}

static void temp_buffer_handle_free(DRWTempBufferHandle *handle)
{
  handle->format = nullptr;
  GPU_VERTBUF_DISCARD_SAFE(handle->buf);
}

static void temp_instancing_handle_free(DRWTempInstancingHandle *handle)
{
  instancing_batch_references_remove(handle->batch);
  GPU_BATCH_DISCARD_SAFE(handle->batch);
}

static void temp_batch_free(GPUBatch **batch)
{
  GPU_BATCH_DISCARD_SAFE(*batch);
}

void DRW_instance_buffer_finish(DRWInstanceDataList *idatalist)
{
  /* Resize down buffers in use and send data to GPU. */
  BLI_memblock_iter iter;
  DRWTempBufferHandle *handle;
  BLI_memblock_iternew(idatalist->pool_buffers, &iter);
  while ((handle = static_cast<DRWTempBufferHandle *>(BLI_memblock_iterstep(&iter)))) {
    if (handle->vert_len != nullptr) {
      uint vert_len = *(handle->vert_len);
      uint target_buf_size = ((vert_len / DRW_BUFFER_VERTS_CHUNK) + 1) * DRW_BUFFER_VERTS_CHUNK;
      if (target_buf_size < GPU_vertbuf_get_vertex_alloc(handle->buf)) {
        GPU_vertbuf_data_resize(handle->buf, target_buf_size);
      }
      GPU_vertbuf_data_len_set(handle->buf, vert_len);
      GPU_vertbuf_use(handle->buf); /* Send data. */
    }
  }
  /* Finish pending instancing batches. */
  DRWTempInstancingHandle *handle_inst;
  BLI_memblock_iternew(idatalist->pool_instancing, &iter);
  while ((handle_inst = static_cast<DRWTempInstancingHandle *>(BLI_memblock_iterstep(&iter)))) {
    GPUBatch *batch = handle_inst->batch;
    if (batch && batch->flag == GPU_BATCH_BUILDING) {
      GPUVertBuf *inst_buf = handle_inst->buf;
      GPUBatch *inst_batch = handle_inst->instancer;
      GPUBatch *geom = handle_inst->geom;
      GPU_batch_copy(batch, geom);
      if (inst_batch != nullptr) {
        for (int i = 0; i < GPU_BATCH_INST_VBO_MAX_LEN && inst_batch->verts[i]; i++) {
          GPU_batch_instbuf_add(batch, inst_batch->verts[i], false);
        }
      }
      else {
        GPU_batch_instbuf_add(batch, inst_buf, false);
      }
      /* Add reference to avoid comparing pointers (in DRW_temp_batch_request) that could
       * potentially be the same. This will delay the freeing of the GPUVertBuf itself. */
      instancing_batch_references_add(batch);
    }
  }
  /* Resize pools and free unused. */
  BLI_memblock_clear(idatalist->pool_buffers, (MemblockValFreeFP)temp_buffer_handle_free);
  BLI_memblock_clear(idatalist->pool_instancing, (MemblockValFreeFP)temp_instancing_handle_free);
  BLI_memblock_clear(idatalist->pool_batching, (MemblockValFreeFP)temp_batch_free);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Instance Data (DRWInstanceData)
 * \{ */

static DRWInstanceData *drw_instance_data_create(DRWInstanceDataList *idatalist, uint attr_size)
{
  DRWInstanceData *idata = static_cast<DRWInstanceData *>(
      MEM_callocN(sizeof(DRWInstanceData), "DRWInstanceData"));
  idata->next = nullptr;
  idata->used = true;
  idata->data_size = attr_size;
  idata->mempool = BLI_mempool_create(sizeof(float) * idata->data_size, 0, 16, 0);

  BLI_assert(attr_size > 0);

  /* Push to linked list. */
  if (idatalist->idata_head[attr_size - 1] == nullptr) {
    idatalist->idata_head[attr_size - 1] = idata;
  }
  else {
    idatalist->idata_tail[attr_size - 1]->next = idata;
  }
  idatalist->idata_tail[attr_size - 1] = idata;

  return idata;
}

static void DRW_instance_data_free(DRWInstanceData *idata)
{
  BLI_mempool_destroy(idata->mempool);
}

void *DRW_instance_data_next(DRWInstanceData *idata)
{
  return BLI_mempool_alloc(idata->mempool);
}

DRWInstanceData *DRW_instance_data_request(DRWInstanceDataList *idatalist, uint attr_size)
{
  BLI_assert(attr_size > 0 && attr_size <= MAX_INSTANCE_DATA_SIZE);

  DRWInstanceData *idata = idatalist->idata_head[attr_size - 1];

  /* Search for an unused data chunk. */
  for (; idata; idata = idata->next) {
    if (idata->used == false) {
      idata->used = true;
      return idata;
    }
  }

  return drw_instance_data_create(idatalist, attr_size);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Instance Data List (DRWInstanceDataList)
 * \{ */

DRWInstanceDataList *DRW_instance_data_list_create()
{
  DRWInstanceDataList *idatalist = static_cast<DRWInstanceDataList *>(
      MEM_callocN(sizeof(DRWInstanceDataList), "DRWInstanceDataList"));

  idatalist->pool_batching = BLI_memblock_create(sizeof(GPUBatch *));
  idatalist->pool_instancing = BLI_memblock_create(sizeof(DRWTempInstancingHandle));
  idatalist->pool_buffers = BLI_memblock_create(sizeof(DRWTempBufferHandle));

  BLI_addtail(&g_idatalists, idatalist);

  return idatalist;
}

void DRW_instance_data_list_free(DRWInstanceDataList *idatalist)
{
  DRWInstanceData *idata, *next_idata;

  for (int i = 0; i < MAX_INSTANCE_DATA_SIZE; i++) {
    for (idata = idatalist->idata_head[i]; idata; idata = next_idata) {
      next_idata = idata->next;
      DRW_instance_data_free(idata);
      MEM_freeN(idata);
    }
    idatalist->idata_head[i] = nullptr;
    idatalist->idata_tail[i] = nullptr;
  }

  BLI_memblock_destroy(idatalist->pool_buffers, (MemblockValFreeFP)temp_buffer_handle_free);
  BLI_memblock_destroy(idatalist->pool_instancing, (MemblockValFreeFP)temp_instancing_handle_free);
  BLI_memblock_destroy(idatalist->pool_batching, (MemblockValFreeFP)temp_batch_free);

  BLI_remlink(&g_idatalists, idatalist);

  MEM_freeN(idatalist);
}

void DRW_instance_data_list_reset(DRWInstanceDataList *idatalist)
{
  DRWInstanceData *idata;

  for (int i = 0; i < MAX_INSTANCE_DATA_SIZE; i++) {
    for (idata = idatalist->idata_head[i]; idata; idata = idata->next) {
      idata->used = false;
    }
  }
}

void DRW_instance_data_list_free_unused(DRWInstanceDataList *idatalist)
{
  DRWInstanceData *idata, *next_idata;

  /* Remove unused data blocks and sanitize each list. */
  for (int i = 0; i < MAX_INSTANCE_DATA_SIZE; i++) {
    idatalist->idata_tail[i] = nullptr;
    for (idata = idatalist->idata_head[i]; idata; idata = next_idata) {
      next_idata = idata->next;
      if (idata->used == false) {
        if (idatalist->idata_head[i] == idata) {
          idatalist->idata_head[i] = next_idata;
        }
        else {
          /* idatalist->idata_tail[i] is guaranteed not to be null in this case. */
          idatalist->idata_tail[i]->next = next_idata;
        }
        DRW_instance_data_free(idata);
        MEM_freeN(idata);
      }
      else {
        if (idatalist->idata_tail[i] != nullptr) {
          idatalist->idata_tail[i]->next = idata;
        }
        idatalist->idata_tail[i] = idata;
      }
    }
  }
}

void DRW_instance_data_list_resize(DRWInstanceDataList *idatalist)
{
  DRWInstanceData *idata;

  for (int i = 0; i < MAX_INSTANCE_DATA_SIZE; i++) {
    for (idata = idatalist->idata_head[i]; idata; idata = idata->next) {
      BLI_mempool_clear_ex(idata->mempool, BLI_mempool_len(idata->mempool));
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sparse Uniform Buffer
 * \{ */

#define CHUNK_LIST_STEP (1 << 4)

/** A chunked UBO manager that doesn't actually allocate unneeded chunks. */
struct DRWSparseUniformBuf {
  /* Memory buffers used to stage chunk data before transfer to UBOs. */
  char **chunk_buffers;
  /* Uniform buffer objects with flushed data. */
  GPUUniformBuf **chunk_ubos;
  /* True if the relevant chunk contains data (distinct from simply being allocated). */
  BLI_bitmap *chunk_used;

  int num_chunks;
  uint item_size, chunk_size, chunk_bytes;
};

static void drw_sparse_uniform_buffer_init(DRWSparseUniformBuf *buffer,
                                           uint item_size,
                                           uint chunk_size)
{
  buffer->chunk_buffers = nullptr;
  buffer->chunk_used = nullptr;
  buffer->chunk_ubos = nullptr;
  buffer->num_chunks = 0;
  buffer->item_size = item_size;
  buffer->chunk_size = chunk_size;
  buffer->chunk_bytes = item_size * chunk_size;
}

DRWSparseUniformBuf *DRW_sparse_uniform_buffer_new(uint item_size, uint chunk_size)
{
  DRWSparseUniformBuf *buf = static_cast<DRWSparseUniformBuf *>(
      MEM_mallocN(sizeof(DRWSparseUniformBuf), __func__));
  drw_sparse_uniform_buffer_init(buf, item_size, chunk_size);
  return buf;
}

void DRW_sparse_uniform_buffer_flush(DRWSparseUniformBuf *buffer)
{
  for (int i = 0; i < buffer->num_chunks; i++) {
    if (BLI_BITMAP_TEST(buffer->chunk_used, i)) {
      if (buffer->chunk_ubos[i] == nullptr) {
        buffer->chunk_ubos[i] = GPU_uniformbuf_create(buffer->chunk_bytes);
      }
      GPU_uniformbuf_update(buffer->chunk_ubos[i], buffer->chunk_buffers[i]);
    }
  }
}

void DRW_sparse_uniform_buffer_clear(DRWSparseUniformBuf *buffer, bool free_all)
{
  int max_used_chunk = 0;

  for (int i = 0; i < buffer->num_chunks; i++) {
    /* Delete buffers that were not used since the last clear call. */
    if (free_all || !BLI_BITMAP_TEST(buffer->chunk_used, i)) {
      MEM_SAFE_FREE(buffer->chunk_buffers[i]);

      if (buffer->chunk_ubos[i]) {
        GPU_uniformbuf_free(buffer->chunk_ubos[i]);
        buffer->chunk_ubos[i] = nullptr;
      }
    }
    else {
      max_used_chunk = i + 1;
    }
  }

  /* Shrink the chunk array if appropriate. */
  const int old_num_chunks = buffer->num_chunks;

  buffer->num_chunks = (max_used_chunk + CHUNK_LIST_STEP - 1) & ~(CHUNK_LIST_STEP - 1);

  if (buffer->num_chunks == 0) {
    /* Ensure that an empty pool holds no memory allocations. */
    MEM_SAFE_FREE(buffer->chunk_buffers);
    MEM_SAFE_FREE(buffer->chunk_used);
    MEM_SAFE_FREE(buffer->chunk_ubos);
    return;
  }

  if (buffer->num_chunks != old_num_chunks) {
    buffer->chunk_buffers = static_cast<char **>(
        MEM_recallocN(buffer->chunk_buffers, buffer->num_chunks * sizeof(void *)));
    buffer->chunk_ubos = static_cast<GPUUniformBuf **>(
        MEM_recallocN(buffer->chunk_ubos, buffer->num_chunks * sizeof(void *)));
    BLI_BITMAP_RESIZE(buffer->chunk_used, buffer->num_chunks);
  }

  BLI_bitmap_set_all(buffer->chunk_used, false, buffer->num_chunks);
}

void DRW_sparse_uniform_buffer_free(DRWSparseUniformBuf *buffer)
{
  DRW_sparse_uniform_buffer_clear(buffer, true);
  MEM_freeN(buffer);
}

bool DRW_sparse_uniform_buffer_is_empty(DRWSparseUniformBuf *buffer)
{
  return buffer->num_chunks == 0;
}

static GPUUniformBuf *drw_sparse_uniform_buffer_get_ubo(DRWSparseUniformBuf *buffer, int chunk)
{
  if (buffer && chunk < buffer->num_chunks && BLI_BITMAP_TEST(buffer->chunk_used, chunk)) {
    return buffer->chunk_ubos[chunk];
  }
  return nullptr;
}

void DRW_sparse_uniform_buffer_bind(DRWSparseUniformBuf *buffer, int chunk, int location)
{
  GPUUniformBuf *ubo = drw_sparse_uniform_buffer_get_ubo(buffer, chunk);
  if (ubo) {
    GPU_uniformbuf_bind(ubo, location);
  }
}

void DRW_sparse_uniform_buffer_unbind(DRWSparseUniformBuf *buffer, int chunk)
{
  GPUUniformBuf *ubo = drw_sparse_uniform_buffer_get_ubo(buffer, chunk);
  if (ubo) {
    GPU_uniformbuf_unbind(ubo);
  }
}

void *DRW_sparse_uniform_buffer_ensure_item(DRWSparseUniformBuf *buffer, int chunk, int item)
{
  if (chunk >= buffer->num_chunks) {
    buffer->num_chunks = (chunk + CHUNK_LIST_STEP) & ~(CHUNK_LIST_STEP - 1);
    buffer->chunk_buffers = static_cast<char **>(
        MEM_recallocN(buffer->chunk_buffers, buffer->num_chunks * sizeof(void *)));
    buffer->chunk_ubos = static_cast<GPUUniformBuf **>(
        MEM_recallocN(buffer->chunk_ubos, buffer->num_chunks * sizeof(void *)));
    BLI_BITMAP_RESIZE(buffer->chunk_used, buffer->num_chunks);
  }

  char *chunk_buffer = buffer->chunk_buffers[chunk];

  if (chunk_buffer == nullptr) {
    buffer->chunk_buffers[chunk] = chunk_buffer = static_cast<char *>(
        MEM_callocN(buffer->chunk_bytes, __func__));
  }
  else if (!BLI_BITMAP_TEST(buffer->chunk_used, chunk)) {
    memset(chunk_buffer, 0, buffer->chunk_bytes);
  }

  BLI_BITMAP_ENABLE(buffer->chunk_used, chunk);

  return chunk_buffer + buffer->item_size * item;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform Attribute Buffers
 * \{ */

/** Sparse UBO buffer for a specific uniform attribute list. */
struct DRWUniformAttrBuf {
  /* Attribute list (also used as hash table key) handled by this buffer. */
  GPUUniformAttrList key;
  /* Sparse UBO buffer containing the attribute values. */
  DRWSparseUniformBuf ubos;
  /* Last handle used to update the buffer, checked for avoiding redundant updates. */
  DRWResourceHandle last_handle;
  /* Linked list pointer used for freeing the empty unneeded buffers. */
  DRWUniformAttrBuf *next_empty;
};

static DRWUniformAttrBuf *drw_uniform_attrs_pool_ensure(GHash *table,
                                                        const GPUUniformAttrList *key)
{
  void **pkey, **pval;

  if (!BLI_ghash_ensure_p_ex(table, key, &pkey, &pval)) {
    DRWUniformAttrBuf *buffer = static_cast<DRWUniformAttrBuf *>(
        MEM_callocN(sizeof(*buffer), __func__));

    *pkey = &buffer->key;
    *pval = buffer;

    GPU_uniform_attr_list_copy(&buffer->key, key);
    drw_sparse_uniform_buffer_init(
        &buffer->ubos, key->count * sizeof(float[4]), DRW_RESOURCE_CHUNK_LEN);

    buffer->last_handle = (DRWResourceHandle)-1;
  }

  return (DRWUniformAttrBuf *)*pval;
}

static void drw_uniform_attribute_lookup(const GPUUniformAttr *attr,
                                         const Object *ob,
                                         const Object *dupli_parent,
                                         const DupliObject *dupli_source,
                                         float r_data[4])
{
  /* If requesting instance data, check the parent particle system and object. */
  if (attr->use_dupli) {
    BKE_object_dupli_find_rgba_attribute(ob, dupli_source, dupli_parent, attr->name, r_data);
  }
  else {
    BKE_object_dupli_find_rgba_attribute(ob, nullptr, nullptr, attr->name, r_data);
  }
}

void drw_uniform_attrs_pool_update(GHash *table,
                                   const GPUUniformAttrList *key,
                                   DRWResourceHandle *handle,
                                   const Object *ob,
                                   const Object *dupli_parent,
                                   const DupliObject *dupli_source)
{
  DRWUniformAttrBuf *buffer = drw_uniform_attrs_pool_ensure(table, key);

  if (buffer->last_handle != *handle) {
    buffer->last_handle = *handle;

    int chunk = DRW_handle_chunk_get(handle);
    int item = DRW_handle_id_get(handle);
    float(*values)[4] = static_cast<float(*)[4]>(
        DRW_sparse_uniform_buffer_ensure_item(&buffer->ubos, chunk, item));

    LISTBASE_FOREACH (const GPUUniformAttr *, attr, &buffer->key.list) {
      drw_uniform_attribute_lookup(attr, ob, dupli_parent, dupli_source, *values++);
    }
  }
}

GPUUniformBuf *drw_ensure_layer_attribute_buffer()
{
  DRWData *data = DST.vmempool;

  if (data->vlattrs_ubo_ready && data->vlattrs_ubo != nullptr) {
    return data->vlattrs_ubo;
  }

  /* Allocate the buffer data. */
  const int buf_size = DRW_RESOURCE_CHUNK_LEN;

  if (data->vlattrs_buf == nullptr) {
    data->vlattrs_buf = static_cast<LayerAttribute *>(
        MEM_calloc_arrayN(buf_size, sizeof(LayerAttribute), "View Layer Attr Data"));
  }

  /* Look up attributes.
   *
   * Mirrors code in draw_resource.cc and cycles/blender/shader.cpp.
   */
  LayerAttribute *buffer = data->vlattrs_buf;
  int count = 0;

  LISTBASE_FOREACH (GPULayerAttr *, attr, &data->vlattrs_name_list) {
    float value[4];

    if (BKE_view_layer_find_rgba_attribute(
            DST.draw_ctx.scene, DST.draw_ctx.view_layer, attr->name, value))
    {
      LayerAttribute *item = &buffer[count++];

      memcpy(item->data, value, sizeof(item->data));
      item->hash_code = attr->hash_code;

      /* Check if the buffer is full just in case. */
      if (count >= buf_size) {
        break;
      }
    }
  }

  buffer[0].buffer_length = count;

  /* Update or create the UBO object. */
  if (data->vlattrs_ubo != nullptr) {
    GPU_uniformbuf_update(data->vlattrs_ubo, buffer);
  }
  else {
    data->vlattrs_ubo = GPU_uniformbuf_create_ex(
        sizeof(*buffer) * buf_size, buffer, "View Layer Attributes");
  }

  data->vlattrs_ubo_ready = true;

  return data->vlattrs_ubo;
}

DRWSparseUniformBuf *DRW_uniform_attrs_pool_find_ubo(GHash *table, const GPUUniformAttrList *key)
{
  DRWUniformAttrBuf *buffer = static_cast<DRWUniformAttrBuf *>(BLI_ghash_lookup(table, key));
  return buffer ? &buffer->ubos : nullptr;
}

GHash *DRW_uniform_attrs_pool_new()
{
  return GPU_uniform_attr_list_hash_new("obattr_hash");
}

void DRW_uniform_attrs_pool_flush_all(GHash *table)
{
  GHASH_FOREACH_BEGIN (DRWUniformAttrBuf *, buffer, table) {
    DRW_sparse_uniform_buffer_flush(&buffer->ubos);
  }
  GHASH_FOREACH_END();
}

static void drw_uniform_attrs_pool_free_cb(void *ptr)
{
  DRWUniformAttrBuf *buffer = static_cast<DRWUniformAttrBuf *>(ptr);

  GPU_uniform_attr_list_free(&buffer->key);
  DRW_sparse_uniform_buffer_clear(&buffer->ubos, true);
  MEM_freeN(buffer);
}

void DRW_uniform_attrs_pool_clear_all(GHash *table)
{
  DRWUniformAttrBuf *remove_list = nullptr;

  GHASH_FOREACH_BEGIN (DRWUniformAttrBuf *, buffer, table) {
    buffer->last_handle = (DRWResourceHandle)-1;
    DRW_sparse_uniform_buffer_clear(&buffer->ubos, false);

    if (DRW_sparse_uniform_buffer_is_empty(&buffer->ubos)) {
      buffer->next_empty = remove_list;
      remove_list = buffer;
    }
  }
  GHASH_FOREACH_END();

  while (remove_list) {
    DRWUniformAttrBuf *buffer = remove_list;
    remove_list = buffer->next_empty;
    BLI_ghash_remove(table, &buffer->key, nullptr, drw_uniform_attrs_pool_free_cb);
  }
}

void DRW_uniform_attrs_pool_free(GHash *table)
{
  BLI_ghash_free(table, nullptr, drw_uniform_attrs_pool_free_cb);
}

/** \} */
