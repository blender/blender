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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

/**
 * DRW Instance Data Manager
 * This is a special memory manager that keeps memory blocks ready to send as vbo data in one
 * continuous allocation. This way we avoid feeding #GPUBatch each instance data one by one and
 * unnecessary memcpy. Since we loose which memory block was used each #DRWShadingGroup we need to
 * redistribute them in the same order/size to avoid to realloc each frame. This is why
 * #DRWInstanceDatas are sorted in a list for each different data size.
 */

#include "draw_instance_data.h"
#include "DRW_engine.h"
#include "DRW_render.h" /* For DRW_shgroup_get_instance_count() */

#include "BLI_memblock.h"
#include "BLI_mempool.h"
#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

#include "intern/gpu_primitive_private.h"

struct DRWInstanceData {
  struct DRWInstanceData *next;
  bool used;        /* If this data is used or not. */
  size_t data_size; /* Size of one instance data. */
  BLI_mempool *mempool;
};

struct DRWInstanceDataList {
  struct DRWInstanceDataList *next, *prev;
  /* Linked lists for all possible data pool size */
  DRWInstanceData *idata_head[MAX_INSTANCE_DATA_SIZE];
  DRWInstanceData *idata_tail[MAX_INSTANCE_DATA_SIZE];

  BLI_memblock *pool_instancing;
  BLI_memblock *pool_batching;
  BLI_memblock *pool_buffers;
};

typedef struct DRWTempBufferHandle {
  GPUVertBuf *buf;
  /** Format pointer for reuse. */
  GPUVertFormat *format;
  /** Touched vertex length for resize. */
  int *vert_len;
} DRWTempBufferHandle;

typedef struct DRWTempInstancingHandle {
  /** Copy of geom but with the per-instance attributes. */
  GPUBatch *batch;
  /** Batch containing instancing attributes. */
  GPUBatch *instancer;
  /** Callbuffer to be used instead of instancer . */
  GPUVertBuf *buf;
  /** Original non-instanced batch pointer. */
  GPUBatch *geom;
} DRWTempInstancingHandle;

static ListBase g_idatalists = {NULL, NULL};

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

/**
 * This manager allows to distribute existing batches for instancing
 * attributes. This reduce the number of batches creation.
 * Querying a batch is done with a vertex format. This format should
 * be static so that it's pointer never changes (because we are using
 * this pointer as identifier [we don't want to check the full format
 * that would be too slow]).
 */
GPUVertBuf *DRW_temp_buffer_request(DRWInstanceDataList *idatalist,
                                    GPUVertFormat *format,
                                    int *vert_len)
{
  BLI_assert(format != NULL);
  BLI_assert(vert_len != NULL);

  DRWTempBufferHandle *handle = BLI_memblock_alloc(idatalist->pool_buffers);

  if (handle->format != format) {
    handle->format = format;
    GPU_VERTBUF_DISCARD_SAFE(handle->buf);

    GPUVertBuf *vert = GPU_vertbuf_create(GPU_USAGE_DYNAMIC);
    GPU_vertbuf_init_with_format_ex(vert, format, GPU_USAGE_DYNAMIC);
    GPU_vertbuf_data_alloc(vert, DRW_BUFFER_VERTS_CHUNK);

    handle->buf = vert;
  }
  handle->vert_len = vert_len;
  return handle->buf;
}

/* NOTE: Does not return a valid drawable batch until DRW_instance_buffer_finish has run.
 * Initialization is delayed because instancer or geom could still not be initialized. */
GPUBatch *DRW_temp_batch_instance_request(DRWInstanceDataList *idatalist,
                                          GPUVertBuf *buf,
                                          GPUBatch *instancer,
                                          GPUBatch *geom)
{
  /* Do not call this with a batch that is already an instancing batch. */
  BLI_assert(geom->inst[0] == NULL);
  /* Only call with one of them. */
  BLI_assert((instancer != NULL) != (buf != NULL));

  DRWTempInstancingHandle *handle = BLI_memblock_alloc(idatalist->pool_instancing);
  if (handle->batch == NULL) {
    handle->batch = GPU_batch_calloc();
  }

  GPUBatch *batch = handle->batch;
  bool instancer_compat = buf ? ((batch->inst[0] == buf) && (buf->vbo_id != 0)) :
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

/* NOTE: Use only with buf allocated via DRW_temp_buffer_request. */
GPUBatch *DRW_temp_batch_request(DRWInstanceDataList *idatalist,
                                 GPUVertBuf *buf,
                                 GPUPrimType prim_type)
{
  GPUBatch **batch_ptr = BLI_memblock_alloc(idatalist->pool_batching);
  if (*batch_ptr == NULL) {
    *batch_ptr = GPU_batch_calloc();
  }

  GPUBatch *batch = *batch_ptr;
  bool is_compatible = (batch->verts[0] == buf) && (buf->vbo_id != 0) &&
                       (batch->prim_type == prim_type);
  if (!is_compatible) {
    GPU_batch_clear(batch);
    GPU_batch_init(batch, prim_type, buf, NULL);
  }
  return batch;
}

static void temp_buffer_handle_free(DRWTempBufferHandle *handle)
{
  handle->format = NULL;
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
  while ((handle = BLI_memblock_iterstep(&iter))) {
    if (handle->vert_len != NULL) {
      uint vert_len = *(handle->vert_len);
      uint target_buf_size = ((vert_len / DRW_BUFFER_VERTS_CHUNK) + 1) * DRW_BUFFER_VERTS_CHUNK;
      if (target_buf_size < handle->buf->vertex_alloc) {
        GPU_vertbuf_data_resize(handle->buf, target_buf_size);
      }
      GPU_vertbuf_data_len_set(handle->buf, vert_len);
      GPU_vertbuf_use(handle->buf); /* Send data. */
    }
  }
  /* Finish pending instancing batches. */
  DRWTempInstancingHandle *handle_inst;
  BLI_memblock_iternew(idatalist->pool_instancing, &iter);
  while ((handle_inst = BLI_memblock_iterstep(&iter))) {
    GPUBatch *batch = handle_inst->batch;
    if (batch && batch->flag == GPU_BATCH_BUILDING) {
      GPUVertBuf *inst_buf = handle_inst->buf;
      GPUBatch *inst_batch = handle_inst->instancer;
      GPUBatch *geom = handle_inst->geom;
      GPU_batch_copy(batch, geom);
      if (inst_batch != NULL) {
        for (int i = 0; i < GPU_BATCH_INST_VBO_MAX_LEN && inst_batch->verts[i]; i++) {
          GPU_batch_instbuf_add_ex(batch, inst_batch->verts[i], false);
        }
      }
      else {
        GPU_batch_instbuf_add_ex(batch, inst_buf, false);
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
  DRWInstanceData *idata = MEM_callocN(sizeof(DRWInstanceData), "DRWInstanceData");
  idata->next = NULL;
  idata->used = true;
  idata->data_size = attr_size;
  idata->mempool = BLI_mempool_create(sizeof(float) * idata->data_size, 0, 16, 0);

  BLI_assert(attr_size > 0);

  /* Push to linked list. */
  if (idatalist->idata_head[attr_size - 1] == NULL) {
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

/**
 * Return a pointer to the next instance data space.
 */
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

DRWInstanceDataList *DRW_instance_data_list_create(void)
{
  DRWInstanceDataList *idatalist = MEM_callocN(sizeof(DRWInstanceDataList), "DRWInstanceDataList");

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
    idatalist->idata_head[i] = NULL;
    idatalist->idata_tail[i] = NULL;
  }

  BLI_memblock_destroy(idatalist->pool_buffers, (MemblockValFreeFP)temp_buffer_handle_free);
  BLI_memblock_destroy(idatalist->pool_instancing, (MemblockValFreeFP)temp_instancing_handle_free);
  BLI_memblock_destroy(idatalist->pool_batching, (MemblockValFreeFP)temp_batch_free);

  BLI_remlink(&g_idatalists, idatalist);
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
    idatalist->idata_tail[i] = NULL;
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
        if (idatalist->idata_tail[i] != NULL) {
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
