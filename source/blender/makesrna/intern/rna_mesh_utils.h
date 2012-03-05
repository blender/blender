/* 
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Andrew Wiggin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/source/blender/makesrna/intern/rna_mesh_utils.h
 *  \ingroup RNA
 */
 
#ifndef __RNA_MESH_UTILS_H__
#define __RNA_MESH_UTILS_H__

/* Macros to help reduce code clutter in rna_mesh.c */

/* Define the accessors for a basic CustomDataLayer collection */
#define DEFINE_CUSTOMDATA_LAYER_COLLECTION(collection_name, customdata_type, layer_type) \
																								\
	static int rna_##collection_name##_check(CollectionPropertyIterator *iter, void *data) \
	{																							\
		CustomDataLayer *layer = (CustomDataLayer*)data;							\
		return (layer->type != layer_type);											\
	}																							\
																								\
	static void rna_Mesh_##collection_name##s_begin(CollectionPropertyIterator *iter, PointerRNA *ptr) \
	{																							\
		CustomData *data = rna_mesh_##customdata_type(ptr);					\
		rna_iterator_array_begin(iter, (void*)data->layers, sizeof(CustomDataLayer), data->totlayer, 0, rna_##collection_name##_check); \
	}																							\
																								\
	static int rna_Mesh_##collection_name##s_length(PointerRNA *ptr)		\
	{																							\
		CustomData *data = rna_mesh_##customdata_type(ptr);					\
		return data ? CustomData_number_of_layers(data, layer_type) : 0;	\
	}																							\
																								\
	static void rna_Mesh_##collection_name##_index_range(PointerRNA *ptr, int *min, int *max) \
	{																							\
		CustomData *data = rna_mesh_##customdata_type(ptr);						\
																								\
		*min = 0;																				\
		*max = data ? CustomData_number_of_layers(data, layer_type) - 1 : 0;		\
		*max = MAX2(0, *max);																\
	}

/* Define the accessors for special CustomDataLayers in the collection
   (active, render, clone, stencil, etc) */
#define DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM(collection_name, customdata_type, layer_type, active_type, layer_rna_type) \
																								\
	static PointerRNA rna_Mesh_##collection_name##_##active_type##_get(PointerRNA *ptr) \
	{																							\
		CustomData *data = rna_mesh_##customdata_type(ptr);					\
		int index = CustomData_get_##active_type##_layer_index(data, layer_type); \
		CustomDataLayer *cdl = (index == -1)? NULL: &data->layers[index];	\
																								\
		return rna_pointer_inherit_refine(ptr, &RNA_##layer_rna_type, cdl); \
	}																							\
																								\
	static void rna_Mesh_##collection_name##_##active_type##_set(PointerRNA *ptr, PointerRNA value) \
	{																							\
		Mesh *me = rna_mesh(ptr);														\
		CustomData *data = rna_mesh_##customdata_type(ptr);					\
		CustomDataLayer *cdl;															\
		int a;																				\
																								\
		for (cdl = data->layers, a = 0; a<data->totlayer; cdl++, a++) {			\
			if (value.data == cdl) {														\
				CustomData_set_layer_##active_type##_index(data, layer_type, a); \
				mesh_update_customdata_pointers(me, TRUE);								\
				return;																		\
			}																					\
		}																						\
	}																							\
																								\
	static int rna_Mesh_##collection_name##_##active_type##_index_get(PointerRNA *ptr) \
	{																							\
		CustomData *data = rna_mesh_##customdata_type(ptr);					\
		return CustomData_get_##active_type##_layer(data, layer_type);		\
	}																							\
																								\
	static void rna_Mesh_##collection_name##_##active_type##_index_set(PointerRNA *ptr, int value) \
	{																							\
		Mesh *me = rna_mesh(ptr);														\
		CustomData *data = rna_mesh_##customdata_type(ptr);					\
																								\
		CustomData_set_layer_##active_type(data, layer_type, value);		\
		mesh_update_customdata_pointers(me, TRUE);										\
	}

#endif /* __RNA_MESH_UTILS_H__ */
