/*
 * Copyright 2016, Blender Foundation.
 *
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/intern/draw_instance_data.c
 *  \ingroup draw
 */

/**
 * DRW Instance Data Manager
 * This is a special memory manager that keeps memory blocks ready to send as vbo data in one continuous allocation.
 * This way we avoid feeding gawain each instance data one by one and unecessary memcpy.
 * Since we loose which memory block was used each DRWShadingGroup we need to redistribute them in the same order/size
 * to avoid to realloc each frame.
 * This is why DRWInstanceDatas are sorted in a list for each different data size.
 **/

#include "draw_instance_data.h"
#include "DRW_engine.h"
#include "DRW_render.h" /* For DRW_shgroup_get_instance_count() */

#include "MEM_guardedalloc.h"
#include "BLI_utildefines.h"

#define BUFFER_CHUNK_SIZE 32
#define BUFFER_VERTS_CHUNK 32

typedef struct DRWBatchingBuffer {
	struct DRWShadingGroup *shgroup;  /* Link back to the owning shGroup. Also tells if it's used */
	Gwn_VertFormat *format;           /* Identifier. */
	Gwn_VertBuf *vert;                /* Gwn_VertBuf contained in the Gwn_Batch. */
	Gwn_Batch *batch;                 /* Gwn_Batch containing the Gwn_VertBuf. */
} DRWBatchingBuffer;

typedef struct DRWInstancingBuffer {
	struct DRWShadingGroup *shgroup;  /* Link back to the owning shGroup. Also tells if it's used */
	Gwn_VertFormat *format;           /* Identifier. */
	Gwn_Batch *instance;              /* Identifier. */
	Gwn_VertBuf *vert;                /* Gwn_VertBuf contained in the Gwn_Batch. */
	Gwn_Batch *batch;                 /* Gwn_Batch containing the Gwn_VertBuf. */
} DRWInstancingBuffer;

typedef struct DRWInstanceChunk {
	size_t cursor;             /* Offset to the next instance data. */
	size_t alloc_size;         /* Number of DRWBatchingBuffer/Batches alloc'd in ibufs/btchs. */
	union {
		DRWBatchingBuffer *bbufs;
		DRWInstancingBuffer *ibufs;
	};
} DRWInstanceChunk;

struct DRWInstanceData {
	struct DRWInstanceData *next;
	bool used;                 /* If this data is used or not. */
	size_t chunk_size;         /* Current size of the whole chunk. */
	size_t data_size;          /* Size of one instance data. */
	size_t instance_group;     /* How many instance to allocate at a time. */
	size_t offset;             /* Offset to the next instance data. */
	float *memchunk;           /* Should be float no matter what. */
};

struct DRWInstanceDataList {
	struct DRWInstanceDataList *next, *prev;
	/* Linked lists for all possible data pool size */
	/* Not entirely sure if we should separate them in the first place.
	 * This is done to minimize the reattribution misses. */
	DRWInstanceData *idata_head[MAX_INSTANCE_DATA_SIZE];
	DRWInstanceData *idata_tail[MAX_INSTANCE_DATA_SIZE];

	DRWInstanceChunk instancing;
	DRWInstanceChunk batching;
};

static ListBase g_idatalists = {NULL, NULL};

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
 **/

static void instance_batch_free(Gwn_Batch *batch, void *UNUSED(user_data))
{
	/* Free all batches that have the same key before they are reused. */
	/* TODO: Make it thread safe! Batch freeing can happen from another thread. */
	/* XXX we need to iterate over all idatalists unless we make some smart
	 * data structure to store the locations to update. */
	for (DRWInstanceDataList *idatalist = g_idatalists.first; idatalist; idatalist = idatalist->next) {
		DRWInstancingBuffer *ibuf = idatalist->instancing.ibufs;
		for (int i = 0; i < idatalist->instancing.alloc_size; i++, ibuf++) {
			if (ibuf->instance == batch) {
				BLI_assert(ibuf->shgroup == NULL); /* Make sure it has no other users. */
				GWN_VERTBUF_DISCARD_SAFE(ibuf->vert);
				GWN_BATCH_DISCARD_SAFE(ibuf->batch);
				/* Tag as non alloced. */
				ibuf->format = NULL;
			}
		}
	}
}

void DRW_batching_buffer_request(
        DRWInstanceDataList *idatalist, Gwn_VertFormat *format, Gwn_PrimType type, struct DRWShadingGroup *shgroup,
        Gwn_Batch **r_batch, Gwn_VertBuf **r_vert)
{
	DRWInstanceChunk *chunk = &idatalist->batching;
	DRWBatchingBuffer *bbuf = idatalist->batching.bbufs;
	BLI_assert(format);
	/* Search for an unused batch. */
	for (int i = 0; i < idatalist->batching.alloc_size; i++, bbuf++) {
		if (bbuf->shgroup == NULL) {
			if (bbuf->format == format) {
				bbuf->shgroup = shgroup;
				*r_batch = bbuf->batch;
				*r_vert = bbuf->vert;
				return;
			}
		}
	}
	int new_id = 0; /* Find insertion point. */
	for (; new_id < chunk->alloc_size; ++new_id) {
		if (chunk->bbufs[new_id].format == NULL)
			break;
	}
	/* If there is no batch left. Allocate more. */
	if (new_id == chunk->alloc_size) {
		new_id = chunk->alloc_size;
		chunk->alloc_size += BUFFER_CHUNK_SIZE;
		chunk->bbufs = MEM_reallocN(chunk->bbufs, chunk->alloc_size * sizeof(DRWBatchingBuffer));
		memset(chunk->bbufs + new_id, 0, sizeof(DRWBatchingBuffer) * BUFFER_CHUNK_SIZE);
	}
	/* Create the batch. */
	bbuf = chunk->bbufs + new_id;
	bbuf->vert = *r_vert = GWN_vertbuf_create_with_format_ex(format, GWN_USAGE_DYNAMIC);
	bbuf->batch = *r_batch = GWN_batch_create_ex(type, bbuf->vert, NULL, 0);
	bbuf->format = format;
	bbuf->shgroup = shgroup;
	GWN_vertbuf_data_alloc(*r_vert, BUFFER_VERTS_CHUNK);
}

void DRW_instancing_buffer_request(
        DRWInstanceDataList *idatalist, Gwn_VertFormat *format, Gwn_Batch *instance, struct DRWShadingGroup *shgroup,
        Gwn_Batch **r_batch, Gwn_VertBuf **r_vert)
{
	DRWInstanceChunk *chunk = &idatalist->instancing;
	DRWInstancingBuffer *ibuf = idatalist->instancing.ibufs;
	BLI_assert(format);
	/* Search for an unused batch. */
	for (int i = 0; i < idatalist->instancing.alloc_size; i++, ibuf++) {
		if (ibuf->shgroup == NULL) {
			if (ibuf->format == format) {
				if (ibuf->instance == instance) {
					ibuf->shgroup = shgroup;
					*r_batch = ibuf->batch;
					*r_vert = ibuf->vert;
					return;
				}
			}
		}
	}
	int new_id = 0; /* Find insertion point. */
	for (; new_id < chunk->alloc_size; ++new_id) {
		if (chunk->ibufs[new_id].format == NULL)
			break;
	}
	/* If there is no batch left. Allocate more. */
	if (new_id == chunk->alloc_size) {
		new_id = chunk->alloc_size;
		chunk->alloc_size += BUFFER_CHUNK_SIZE;
		chunk->ibufs = MEM_reallocN(chunk->ibufs, chunk->alloc_size * sizeof(DRWInstancingBuffer));
		memset(chunk->ibufs + new_id, 0, sizeof(DRWInstancingBuffer) * BUFFER_CHUNK_SIZE);
	}
	/* Create the batch. */
	ibuf = chunk->ibufs + new_id;
	ibuf->vert = *r_vert = GWN_vertbuf_create_with_format_ex(format, GWN_USAGE_DYNAMIC);
	ibuf->batch = *r_batch = GWN_batch_duplicate(instance);
	ibuf->format = format;
	ibuf->shgroup = shgroup;
	ibuf->instance = instance;
	GWN_vertbuf_data_alloc(*r_vert, BUFFER_VERTS_CHUNK);
	GWN_batch_instbuf_set(ibuf->batch, ibuf->vert, false);
	/* Make sure to free this ibuf if the instance batch gets free. */
	GWN_batch_callback_free_set(instance, &instance_batch_free, NULL);
}

void DRW_instance_buffer_finish(DRWInstanceDataList *idatalist)
{
	size_t realloc_size = 1; /* Avoid 0 size realloc. */
	/* Resize down buffers in use and send data to GPU & free unused buffers. */
	DRWInstanceChunk *batching = &idatalist->batching;
	DRWBatchingBuffer *bbuf = batching->bbufs;
	for (int i = 0; i < batching->alloc_size; i++, bbuf++) {
		if (bbuf->shgroup != NULL) {
			realloc_size = i + 1;
			unsigned int vert_ct = DRW_shgroup_get_instance_count(bbuf->shgroup);
			vert_ct += (vert_ct == 0) ? 1 : 0; /* Do not realloc to 0 size buffer */
			if (vert_ct + BUFFER_VERTS_CHUNK <= bbuf->vert->vertex_ct) {
				unsigned int size = vert_ct + BUFFER_VERTS_CHUNK - 1;
				size = size - size % BUFFER_VERTS_CHUNK;
				GWN_vertbuf_data_resize(bbuf->vert, size);
			}
			GWN_vertbuf_use(bbuf->vert); /* Send data. */
			bbuf->shgroup = NULL; /* Set as non used for the next round. */
		}
		else {
			GWN_VERTBUF_DISCARD_SAFE(bbuf->vert);
			GWN_BATCH_DISCARD_SAFE(bbuf->batch);
			bbuf->format = NULL; /* Tag as non alloced. */
		}
	}
	/* Rounding up to nearest chunk size. */
	realloc_size += BUFFER_CHUNK_SIZE - 1;
	realloc_size -= realloc_size % BUFFER_CHUNK_SIZE;
	/* Resize down if necessary. */
	if (realloc_size < batching->alloc_size) {
		batching->alloc_size = realloc_size;
		batching->ibufs = MEM_reallocN(batching->ibufs, realloc_size * sizeof(DRWBatchingBuffer));
	}

	realloc_size = 1;
	/* Resize down buffers in use and send data to GPU & free unused buffers. */
	DRWInstanceChunk *instancing = &idatalist->instancing;
	DRWInstancingBuffer *ibuf = instancing->ibufs;
	for (int i = 0; i < instancing->alloc_size; i++, ibuf++) {
		if (ibuf->shgroup != NULL) {
			realloc_size = i + 1;
			unsigned int vert_ct = DRW_shgroup_get_instance_count(ibuf->shgroup);
			vert_ct += (vert_ct == 0) ? 1 : 0; /* Do not realloc to 0 size buffer */
			if (vert_ct + BUFFER_VERTS_CHUNK <= ibuf->vert->vertex_ct) {
				unsigned int size = vert_ct + BUFFER_VERTS_CHUNK - 1;
				size = size - size % BUFFER_VERTS_CHUNK;
				GWN_vertbuf_data_resize(ibuf->vert, size);
			}
			GWN_vertbuf_use(ibuf->vert); /* Send data. */
			ibuf->shgroup = NULL; /* Set as non used for the next round. */
		}
		else {
			GWN_VERTBUF_DISCARD_SAFE(ibuf->vert);
			GWN_BATCH_DISCARD_SAFE(ibuf->batch);
			ibuf->format = NULL; /* Tag as non alloced. */
		}
	}
	/* Rounding up to nearest chunk size. */
	realloc_size += BUFFER_CHUNK_SIZE - 1;
	realloc_size -= realloc_size % BUFFER_CHUNK_SIZE;
	/* Resize down if necessary. */
	if (realloc_size < instancing->alloc_size) {
		instancing->alloc_size = realloc_size;
		instancing->ibufs = MEM_reallocN(instancing->ibufs, realloc_size * sizeof(DRWInstancingBuffer));
	}
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Instance Data (DRWInstanceData)
 * \{ */

static DRWInstanceData *drw_instance_data_create(
        DRWInstanceDataList *idatalist, unsigned int attrib_size, unsigned int instance_group)
{
	DRWInstanceData *idata = MEM_callocN(sizeof(DRWInstanceData), "DRWInstanceData");
	idata->next = NULL;
	idata->used = true;
	idata->data_size = attrib_size;
	idata->instance_group = instance_group;
	idata->chunk_size = idata->data_size * instance_group;
	idata->offset = 0;
	idata->memchunk = MEM_mallocN(idata->chunk_size * sizeof(float), "DRWInstanceData memchunk");

	BLI_assert(attrib_size > 0);

	/* Push to linked list. */
	if (idatalist->idata_head[attrib_size-1] == NULL) {
		idatalist->idata_head[attrib_size-1] = idata;
	}
	else {
		idatalist->idata_tail[attrib_size-1]->next = idata;
	}
	idatalist->idata_tail[attrib_size-1] = idata;

	return idata;
}

static void DRW_instance_data_free(DRWInstanceData *idata)
{
	MEM_freeN(idata->memchunk);
}

/**
 * Return a pointer to the next instance data space.
 * DO NOT SAVE/REUSE THIS POINTER after the next call
 * to this function since the chunk may have been
 * reallocated.
 **/
void *DRW_instance_data_next(DRWInstanceData *idata)
{
	idata->offset += idata->data_size;

	/* Check if chunk is large enough. realloc otherwise. */
	if (idata->offset > idata->chunk_size) {
		idata->chunk_size += idata->data_size * idata->instance_group;
		idata->memchunk = MEM_reallocN(idata->memchunk, idata->chunk_size * sizeof(float));
	}

	return idata->memchunk + (idata->offset - idata->data_size);
}

void *DRW_instance_data_get(DRWInstanceData *idata)
{
	return (void *)idata->memchunk;
}

DRWInstanceData *DRW_instance_data_request(
        DRWInstanceDataList *idatalist, unsigned int attrib_size, unsigned int instance_group)
{
	BLI_assert(attrib_size > 0 && attrib_size <= MAX_INSTANCE_DATA_SIZE);

	DRWInstanceData *idata = idatalist->idata_head[attrib_size - 1];

	/* Search for an unused data chunk. */
	for (; idata; idata = idata->next) {
		if (idata->used == false) {
			idata->used = true;
			return idata;
		}
	}

	return drw_instance_data_create(idatalist, attrib_size, instance_group);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Instance Data List (DRWInstanceDataList)
 * \{ */

DRWInstanceDataList *DRW_instance_data_list_create(void)
{
	DRWInstanceDataList *idatalist = MEM_callocN(sizeof(DRWInstanceDataList), "DRWInstanceDataList");
	idatalist->batching.bbufs = MEM_callocN(sizeof(DRWBatchingBuffer) * BUFFER_CHUNK_SIZE, "DRWBatchingBuffers");
	idatalist->batching.alloc_size = BUFFER_CHUNK_SIZE;
	idatalist->instancing.ibufs = MEM_callocN(sizeof(DRWInstancingBuffer) * BUFFER_CHUNK_SIZE, "DRWInstancingBuffers");
	idatalist->instancing.alloc_size = BUFFER_CHUNK_SIZE;

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

	DRWBatchingBuffer *bbuf = idatalist->batching.bbufs;
	for (int i = 0; i < idatalist->batching.alloc_size; i++, bbuf++) {
		GWN_VERTBUF_DISCARD_SAFE(bbuf->vert);
		GWN_BATCH_DISCARD_SAFE(bbuf->batch);
	}
	MEM_freeN(idatalist->batching.bbufs);

	DRWInstancingBuffer *ibuf = idatalist->instancing.ibufs;
	for (int i = 0; i < idatalist->instancing.alloc_size; i++, ibuf++) {
		GWN_VERTBUF_DISCARD_SAFE(ibuf->vert);
		GWN_BATCH_DISCARD_SAFE(ibuf->batch);
	}
	MEM_freeN(idatalist->instancing.ibufs);

	BLI_remlink(&g_idatalists, idatalist);
}

void DRW_instance_data_list_reset(DRWInstanceDataList *idatalist)
{
	DRWInstanceData *idata;

	for (int i = 0; i < MAX_INSTANCE_DATA_SIZE; ++i) {
		for (idata = idatalist->idata_head[i]; idata; idata = idata->next) {
			idata->used = false;
			idata->offset = 0;
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
					/* idatalist->idata_tail[i] is garanteed not to be null in this case. */
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
			/* Rounding up to nearest chunk size to compare. */
			size_t fac = idata->data_size * idata->instance_group;
			size_t tmp = idata->offset + fac - 1;
			size_t rounded_offset = tmp - tmp % fac;
			if (rounded_offset < idata->chunk_size) {
				idata->chunk_size = rounded_offset;
				idata->memchunk = MEM_reallocN(idata->memchunk, idata->chunk_size * sizeof(float));
			}
		}
	}
}

/** \} */
