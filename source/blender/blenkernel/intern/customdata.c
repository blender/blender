/*
* $Id$
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
* along with this program; if not, write to the Free Software  Foundation,
* Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
* The Original Code is Copyright (C) 2006 Blender Foundation.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): Ben Batt <benbatt@gmail.com>
*
* ***** END GPL LICENSE BLOCK *****
*
* Implementation of CustomData.
*
* BKE_customdata.h contains the function prototypes for this file.
*
*/ 

#include "BKE_customdata.h"

#include "BLI_linklist.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include <string.h>

/* number of layers to add when growing a CustomData object */
#define CUSTOMDATA_GROW 5

/* descriptor and storage for a custom data layer */
typedef struct LayerDesc {
	int type;   /* type of data in layer */
	int flag;   /* general purpose flag */
	void *data; /* layer data */
} LayerDesc;

/********************* Layer type information **********************/
typedef struct LayerTypeInfo {
	int size; /* the memory size of one element of this layer's data */

	/* a function to copy count elements of this layer's data
	 * (deep copy if appropriate)
	 * size should be the size of one element of this layer's data (e.g.
	 * LayerTypeInfo.size)
	 * if NULL, memcpy is used
	 */
	void (*copy)(const void *source, void *dest, int count, int size);

	/* a function to free any dynamically allocated components of this
	 * layer's data (note the data pointer itself should not be freed)
	 * size should be the size of one element of this layer's data (e.g.
	 * LayerTypeInfo.size)
	 */
	void (*free)(void *data, int count, int size);

	/* a function to interpolate between count source elements of this
	 * layer's data and store the result in dest
	 * if weights == NULL or sub_weights == NULL, they should default to 1
	 *
	 * weights gives the weight for each element in sources
	 * sub_weights gives the sub-element weights for each element in sources
	 *    (there should be (sub element count)^2 weights per element)
	 * count gives the number of elements in sources
	 */
	void (*interp)(void **sources, float *weights, float *sub_weights,
	               int count, void *dest);
} LayerTypeInfo;

static void layerCopy_mdeformvert(const void *source, void *dest,
                                  int count, int size);

static void layerFree_mdeformvert(void *data, int count, int size);

static void layerInterp_mdeformvert(void **sources, float *weights,
                                    float *sub_weights, int count, void *dest);

static void layerInterp_tface(void **sources, float *weights,
                              float *sub_weights, int count, void *dest);

static void layerInterp_mcol(void **sources, float *weights,
                             float *sub_weights, int count, void *dest);

const LayerTypeInfo LAYERTYPEINFO[LAYERTYPE_NUMTYPES] = {
	{sizeof(MVert), NULL, NULL, NULL},
	{sizeof(MSticky), NULL, NULL, NULL},
	{sizeof(MDeformVert),
	 layerCopy_mdeformvert, layerFree_mdeformvert, layerInterp_mdeformvert},
	{sizeof(MEdge), NULL, NULL, NULL},
	{sizeof(MFace), NULL, NULL, NULL},
	{sizeof(TFace), NULL, NULL, layerInterp_tface},
	/* 4 MCol structs per face */
	{sizeof(MCol) * 4, NULL, NULL, layerInterp_mcol},
	{sizeof(int), NULL, NULL, NULL},
	/* 3 floats per normal vector */
	{sizeof(float) * 3, NULL, NULL, NULL},
	{sizeof(int), NULL, NULL, NULL},
};

const LayerTypeInfo *layerType_getInfo(int type)
{
	if(type < 0 || type >= LAYERTYPE_NUMTYPES) return NULL;

	return &LAYERTYPEINFO[type];
}

static void layerCopy_mdeformvert(const void *source, void *dest,
                                  int count, int size)
{
	int i;

	memcpy(dest, source, count * size);

	for(i = 0; i < count; ++i) {
		MDeformVert *dvert = (MDeformVert *)((char *)dest + i * size);
		MDeformWeight *dw = MEM_callocN(dvert->totweight * sizeof(*dw),
		                                "layerCopy_mdeformvert dw");

		memcpy(dw, dvert->dw, dvert->totweight * sizeof(*dw));
		dvert->dw = dw;
	}
}

static void layerFree_mdeformvert(void *data, int count, int size)
{
	int i;

	for(i = 0; i < count; ++i) {
		MDeformVert *dvert = (MDeformVert *)((char *)data + i * size);

		if(dvert->dw) {
			MEM_freeN(dvert->dw);
			dvert->dw = NULL;
		}
	}
}

static void linklist_free_simple(void *link)
{
	MEM_freeN(link);
}

static void layerInterp_mdeformvert(void **sources, float *weights,
                                    float *sub_weights, int count, void *dest)
{
	MDeformVert *dvert = dest;
	LinkNode *dest_dw = NULL; /* a list of lists of MDeformWeight pointers */
	LinkNode *node;
	int i, j, totweight;

	if(count <= 0) return;

	/* build a list of unique def_nrs for dest */
	totweight = 0;
	for(i = 0; i < count; ++i) {
		MDeformVert *source = sources[i];
		float interp_weight = weights ? weights[i] : 1.0f;

		for(j = 0; j < source->totweight; ++j) {
			MDeformWeight *dw = &source->dw[j];

			for(node = dest_dw; node; node = node->next) {
				MDeformWeight *tmp_dw = (MDeformWeight *)node->link;

				if(tmp_dw->def_nr == dw->def_nr) {
					tmp_dw->weight += dw->weight * interp_weight;
					break;
				}
			}

			/* if this def_nr is not in the list, add it */
			if(!node) {
				MDeformWeight *tmp_dw = MEM_callocN(sizeof(*tmp_dw),
				                            "layerInterp_mdeformvert tmp_dw");
				tmp_dw->def_nr = dw->def_nr;
				tmp_dw->weight = dw->weight * interp_weight;
				BLI_linklist_prepend(&dest_dw, tmp_dw);
				totweight++;
			}
		}
	}

	/* now we know how many unique deform weights there are, so realloc */
	if(dvert->dw) MEM_freeN(dvert->dw);
	dvert->dw = MEM_callocN(sizeof(*dvert->dw) * totweight,
	                        "layerInterp_mdeformvert dvert->dw");
	dvert->totweight = totweight;

	for(i = 0, node = dest_dw; node; node = node->next, ++i)
		dvert->dw[i] = *((MDeformWeight *)node->link);

	BLI_linklist_free(dest_dw, linklist_free_simple);
}

static void layerInterp_tface(void **sources, float *weights,
                              float *sub_weights, int count, void *dest)
{
	TFace *tf = dest;
	int i, j, k;
	float uv[4][2];
	float col[4][4];

	if(count <= 0) return;

	memset(uv, 0, sizeof(uv));
	memset(col, 0, sizeof(col));

	for(i = 0; i < count; ++i) {
		float weight = weights ? weights[i] : 1;
		TFace *src = sources[i];

		for(j = 0; j < 4; ++j) {
			if(sub_weights) {
				for(k = 0; k < 4; ++k) {
					float sub_weight = sub_weights[j * 4 + k];
					char *tmp_col = (char *)&src->col[k];
					float *tmp_uv = src->uv[k];

					uv[j][0] += tmp_uv[0] * sub_weight * weight;
					uv[j][1] += tmp_uv[1] * sub_weight * weight;

					col[j][0] += tmp_col[0] * sub_weight * weight;
					col[j][1] += tmp_col[1] * sub_weight * weight;
					col[j][2] += tmp_col[2] * sub_weight * weight;
					col[j][3] += tmp_col[3] * sub_weight * weight;
				}
			} else {
				char *tmp_col = (char *)&src->col[j];
				uv[j][0] += src->uv[j][0] * weight;
				uv[j][1] += src->uv[j][1] * weight;

				col[j][0] += tmp_col[0] * weight;
				col[j][1] += tmp_col[1] * weight;
				col[j][2] += tmp_col[2] * weight;
				col[j][3] += tmp_col[3] * weight;
			}
		}
	}

	*tf = *(TFace *)sources[0];
	for(j = 0; j < 4; ++j) {
		char *tmp_col = (char *)&tf->col[j];

		tf->uv[j][0] = uv[j][0];
		tf->uv[j][1] = uv[j][1];

		tmp_col[0] = (int)col[j][0];
		tmp_col[1] = (int)col[j][1];
		tmp_col[2] = (int)col[j][2];
		tmp_col[3] = (int)col[j][3];
	}
}

static void layerInterp_mcol(void **sources, float *weights,
                             float *sub_weights, int count, void *dest)
{
	MCol *mc = dest;
	int i, j, k;
	struct {
		float a;
		float r;
		float g;
		float b;
	} col[4];

	if(count <= 0) return;

	memset(col, 0, sizeof(col));

	for(i = 0; i < count; ++i) {
		float weight = weights ? weights[i] : 1;
		float *sub_weight = sub_weights;

		for(j = 0; j < 4; ++j) {
			if(sub_weights) {
				MCol *src = sources[i];
				for(k = 0; k < 4; ++k, ++sub_weight, ++src) {
					col[j].a += src->a * (*sub_weight) * weight;
					col[j].r += src->r * (*sub_weight) * weight;
					col[j].g += src->g * (*sub_weight) * weight;
					col[j].b += src->b * (*sub_weight) * weight;
				}
			} else {
				MCol *src = sources[i];
				col[j].a += src[j].a * weight;
				col[j].r += src[j].r * weight;
				col[j].g += src[j].g * weight;
				col[j].b += src[j].b * weight;
			}
		}
	}

	for(j = 0; j < 4; ++j) {
		mc[j].a = (int)col[j].a;
		mc[j].r = (int)col[j].r;
		mc[j].g = (int)col[j].g;
		mc[j].b = (int)col[j].b;
	}
}

/********************* CustomData functions *********************/
void CustomData_init(CustomData *data,
                     int maxLayers, int maxElems, int subElems)
{
	data->layers = MEM_callocN(maxLayers * sizeof(*data->layers),
	                            "CustomData->layers");
	data->numLayers = 0;
	data->maxLayers = maxLayers;
	data->numElems = maxElems;
	data->maxElems = maxElems;
	data->subElems = subElems;
}

void CustomData_from_template(const CustomData *source, CustomData *dest,
                              int maxElems)
{
	int i;

	CustomData_init(dest, source->maxLayers, maxElems, source->subElems);

	for(i = 0; i < source->numLayers; ++i) {
		if(source->layers[i].flag & LAYERFLAG_NOCOPY) continue;

		CustomData_add_layer(dest, source->layers[i].type,
		                     source->layers[i].flag & ~LAYERFLAG_NOFREE, NULL);
	}
}

void CustomData_free(CustomData *data)
{
	int i;
	const LayerTypeInfo *typeInfo;

	for(i = 0; i < data->numLayers; ++i) {
		if(!(data->layers[i].flag & LAYERFLAG_NOFREE)) {
			typeInfo = layerType_getInfo(data->layers[i].type);
			if(typeInfo->free)
				typeInfo->free(data->layers[i].data, data->numElems,
				               typeInfo->size);

			MEM_freeN(data->layers[i].data);
		}
	}

	data->numLayers = 0;

	if(data->layers) {
		MEM_freeN(data->layers);
		data->layers = NULL;
	}
}

static int customData_grow(CustomData *data, int amount)
{
	LayerDesc *tmp = MEM_callocN(sizeof(*tmp) * (data->maxLayers + amount),
                                 "CustomData->layers");
	if(!tmp) return 0;

	data->maxLayers += amount;
	memcpy(tmp, data->layers, sizeof(*tmp) * data->numLayers);

	MEM_freeN(data->layers);
	data->layers = tmp;

	return 1;
}

static int customData_add_layer__internal(CustomData *data, int type,
                                          int flag, void *layer)
{
	int index = data->numLayers;

	if(index >= data->maxLayers)
		if(!customData_grow(data, CUSTOMDATA_GROW)) return 0;

	/* keep layers ordered by type */
	for( ; index > 0 && data->layers[index - 1].type > type; --index)
		data->layers[index] = data->layers[index - 1];

	data->layers[index].type = type;
	data->layers[index].flag = flag;
	data->layers[index].data = layer;

	data->numLayers++;

	return 1;
}

int CustomData_add_layer(CustomData *data, int type, int flag, void *layer)
{
	int size = layerType_getInfo(type)->size * data->numElems;
	void *tmp_layer = layer;

	if(!layer) tmp_layer = MEM_callocN(size, "LayerDesc.data");

	if(!tmp_layer) return 0;

	if(customData_add_layer__internal(data, type, flag, tmp_layer))
		return 1;
	else {
		MEM_freeN(tmp_layer);
		return 0;
	}
}

int CustomData_compat(const CustomData *data1, const CustomData *data2)
{
	int i;

	if(data1->numLayers != data2->numLayers) return 0;

	for(i = 0; i < data1->numLayers; ++i) {
		if(data1->layers[i].type != data2->layers[i].type) return 0;
		if(data1->layers[i].flag != data2->layers[i].flag) return 0;
	}

	return 1;
}

/* gets index of first layer matching type after start_index
 * if start_index < 0, starts searching at 0
 * returns -1 if there is no layer of type
 */
static int CustomData_find_next(const CustomData *data, int type,
                                int start_index)
{
	int i = start_index + 1;

	if(i < 0) i = 0;

	for(; i < data->numLayers; ++i)
		if(data->layers[i].type == type) return i;

	return -1;
}

int CustomData_copy_data(const CustomData *source, CustomData *dest,
                         int source_index, int dest_index, int count)
{
	const LayerTypeInfo *type_info;
	int src_i, dest_i;
	int src_offset;
	int dest_offset;

	if(count < 0) return 0;
	if(source_index < 0 || (source_index + count) > source->numElems)
		return 0;
	if(dest_index < 0 || (dest_index + count) > dest->numElems)
		return 0;

	/* copies a layer at a time */
	dest_i = 0;
	for(src_i = 0; src_i < source->numLayers; ++src_i) {
		if(source->layers[src_i].flag & LAYERFLAG_NOCOPY) continue;

		/* find the first dest layer with type >= the source type
		 * (this should work because layers are ordered by type)
		 */
		while(dest_i < dest->numLayers
		      && dest->layers[dest_i].type < source->layers[src_i].type)
			++dest_i;

		/* if there are no more dest layers, we're done */
		if(dest_i >= dest->numLayers) return 1;

		/* if we found a matching layer, copy the data */
		if(dest->layers[dest_i].type == source->layers[src_i].type) {
			char *src_data = source->layers[src_i].data;
			char *dest_data = dest->layers[dest_i].data;

			type_info = layerType_getInfo(source->layers[src_i].type);

			src_offset = source_index * type_info->size;
			dest_offset = dest_index * type_info->size;

			if(type_info->copy)
				type_info->copy(src_data + src_offset,
				                dest_data + dest_offset,
				                count, type_info->size);
			else
				memcpy(dest_data + dest_offset,
				       src_data + src_offset,
				       count * type_info->size);

			/* if there are multiple source & dest layers of the same type,
			 * we don't want to copy all source layers to the same dest, so
			 * increment dest_i
			 */
			++dest_i;
		}
	}

	return 1;
}

int CustomData_free_elem(CustomData *data, int index, int count)
{
	int i;
	const LayerTypeInfo *typeInfo;

	if(index < 0 || count <= 0 || index + count > data->numElems) return 0;

	for(i = 0; i < data->numLayers; ++i) {
		if(!(data->layers[i].flag & LAYERFLAG_NOFREE)) {
			typeInfo = layerType_getInfo(data->layers[i].type);

			if(typeInfo->free) {
				int offset = typeInfo->size * index;

				typeInfo->free((char *)data->layers[i].data + offset,
				               count, typeInfo->size);
			}
		}
	}

	return 1;
}

#define SOURCE_BUF_SIZE 100

int CustomData_interp(const CustomData *source, CustomData *dest,
                      int *src_indices, float *weights, float *sub_weights,
                      int count, int dest_index)
{
	int src_i, dest_i;
	int dest_offset;
	int j;
	void *source_buf[SOURCE_BUF_SIZE];
	void **sources = source_buf;

	if(count <= 0) return 0;
	if(dest_index < 0 || dest_index >= dest->numElems) return 0;

	/* slow fallback in case we're interpolating a ridiculous number of
	 * elements
	 */
	if(count > SOURCE_BUF_SIZE)
		sources = MEM_callocN(sizeof(*sources) * count,
		                      "CustomData_interp sources");

	/* interpolates a layer at a time */
	for(src_i = 0; src_i < source->numLayers; ++src_i) {
		LayerDesc *source_layer = &source->layers[src_i];
		const LayerTypeInfo *type_info =
		                        layerType_getInfo(source_layer->type);

		dest_i = CustomData_find_next(dest, source_layer->type, -1);

		if(dest_i >= 0 && type_info->interp) {
			void *src_data = source_layer->data; 

			for(j = 0; j < count; ++j)
				sources[j] = (char *)src_data
				             + type_info->size * src_indices[j];

			dest_offset = dest_index * type_info->size;

			type_info->interp(sources, weights, sub_weights, count,
			               (char *)dest->layers[dest_i].data + dest_offset);
		}
	}

	if(count > SOURCE_BUF_SIZE) MEM_freeN(sources);
	return 1;
}

void *CustomData_get(const CustomData *data, int index, int type)
{
	int offset;
	int layer_index;
	
	if(index < 0 || index > data->numElems) return NULL;

	/* get the layer index of the first layer of type */
	layer_index = CustomData_find_next(data, type, -1);
	if(layer_index < 0) return NULL;

	/* get the offset of the desired element */
	offset = layerType_getInfo(type)->size * index;

	return (char *)data->layers[layer_index].data + offset;
}

void *CustomData_get_layer(const CustomData *data, int type)
{
	/* get the layer index of the first layer of type */
	int layer_index = CustomData_find_next(data, type, -1);

	if(layer_index < 0) return NULL;

	return data->layers[layer_index].data;
}

void CustomData_set(const CustomData *data, int index, int type, void *source)
{
	void *dest = CustomData_get(data, index, type);
	const LayerTypeInfo *type_info = layerType_getInfo(type);

	if(!dest) return;

	if(type_info->copy)
		type_info->copy(source, dest, 1, type_info->size);
	else
		memcpy(dest, source, type_info->size);
}

void CustomData_set_num_elems(CustomData *data, int numElems)
{
	if(numElems < 0) return;
	if(numElems < data->maxElems) data->numElems = numElems;
	else data->numElems = data->maxElems;
}
