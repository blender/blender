/*
 * BME_customdata.c    jan 2007
 *
 *	Custom Data functions for Bmesh
 *
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle, Brecht Van Lommel, Ben Batt
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/BME_Customdata.c
 *  \ingroup bke
 */


#include <string.h>

#include "MEM_guardedalloc.h"
#include "BKE_bmeshCustomData.h"
#include "bmesh_private.h"

/********************* Layer type information **********************/
typedef struct BME_LayerTypeInfo {
	int size;
	const char *defaultname;
	void (*copy)(const void *source, void *dest, int count);
	void (*free)(void *data, int count, int size);
	void (*interp)(void **sources, float *weights, float *sub_weights, int count, void *dest);
	void (*set_default)(void *data, int count);
} BME_LayerTypeInfo;
const BME_LayerTypeInfo BMELAYERTYPEINFO[BME_CD_NUMTYPES] = {
	{sizeof(BME_facetex), "TexFace", NULL, NULL, NULL, NULL},
	{sizeof(BME_looptex), "UV", NULL, NULL, NULL, NULL},
	{sizeof(BME_loopcol), "VCol", NULL, NULL, NULL, NULL},
	{sizeof(BME_DeformVert), "Group", NULL, NULL, NULL, NULL}
};
static const BME_LayerTypeInfo *BME_layerType_getInfo(int type)
{
	if(type < 0 || type >= CD_NUMTYPES) return NULL;

	return &BMELAYERTYPEINFO[type];
}
void BME_CD_Create(BME_CustomData *data, BME_CustomDataInit *init, int initalloc)
{
	int i, j, offset=0;
	const BME_LayerTypeInfo *info;
	
	/*initialize data members*/
	data->layers = NULL;
	data->pool = NULL;
	data->totlayer = 0;
	data->totsize = 0;

	/*first count how many layers to alloc*/
	for(i=0; i < BME_CD_NUMTYPES; i++){
		info = BME_layerType_getInfo(i);
		data->totlayer += init->layout[i];
		data->totsize  += (init->layout[i] * info->size);
	}
	/*alloc our layers*/
	if(data->totlayer){
		/*alloc memory*/
		data->layers = MEM_callocN(sizeof(BME_CustomDataLayer)*data->totlayer, "BMesh Custom Data Layers");
		data->pool = BLI_mempool_create(data->totsize, initalloc, initalloc, FALSE, FALSE);
		/*initialize layer data*/
		for(i=0; i < BME_CD_NUMTYPES; i++){
			if(init->layout[i]){
				info = BME_layerType_getInfo(i);
				for(j=0; j < init->layout[i]; j++){
					if(j==0) data->layers[j+i].active = init->active[i];
					data->layers[j+i].type = i;
					data->layers[j+i].offset = offset;	
					strcpy(data->layers[j+i].name, &(init->nametemplate[j+i]));
					offset += info->size;
				}
			}
		}
	}
}

void BME_CD_Free(BME_CustomData *data)
{
	if(data->pool) BLI_mempool_destroy(data->pool);
}

/*Block level ops*/
void BME_CD_free_block(BME_CustomData *data, void **block)
{
	const BME_LayerTypeInfo *typeInfo;
	int i;

	if(!*block) return;
	for(i = 0; i < data->totlayer; ++i) {
		typeInfo = BME_layerType_getInfo(data->layers[i].type);
		if(typeInfo->free) {
			int offset = data->layers[i].offset;
			typeInfo->free((char*)*block + offset, 1, typeInfo->size);
		}
	}
	BLI_mempool_free(data->pool, *block);
	*block = NULL;
}


static void BME_CD_alloc_block(BME_CustomData *data, void **block)
{	
	
	if (*block) BME_CD_free_block(data, block); //if we copy layers that have their own free functions like deformverts
	
	if (data->totsize > 0)
		*block = BLI_mempool_alloc(data->pool);	
	else
		*block = NULL;
}

void BME_CD_copy_data(const BME_CustomData *source, BME_CustomData *dest,
							void *src_block, void **dest_block)
{
	const BME_LayerTypeInfo *typeInfo;
	int dest_i, src_i;

	if (!*dest_block) /*for addXXXlist functions!*/
		BME_CD_alloc_block(dest, dest_block);
	
	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->totlayer; ++src_i) {

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->totlayer
			  && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->totlayer) return;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type &&
			strcmp(dest->layers[dest_i].name, source->layers[src_i].name) == 0) {
			char *src_data = (char*)src_block + source->layers[src_i].offset;
			char *dest_data = (char*)*dest_block + dest->layers[dest_i].offset;

			typeInfo = BME_layerType_getInfo(source->layers[src_i].type);

			if(typeInfo->copy)
				typeInfo->copy(src_data, dest_data, 1);
			else
				memcpy(dest_data, src_data, typeInfo->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}
}
void BME_CD_set_default(BME_CustomData *data, void **block)
{
	const BME_LayerTypeInfo *typeInfo;
	int i;

	if (!*block)
		BME_CD_alloc_block(data, block); //for addXXXlist functions...

	for(i = 0; i < data->totlayer; ++i) {
		int offset = data->layers[i].offset;

		typeInfo = BME_layerType_getInfo(data->layers[i].type);

		if(typeInfo->set_default)
			typeInfo->set_default((char*)*block + offset, 1);
	}
}
