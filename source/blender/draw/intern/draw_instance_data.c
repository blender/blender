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

#include "MEM_guardedalloc.h"
#include "BLI_utildefines.h"

#define MAX_INSTANCE_DATA_SIZE 32 /* Can be adjusted for more */

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
};

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
	return MEM_callocN(sizeof(DRWInstanceDataList), "DRWInstanceDataList");
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
