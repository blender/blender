/* SPDX-FileCopyrightText: 2016 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#include "GPU_batch.h"

#define MAX_INSTANCE_DATA_SIZE 64 /* Can be adjusted for more */

#define DRW_BUFFER_VERTS_CHUNK 128

#ifdef __cplusplus
extern "C" {
#endif

struct GHash;
struct GPUUniformAttrList;

typedef struct DRWInstanceData DRWInstanceData;
typedef struct DRWInstanceDataList DRWInstanceDataList;
typedef struct DRWSparseUniformBuf DRWSparseUniformBuf;

/**
 * Return a pointer to the next instance data space.
 */
void *DRW_instance_data_next(DRWInstanceData *idata);
DRWInstanceData *DRW_instance_data_request(DRWInstanceDataList *idatalist, uint attr_size);

/**
 * This manager allows to distribute existing batches for instancing
 * attributes. This reduce the number of batches creation.
 * Querying a batch is done with a vertex format. This format should
 * be static so that its pointer never changes (because we are using
 * this pointer as identifier [we don't want to check the full format
 * that would be too slow]).
 */
GPUVertBuf *DRW_temp_buffer_request(DRWInstanceDataList *idatalist,
                                    GPUVertFormat *format,
                                    int *vert_len);
/**
 * \note Does not return a valid drawable batch until DRW_instance_buffer_finish has run.
 * Initialization is delayed because instancer or geom could still not be initialized.
 */
GPUBatch *DRW_temp_batch_instance_request(DRWInstanceDataList *idatalist,
                                          GPUVertBuf *buf,
                                          GPUBatch *instancer,
                                          GPUBatch *geom);
/**
 * \note Use only with buf allocated via DRW_temp_buffer_request.
 */
GPUBatch *DRW_temp_batch_request(DRWInstanceDataList *idatalist,
                                 GPUVertBuf *buf,
                                 GPUPrimType type);

/**
 * Upload all instance data to the GPU as soon as possible.
 */
void DRW_instance_buffer_finish(DRWInstanceDataList *idatalist);

void DRW_instance_data_list_reset(DRWInstanceDataList *idatalist);
void DRW_instance_data_list_free_unused(DRWInstanceDataList *idatalist);
void DRW_instance_data_list_resize(DRWInstanceDataList *idatalist);

/* Sparse chunked UBO manager. */

/**
 * Allocate a chunked UBO with the specified item and chunk size.
 */
DRWSparseUniformBuf *DRW_sparse_uniform_buffer_new(unsigned int item_size,
                                                   unsigned int chunk_size);
/**
 * Flush data from ordinary memory to UBOs.
 */
void DRW_sparse_uniform_buffer_flush(DRWSparseUniformBuf *buffer);
/**
 * Clean all buffers and free unused ones.
 */
void DRW_sparse_uniform_buffer_clear(DRWSparseUniformBuf *buffer, bool free_all);
/**
 * Frees the buffer.
 */
void DRW_sparse_uniform_buffer_free(DRWSparseUniformBuf *buffer);
/**
 * Checks if the buffer contains any allocated chunks.
 */
bool DRW_sparse_uniform_buffer_is_empty(DRWSparseUniformBuf *buffer);
/**
 * Bind the UBO for the given chunk, if present. A NULL buffer pointer is handled as empty.
 */
void DRW_sparse_uniform_buffer_bind(DRWSparseUniformBuf *buffer, int chunk, int location);
/**
 * Unbind the UBO for the given chunk, if present. A NULL buffer pointer is handled as empty.
 */
void DRW_sparse_uniform_buffer_unbind(DRWSparseUniformBuf *buffer, int chunk);
/**
 * Returns a pointer to the given item of the given chunk, allocating memory if necessary.
 */
void *DRW_sparse_uniform_buffer_ensure_item(DRWSparseUniformBuf *buffer, int chunk, int item);

/* Uniform attribute UBO management. */

struct GHash *DRW_uniform_attrs_pool_new(void);
void DRW_uniform_attrs_pool_flush_all(struct GHash *table);
void DRW_uniform_attrs_pool_clear_all(struct GHash *table);
struct DRWSparseUniformBuf *DRW_uniform_attrs_pool_find_ubo(struct GHash *table,
                                                            const struct GPUUniformAttrList *key);

#ifdef __cplusplus
}
#endif
