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
 * continuous allocation. This way we avoid feeding gawain each instance data one by one and
 * unnecessary memcpy. Since we loose which memory block was used each #DRWShadingGroup we need to
 * redistribute them in the same order/size to avoid to realloc each frame. This is why
 * #DRWInstanceDatas are sorted in a list for each different data size.
 */

#include "draw_instance_data.h"
#include "DRW_engine.h"
#include "DRW_render.h" /* For DRW_shgroup_get_instance_count() */

#include "MEM_guardedalloc.h"
#include "BLI_utildefines.h"
#include "BLI_mempool.h"
#include "BLI_memblock.h"

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
  /** Must be first for casting. */
  GPUVertBuf buf;
  /** Format pointer for reuse. */
  GPUVertFormat *format;
  /** Touched vertex length for resize. */
  uint *vert_len;
} DRWTempBufferHandle;

static ListBase g_idatalists = {NULL, NULL};

/* -------------------------------------------------------------------- */
/** \name Instance Buffer Management
 * \{ */

static void instance_batch_free(GPUBatch *geom, void *UNUSED(user_data))
{
  if (geom->verts[0] == NULL) {
    /** XXX This is a false positive case.
     * The batch has been requested but not init yet
     * and there is a chance that it might become init.
     */
    return;
  }

  /* Free all batches that use the same vbos before they are reused. */
  /* TODO: Make it thread safe! Batch freeing can happen from another thread. */
  /* FIXME: This is not really correct. The correct way would be to check based on
   * the vertex buffers. We assume the batch containing the VBO is being when it should. */
  /* PERF: This is doing a linear search. This can be very costly. */
  LISTBASE_FOREACH (DRWInstanceDataList *, data_list, &g_idatalists) {
    BLI_memblock *memblock = data_list->pool_instancing;
    BLI_memblock_iter iter;
    BLI_memblock_iternew(memblock, &iter);
    GPUBatch *batch;
    while ((batch = (GPUBatch *)BLI_memblock_iterstep(&iter))) {
      /* Only check verts[0] that's enough. */
      if (batch->verts[0] == geom->verts[0]) {
        GPU_batch_clear(batch);
      }
    }
  }
}

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
                                    uint *vert_len)
{
  BLI_assert(format != NULL);
  BLI_assert(vert_len != NULL);

  DRWTempBufferHandle *handle = BLI_memblock_alloc(idatalist->pool_buffers);
  GPUVertBuf *vert = &handle->buf;
  handle->vert_len = vert_len;

  if (handle->format != format) {
    handle->format = format;
    /* TODO/PERF: Save the allocated data from freeing to avoid reallocation. */
    GPU_vertbuf_clear(vert);
    GPU_vertbuf_init_with_format_ex(vert, format, GPU_USAGE_DYNAMIC);
    GPU_vertbuf_data_alloc(vert, DRW_BUFFER_VERTS_CHUNK);
  }
  return vert;
}

/* NOTE: Does not return a valid drawable batch until DRW_instance_buffer_finish has run. */
GPUBatch *DRW_temp_batch_instance_request(DRWInstanceDataList *idatalist,
                                          GPUVertBuf *buf,
                                          GPUBatch *geom)
{
  /* Do not call this with a batch that is already an instancing batch. */
  BLI_assert(geom->inst == NULL);

  GPUBatch *batch = BLI_memblock_alloc(idatalist->pool_instancing);
  bool is_compatible = (batch->gl_prim_type == geom->gl_prim_type) && (batch->inst == buf) &&
                       (buf->vbo_id != 0) && (batch->phase == GPU_BATCH_READY_TO_DRAW);
  for (int i = 0; i < GPU_BATCH_VBO_MAX_LEN && is_compatible; i++) {
    if (batch->verts[i] != geom->verts[i]) {
      is_compatible = false;
    }
  }

  if (!is_compatible) {
    GPU_batch_clear(batch);
    /* Save args and init later */
    batch->inst = buf;
    batch->phase = GPU_BATCH_READY_TO_BUILD;
    batch->verts[0] = (void *)geom; /* HACK to save the pointer without other alloc. */

    /* Make sure to free this batch if the instance geom gets free. */
    GPU_batch_callback_free_set(geom, &instance_batch_free, NULL);
  }
  return batch;
}

/* NOTE: Use only with buf allocated via DRW_temp_buffer_request. */
GPUBatch *DRW_temp_batch_request(DRWInstanceDataList *idatalist,
                                 GPUVertBuf *buf,
                                 GPUPrimType prim_type)
{
  GPUBatch *batch = BLI_memblock_alloc(idatalist->pool_batching);
  bool is_compatible = (batch->verts[0] == buf) && (buf->vbo_id != 0) &&
                       (batch->gl_prim_type == convert_prim_type_to_gl(prim_type));
  if (!is_compatible) {
    GPU_batch_clear(batch);
    GPU_batch_init(batch, prim_type, buf, NULL);
  }
  return batch;
}

static void temp_buffer_handle_free(DRWTempBufferHandle *handle)
{
  handle->format = NULL;
  GPU_vertbuf_clear(&handle->buf);
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
      if (target_buf_size < handle->buf.vertex_alloc) {
        GPU_vertbuf_data_resize(&handle->buf, target_buf_size);
      }
      GPU_vertbuf_data_len_set(&handle->buf, vert_len);
      GPU_vertbuf_use(&handle->buf); /* Send data. */
    }
  }
  /* Finish pending instancing batches. */
  GPUBatch *batch;
  BLI_memblock_iternew(idatalist->pool_instancing, &iter);
  while ((batch = BLI_memblock_iterstep(&iter))) {
    if (batch->phase == GPU_BATCH_READY_TO_BUILD) {
      GPUVertBuf *inst = batch->inst;
      GPUBatch *geom = (void *)batch->verts[0]; /* HACK see DRW_temp_batch_instance_request. */
      GPU_batch_copy(batch, geom);
      GPU_batch_instbuf_set(batch, inst, false);
    }
  }
  /* Resize pools and free unused. */
  BLI_memblock_clear(idatalist->pool_buffers, (MemblockValFreeFP)temp_buffer_handle_free);
  BLI_memblock_clear(idatalist->pool_instancing, (MemblockValFreeFP)GPU_batch_clear);
  BLI_memblock_clear(idatalist->pool_batching, (MemblockValFreeFP)GPU_batch_clear);
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

  idatalist->pool_batching = BLI_memblock_create(sizeof(GPUBatch));
  idatalist->pool_instancing = BLI_memblock_create(sizeof(GPUBatch));
  idatalist->pool_buffers = BLI_memblock_create(sizeof(DRWTempBufferHandle));

  BLI_addtail(&g_idatalists, idatalist);

  return idatalist;
}

void DRW_instance_data_list_free(DRWInstanceDataList *idatalist)
{
  DRWInstanceData *idata, *next_idata;

  for (int i = 0; i < MAX_INSTANCE_DATA_SIZE; ++i) {
    for (idata = idatalist->idata_head[i]; idata; idata = next_idata) {
      next_idata = idata->next;
      DRW_instance_data_free(idata);
      MEM_freeN(idata);
    }
    idatalist->idata_head[i] = NULL;
    idatalist->idata_tail[i] = NULL;
  }

  BLI_memblock_destroy(idatalist->pool_buffers, (MemblockValFreeFP)temp_buffer_handle_free);
  BLI_memblock_destroy(idatalist->pool_instancing, (MemblockValFreeFP)GPU_batch_clear);
  BLI_memblock_destroy(idatalist->pool_batching, (MemblockValFreeFP)GPU_batch_clear);

  BLI_remlink(&g_idatalists, idatalist);
}

void DRW_instance_data_list_reset(DRWInstanceDataList *idatalist)
{
  DRWInstanceData *idata;

  for (int i = 0; i < MAX_INSTANCE_DATA_SIZE; ++i) {
    for (idata = idatalist->idata_head[i]; idata; idata = idata->next) {
      idata->used = false;
    }
  }
}

void DRW_instance_data_list_free_unused(DRWInstanceDataList *idatalist)
{
  DRWInstanceData *idata, *next_idata;

  /* Remove unused data blocks and sanitize each list. */
  for (int i = 0; i < MAX_INSTANCE_DATA_SIZE; ++i) {
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

  for (int i = 0; i < MAX_INSTANCE_DATA_SIZE; ++i) {
    for (idata = idatalist->idata_head[i]; idata; idata = idata->next) {
      BLI_mempool_clear_ex(idata->mempool, BLI_mempool_len(idata->mempool));
    }
  }
}

/** \} */
