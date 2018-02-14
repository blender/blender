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

typedef struct DRWInstanceBuffer {
	struct DRWShadingGroup *shgroup;  /* Link back to the owning shGroup. Also tells if it's used */
	Gwn_VertFormat *format;           /* Identifier. */
	Gwn_VertBuf *vert;                /* Gwn_VertBuf contained in the Gwn_Batch. */
	Gwn_Batch *batch;                 /* Gwn_Batch containing the Gwn_VertBuf. */
} DRWInstanceBuffer;

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
	/* Linked lists for all possible data pool size */
	/* Not entirely sure if we should separate them in the first place.
	 * This is done to minimize the reattribution misses. */
	DRWInstanceData *idata_head[MAX_INSTANCE_DATA_SIZE];
	DRWInstanceData *idata_tail[MAX_INSTANCE_DATA_SIZE];

	struct {
		size_t cursor;             /* Offset to the next instance data. */
		size_t alloc_size;         /* Number of DRWInstanceBuffer alloc'd in ibufs. */
		DRWInstanceBuffer *ibufs;
	} ibuffers;
};

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

void DRW_instance_buffer_request(
        DRWInstanceDataList *idatalist, Gwn_VertFormat *format, struct DRWShadingGroup *shgroup,
        Gwn_Batch **r_batch, Gwn_VertBuf **r_vert, Gwn_PrimType type)
{
	BLI_assert(format);

	DRWInstanceBuffer *ibuf = idatalist->ibuffers.ibufs;
	int first_non_alloced = -1;

	/* Search for an unused batch. */
	for (int i = 0; i < idatalist->ibuffers.alloc_size; i++, ibuf++) {
		if (ibuf->shgroup == NULL) {
			if (ibuf->format == format) {
				ibuf->shgroup = shgroup;
				*r_batch = ibuf->batch;
				*r_vert = ibuf->vert;
				return;
			}
			else if (ibuf->format == NULL && first_non_alloced == -1) {
				first_non_alloced = i;
			}
		}
	}

	if (first_non_alloced == -1) {
		/* There is no batch left. Allocate more. */
		first_non_alloced = idatalist->ibuffers.alloc_size;
		idatalist->ibuffers.alloc_size += BUFFER_CHUNK_SIZE;
		idatalist->ibuffers.ibufs = MEM_reallocN(idatalist->ibuffers.ibufs,
		                                         idatalist->ibuffers.alloc_size * sizeof(DRWInstanceBuffer));
		/* Clear new part of the memory. */
		memset(idatalist->ibuffers.ibufs + first_non_alloced, 0, sizeof(DRWInstanceBuffer) * BUFFER_CHUNK_SIZE);
	}

	/* Create the batch. */
	ibuf = idatalist->ibuffers.ibufs + first_non_alloced;
	ibuf->vert = *r_vert = GWN_vertbuf_create_dynamic_with_format(format);
	ibuf->batch = *r_batch = GWN_batch_create_ex(type, ibuf->vert, NULL, GWN_BATCH_OWNS_VBO);
	ibuf->format = format;
	ibuf->shgroup = shgroup;

	GWN_vertbuf_data_alloc(*r_vert, BUFFER_VERTS_CHUNK);
}

void DRW_instance_buffer_finish(DRWInstanceDataList *idatalist)
{
	DRWInstanceBuffer *ibuf = idatalist->ibuffers.ibufs;
	size_t minimum_alloc_size = 1; /* Avoid 0 size realloc. */

	/* Resize down buffers in use and send data to GPU & free unused buffers. */
	for (int i = 0; i < idatalist->ibuffers.alloc_size; i++, ibuf++) {
		if (ibuf->shgroup != NULL) {
			minimum_alloc_size = i + 1;
			unsigned int vert_ct = DRW_shgroup_get_instance_count(ibuf->shgroup);
			/* Do not realloc to 0 size buffer */
			vert_ct += (vert_ct == 0) ? 1 : 0;
			/* Resize buffer to reclame space. */
			if (vert_ct + BUFFER_VERTS_CHUNK <= ibuf->vert->vertex_ct) {
				unsigned int size = vert_ct + BUFFER_VERTS_CHUNK - 1;
				size = size - size % BUFFER_VERTS_CHUNK;
				GWN_vertbuf_data_resize(ibuf->vert, size);
			}
			/* Send data. */
			GWN_vertbuf_use(ibuf->vert);
			/* Set as non used for the next round. */
			ibuf->shgroup = NULL;
		}
		else {
			GWN_BATCH_DISCARD_SAFE(ibuf->batch);
			/* Tag as non alloced. */
			ibuf->format = NULL;
		}
	}

	/* Resize down the handle buffer (ibuffers). */
	/* Rounding up to nearest chunk size. */
	minimum_alloc_size += BUFFER_CHUNK_SIZE - 1;
	minimum_alloc_size -= minimum_alloc_size % BUFFER_CHUNK_SIZE;
	/* Resize down if necessary. */
	if (minimum_alloc_size < idatalist->ibuffers.alloc_size) {
		idatalist->ibuffers.alloc_size = minimum_alloc_size;
		idatalist->ibuffers.ibufs = MEM_reallocN(idatalist->ibuffers.ibufs,
		                                         minimum_alloc_size * sizeof(DRWInstanceBuffer));
	}
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Instance Data (DRWInstanceData)
 * \{ */

static DRWInstanceData *drw_instance_data_create(
        DRWInstanceDataList *idatalist, unsigned int attrib_size, unsigned int instance_group)
{
	DRWInstanceData *idata = MEM_mallocN(sizeof(DRWInstanceData), "DRWInstanceData");
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
	idatalist->ibuffers.ibufs = MEM_callocN(sizeof(DRWInstanceBuffer) * BUFFER_CHUNK_SIZE, "DRWInstanceBuffers");
	idatalist->ibuffers.alloc_size = BUFFER_CHUNK_SIZE;

	return idatalist;
}

void DRW_instance_data_list_free(DRWInstanceDataList *idatalist)
{
	DRWInstanceBuffer *ibuf = idatalist->ibuffers.ibufs;
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

	for (int i = 0; i < idatalist->ibuffers.alloc_size; i++, ibuf++) {
		GWN_BATCH_DISCARD_SAFE(ibuf->batch);
	}
	MEM_freeN(idatalist->ibuffers.ibufs);
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
