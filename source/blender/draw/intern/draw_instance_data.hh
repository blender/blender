/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#include "GPU_batch.hh"

#define MAX_INSTANCE_DATA_SIZE 64 /* Can be adjusted for more */

#define DRW_BUFFER_VERTS_CHUNK 128

struct GHash;
struct GPUUniformAttrList;
struct DRWInstanceData;
struct DRWInstanceDataList;
struct DRWSparseUniformBuf;

/**
 * Return a pointer to the next instance data space.
 */
void *DRW_instance_data_next(DRWInstanceData *idata);
DRWInstanceData *DRW_instance_data_request(DRWInstanceDataList *idatalist, uint attr_size);

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
 * Bind the UBO for the given chunk, if present. A nullptr buffer pointer is handled as empty.
 */
void DRW_sparse_uniform_buffer_bind(DRWSparseUniformBuf *buffer, int chunk, int location);
/**
 * Unbind the UBO for the given chunk, if present. A nullptr buffer pointer is handled as empty.
 */
void DRW_sparse_uniform_buffer_unbind(DRWSparseUniformBuf *buffer, int chunk);
/**
 * Returns a pointer to the given item of the given chunk, allocating memory if necessary.
 */
void *DRW_sparse_uniform_buffer_ensure_item(DRWSparseUniformBuf *buffer, int chunk, int item);

/* Uniform attribute UBO management. */

GHash *DRW_uniform_attrs_pool_new();
void DRW_uniform_attrs_pool_flush_all(GHash *table);
void DRW_uniform_attrs_pool_clear_all(GHash *table);
DRWSparseUniformBuf *DRW_uniform_attrs_pool_find_ubo(GHash *table, const GPUUniformAttrList *key);
