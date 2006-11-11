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
*/ 

/* CustomData interface, see also DNA_customdata_types.h. */

#ifndef BKE_CUSTOMDATA_H
#define BKE_CUSTOMDATA_H

struct CustomData;

#define ORIGINDEX_NONE -1 /* indicates no original index for this element */

/* layer flags - to be used with CustomData_add_layer */

/* indicates layer should not be copied by CustomData_from_template or
 * CustomData_copy_data (for temporary utility layers)
 */
#define LAYERFLAG_NOCOPY 1<<0

/* indicates layer should not be freed (for layers backed by external data)
 */
#define LAYERFLAG_NOFREE 1<<1

/* initialises a CustomData object with space for the given number
 * of data layers and the given number of elements per layer
 */
void CustomData_init(struct CustomData *data,
                     int maxLayers, int maxElems, int subElems);

/* initialises a CustomData object with the same layer setup as source
 * and memory space for maxElems elements. flag is added to all layer flags
 */
void CustomData_from_template(const struct CustomData *source,
                              struct CustomData *dest, int flag, int maxElems);

/* frees data associated with a CustomData object (doesn't free the object
 * itself, though)
 */
void CustomData_free(struct CustomData *data);

/* adds a data layer of the given type to the CustomData object, optionally
 * backed by an external data array
 * if layer != NULL, it is used as the layer data array, otherwise new memory
 * is allocated
 * the layer data will be freed by CustomData_free unless
 * (flag & LAYERFLAG_NOFREE) is true
 * grows the number of layers in data if data->maxLayers has been reached
 * returns 1 on success, 0 on failure
 */
int CustomData_add_layer(struct CustomData *data, int type, int flag,
                         void *layer);

/* frees the first data layer with the give type.
 * returns 1 on succes, 0 if no layer with the given type is found
 */
int CustomData_free_layer(struct CustomData *data, int type);

/* returns 1 if the two objects are compatible (same layer types and
 * flags in the same order), 0 if not
 */
int CustomData_compat(const struct CustomData *data1,
                      const struct CustomData *data2);

/* returns 1 if a layer with the specified type exists */
int CustomData_has_layer(const struct CustomData *data, int type);

/* copies data from one CustomData object to another
 * objects need not be compatible, each source layer is copied to the
 * first dest layer of correct type (if there is none, the layer is skipped)
 * return 1 on success, 0 on failure
 */
int CustomData_copy_data(const struct CustomData *source,
                         struct CustomData *dest, int source_index,
                         int dest_index, int count);
int CustomData_em_copy_data(struct CustomData *data, void *src_block,
                            void **dest_block);

/* frees data in a CustomData object
 * return 1 on success, 0 on failure
 */
int CustomData_free_elem(struct CustomData *data, int index, int count);

/* interpolates data from one CustomData object to another
 * objects need not be compatible, each source layer is interpolated to the
 * first dest layer of correct type (if there is none, the layer is skipped)
 * if weights == NULL or sub_weights == NULL, they default to all 1's
 *
 * src_indices gives the source elements to interpolate from
 * weights gives the weight for each source element
 * sub_weights is an array of matrices of weights for sub-elements (matrices
 *     should be source->subElems * source->subElems in size)
 * count gives the number of source elements to interpolate from
 * dest_index gives the dest element to write the interpolated value to
 *
 * returns 1 on success, 0 on failure
 */
int CustomData_interp(const struct CustomData *source, struct CustomData *dest,
                      int *src_indices, float *weights, float *sub_weights,
                      int count, int dest_index);
int CustomData_em_interp(struct CustomData *data,  void **src_blocks,
                         float *weights, float *sub_weights, int count,
                         void *dest_block);

/* gets a pointer to the data element at index from the first layer of type
 * returns NULL if there is no layer of type
 */
void *CustomData_get(const struct CustomData *data, int index, int type);
void *CustomData_em_get(const struct CustomData *data, void *block, int type);

/* gets a pointer to the first layer of type
 * returns NULL if there is no layer of type
 */
void *CustomData_get_layer(const struct CustomData *data, int type);

/* copies the data from source to the data element at index in the first
 * layer of type
 * no effect if there is no layer of type
 */
void CustomData_set(const struct CustomData *data, int index, int type,
                    void *source);
void CustomData_em_set(struct CustomData *data, void *block, int type,
                       void *source);

/* sets the number of elements in a CustomData object
 * if the value given is more than the maximum, the maximum is used
 */
void CustomData_set_num_elems(struct CustomData *data, int numElems);

/* alloc/free a block of custom data attached to one element in editmode */
void CustomData_em_set_default(struct CustomData *data, void **block);
void CustomData_em_free_block(struct CustomData *data, void **block);

/* copy custom data to/from layers as in mesh/derivedmesh, to editmesh
   blocks of data. the CustomData's must be compatible  */
void CustomData_to_em_block(const struct CustomData *source,
                            struct CustomData *dest, int index, void **block);
void CustomData_from_em_block(const struct CustomData *source,
                              struct CustomData *dest, void *block, int index);

#endif
