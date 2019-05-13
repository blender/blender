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

#ifndef __DRAW_INSTANCE_DATA_H__
#define __DRAW_INSTANCE_DATA_H__

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#include "GPU_batch.h"

#define MAX_INSTANCE_DATA_SIZE 64 /* Can be adjusted for more */

#define DRW_BUFFER_VERTS_CHUNK 128

typedef struct DRWInstanceData DRWInstanceData;
typedef struct DRWInstanceDataList DRWInstanceDataList;

struct DRWShadingGroup;

void *DRW_instance_data_next(DRWInstanceData *idata);
DRWInstanceData *DRW_instance_data_request(DRWInstanceDataList *idatalist, uint attr_size);

GPUVertBuf *DRW_temp_buffer_request(DRWInstanceDataList *idatalist,
                                    GPUVertFormat *format,
                                    uint *vert_len);
GPUBatch *DRW_temp_batch_instance_request(DRWInstanceDataList *idatalist,
                                          GPUVertBuf *buf,
                                          GPUBatch *geom);
GPUBatch *DRW_temp_batch_request(DRWInstanceDataList *idatalist,
                                 GPUVertBuf *buf,
                                 GPUPrimType type);

/* Upload all instance data to the GPU as soon as possible. */
void DRW_instance_buffer_finish(DRWInstanceDataList *idatalist);

void DRW_instance_data_list_reset(DRWInstanceDataList *idatalist);
void DRW_instance_data_list_free_unused(DRWInstanceDataList *idatalist);
void DRW_instance_data_list_resize(DRWInstanceDataList *idatalist);

#endif /* __DRAW_INSTANCE_DATA_H__ */
