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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file BKE_customdata.h
 *  \ingroup bke
 *  \author Ben Batt
 *  \brief CustomData interface, see also DNA_customdata_types.h.
 */

#ifndef __BKE_CUSTOMDATA_H__
#define __BKE_CUSTOMDATA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"

struct BMesh;
struct ID;
struct CustomData;
struct CustomDataLayer;
typedef uint64_t CustomDataMask;

/*a data type large enough to hold 1 element from any customdata layer type*/
typedef struct {unsigned char data[64]; } CDBlockBytes;

extern const CustomDataMask CD_MASK_BAREMESH;
extern const CustomDataMask CD_MASK_MESH;
extern const CustomDataMask CD_MASK_EDITMESH;
extern const CustomDataMask CD_MASK_DERIVEDMESH;
extern const CustomDataMask CD_MASK_BMESH;
extern const CustomDataMask CD_MASK_FACECORNERS;
extern const CustomDataMask CD_MASK_EVERYTHING;

/* for ORIGINDEX layer type, indicates no original index for this element */
#define ORIGINDEX_NONE -1

/* initializes a CustomData object with the same layer setup as source and
 * memory space for totelem elements. mask must be an array of length
 * CD_NUMTYPES elements, that indicate if a layer can be copied. */

/* add/copy/merge allocation types */
#define CD_ASSIGN    0  /* use the data pointer */
#define CD_CALLOC    1  /* allocate blank memory */
#define CD_DEFAULT   2  /* allocate and set to default */
#define CD_REFERENCE 3  /* use data pointers, set layer flag NOFREE */
#define CD_DUPLICATE 4  /* do a full copy of all layers, only allowed if source
                         * has same number of elements */

#define CD_TYPE_AS_MASK(_type) (CustomDataMask)((CustomDataMask)1 << (CustomDataMask)(_type))

void customData_mask_layers__print(CustomDataMask mask);

typedef void (*cd_interp)(void **sources, const float *weights, const float *sub_weights, int count, void *dest);
typedef void (*cd_copy)(const void *source, void *dest, int count);

/**
 * Checks if the layer at physical offset \a layer_n (in data->layers) support math
 * the below operations.
 */
bool CustomData_layer_has_math(const struct CustomData *data, int layer_n);
bool CustomData_layer_has_interp(const struct CustomData *data, int layer_n);

/**
 * Checks if any of the customdata layers has math.
 */
bool CustomData_has_math(const struct CustomData *data);
bool CustomData_has_interp(const struct CustomData *data);
bool CustomData_bmesh_has_free(const struct CustomData *data);

/* copies the "value" (e.g. mloopuv uv or mloopcol colors) from one block to
 * another, while not overwriting anything else (e.g. flags).  probably only
 * implemented for mloopuv/mloopcol, for now.*/
void CustomData_data_copy_value(int type, const void *source, void *dest);

/* Same as above, but doing advanced mixing. Only available for a few types of data (like colors...). */
void CustomData_data_mix_value(int type, const void *source, void *dest, const int mixmode, const float mixfactor);

/* compares if data1 is equal to data2.  type is a valid CustomData type
 * enum (e.g. CD_MLOOPUV). the layer type's equal function is used to compare
 * the data, if it exists, otherwise memcmp is used.*/
bool CustomData_data_equals(int type, const void *data1, const void *data2);
void CustomData_data_initminmax(int type, void *min, void *max);
void CustomData_data_dominmax(int type, const void *data, void *min, void *max);
void CustomData_data_multiply(int type, void *data, float fac);
void CustomData_data_add(int type, void *data1, const void *data2);

/* initializes a CustomData object with the same layer setup as source.
 * mask is a bitfield where (mask & (1 << (layer type))) indicates
 * if a layer should be copied or not. alloctype must be one of the above. */
void CustomData_copy(const struct CustomData *source, struct CustomData *dest,
                     CustomDataMask mask, int alloctype, int totelem);

/* BMESH_TODO, not really a public function but readfile.c needs it */
void CustomData_update_typemap(struct CustomData *data);

/* same as the above, except that this will preserve existing layers, and only
 * add the layers that were not there yet */
bool CustomData_merge(const struct CustomData *source, struct CustomData *dest,
                      CustomDataMask mask, int alloctype, int totelem);

/* bmesh version of CustomData_merge; merges the layouts of source and dest,
 * then goes through the mesh and makes sure all the customdata blocks are
 * consistent with the new layout.*/
bool CustomData_bmesh_merge(struct CustomData *source, struct CustomData *dest,
                            CustomDataMask mask, int alloctype, struct BMesh *bm, const char htype);

/** NULL's all members and resets the typemap. */
void CustomData_reset(struct CustomData *data);

/** frees data associated with a CustomData object (doesn't free the object
 * itself, though)
 */
void CustomData_free(struct CustomData *data, int totelem);

/* frees all layers with CD_FLAG_TEMPORARY */
void CustomData_free_temporary(struct CustomData *data, int totelem);

/* adds a data layer of the given type to the CustomData object, optionally
 * backed by an external data array. the different allocation types are
 * defined above. returns the data of the layer.
 */
void *CustomData_add_layer(struct CustomData *data, int type, int alloctype,
                           void *layer, int totelem);
/*same as above but accepts a name */
void *CustomData_add_layer_named(struct CustomData *data, int type, int alloctype,
                                 void *layer, int totelem, const char *name);

/* frees the active or first data layer with the give type.
 * returns 1 on success, 0 if no layer with the given type is found
 *
 * in editmode, use EDBM_data_layer_free instead of this function
 */
bool CustomData_free_layer(struct CustomData *data, int type, int totelem, int index);

/* frees the layer index with the give type.
 * returns 1 on success, 0 if no layer with the given type is found
 *
 * in editmode, use EDBM_data_layer_free instead of this function
 */
bool CustomData_free_layer_active(struct CustomData *data, int type, int totelem);

/* same as above, but free all layers with type */
void CustomData_free_layers(struct CustomData *data, int type, int totelem);

/* returns 1 if a layer with the specified type exists */
bool CustomData_has_layer(const struct CustomData *data, int type);

/* returns the number of layers with this type */
int CustomData_number_of_layers(const struct CustomData *data, int type);
int CustomData_number_of_layers_typemask(const struct CustomData *data, CustomDataMask mask);

/* duplicate data of a layer with flag NOFREE, and remove that flag.
 * returns the layer data */
void *CustomData_duplicate_referenced_layer(struct CustomData *data, const int type, const int totelem);
void *CustomData_duplicate_referenced_layer_n(struct CustomData *data, const int type, const int n, const int totelem);
void *CustomData_duplicate_referenced_layer_named(struct CustomData *data,
                                                  const int type, const char *name, const int totelem);
bool CustomData_is_referenced_layer(struct CustomData *data, int type);

/* set the CD_FLAG_NOCOPY flag in custom data layers where the mask is
 * zero for the layer type, so only layer types specified by the mask
 * will be copied
 */
void CustomData_set_only_copy(const struct CustomData *data,
                              CustomDataMask mask);

/* copies data from one CustomData object to another
 * objects need not be compatible, each source layer is copied to the
 * first dest layer of correct type (if there is none, the layer is skipped)
 * return 1 on success, 0 on failure
 */
void CustomData_copy_data(const struct CustomData *source,
                          struct CustomData *dest, int source_index,
                          int dest_index, int count);
void CustomData_copy_data_named(const struct CustomData *source,
                          struct CustomData *dest, int source_index,
                          int dest_index, int count);
void CustomData_copy_elements(int type, void *source, void *dest, int count);
void CustomData_bmesh_copy_data(const struct CustomData *source, 
                                struct CustomData *dest, void *src_block, 
                                void **dest_block);

/* frees data in a CustomData object
 * return 1 on success, 0 on failure
 */
void CustomData_free_elem(struct CustomData *data, int index, int count);

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
 */
void CustomData_interp(const struct CustomData *source, struct CustomData *dest,
                       int *src_indices, float *weights, float *sub_weights,
                       int count, int dest_index);
void CustomData_bmesh_interp_n(struct CustomData *data, void **src_blocks, const float *weights,
                               const float *sub_weights, int count, void *dest_block, int n);
void CustomData_bmesh_interp(struct CustomData *data, void **src_blocks,
                             const float *weights, const float *sub_weights, int count,
                             void *dest_block);


/* swaps the data in the element corners, to new corners with indices as
 * specified in corner_indices. for edges this is an array of length 2, for
 * faces an array of length 4 */
void CustomData_swap(struct CustomData *data, int index, const int *corner_indices);

/* gets a pointer to the data element at index from the first layer of type
 * returns NULL if there is no layer of type
 */
void *CustomData_get(const struct CustomData *data, int index, int type);
void *CustomData_get_n(const struct CustomData *data, int type, int index, int n);
void *CustomData_bmesh_get(const struct CustomData *data, void *block, int type);
void *CustomData_bmesh_get_n(const struct CustomData *data, void *block, int type, int n);

/* gets the layer at physical index n, with no type checking.
 */
void *CustomData_bmesh_get_layer_n(const struct CustomData *data, void *block, int n);

bool CustomData_set_layer_name(const struct CustomData *data, int type, int n, const char *name);
const char *CustomData_get_layer_name(const struct CustomData *data, int type, int n);

/* gets a pointer to the active or first layer of type
 * returns NULL if there is no layer of type
 */
void *CustomData_get_layer(const struct CustomData *data, int type);
void *CustomData_get_layer_n(const struct CustomData *data, int type, int n);
void *CustomData_get_layer_named(const struct CustomData *data, int type, const char *name);
int CustomData_get_offset(const struct CustomData *data, int type);
int CustomData_get_n_offset(const struct CustomData *data, int type, int n);

int CustomData_get_layer_index(const struct CustomData *data, int type);
int CustomData_get_layer_index_n(const struct CustomData *data, int type, int n);
int CustomData_get_named_layer_index(const struct CustomData *data, int type, const char *name);
int CustomData_get_active_layer_index(const struct CustomData *data, int type);
int CustomData_get_render_layer_index(const struct CustomData *data, int type);
int CustomData_get_clone_layer_index(const struct CustomData *data, int type);
int CustomData_get_stencil_layer_index(const struct CustomData *data, int type);
int CustomData_get_named_layer(const struct CustomData *data, int type, const char *name);
int CustomData_get_active_layer(const struct CustomData *data, int type);
int CustomData_get_render_layer(const struct CustomData *data, int type);
int CustomData_get_clone_layer(const struct CustomData *data, int type);
int CustomData_get_stencil_layer(const struct CustomData *data, int type);

/* copies the data from source to the data element at index in the first
 * layer of type
 * no effect if there is no layer of type
 */
void CustomData_set(const struct CustomData *data, int index, int type,
                    void *source);

void CustomData_bmesh_set(const struct CustomData *data, void *block, int type, 
                          void *source);

void CustomData_bmesh_set_n(struct CustomData *data, void *block, int type, int n, 
                            void *source);
/* sets the data of the block at physical layer n.  no real type checking
 * is performed.
 */
void CustomData_bmesh_set_layer_n(struct CustomData *data, void *block, int n,
                                  void *source);

/* set the pointer of to the first layer of type. the old data is not freed.
 * returns the value of ptr if the layer is found, NULL otherwise
 */
void *CustomData_set_layer(const struct CustomData *data, int type, void *ptr);
void *CustomData_set_layer_n(const struct CustomData *data, int type, int n, void *ptr);

/* sets the nth layer of type as active */
void CustomData_set_layer_active(struct CustomData *data, int type, int n);
void CustomData_set_layer_render(struct CustomData *data, int type, int n);
void CustomData_set_layer_clone(struct CustomData *data, int type, int n);
void CustomData_set_layer_stencil(struct CustomData *data, int type, int n);

/* same as above but works with an index from CustomData_get_layer_index */
void CustomData_set_layer_active_index(struct CustomData *data, int type, int n);
void CustomData_set_layer_render_index(struct CustomData *data, int type, int n);
void CustomData_set_layer_clone_index(struct CustomData *data, int type, int n);
void CustomData_set_layer_stencil_index(struct CustomData *data, int type, int n);

/* adds flag to the layer flags */
void CustomData_set_layer_flag(struct CustomData *data, int type, int flag);

void CustomData_bmesh_set_default(struct CustomData *data, void **block);
void CustomData_bmesh_free_block(struct CustomData *data, void **block);
void CustomData_bmesh_free_block_data(struct CustomData *data, void *block);

/* copy custom data to/from layers as in mesh/derivedmesh, to editmesh
 * blocks of data. the CustomData's must not be compatible */
void CustomData_to_bmesh_block(const struct CustomData *source, 
                               struct CustomData *dest, int src_index, void **dest_block, bool use_default_init);
void CustomData_from_bmesh_block(const struct CustomData *source, 
                                 struct CustomData *dest, void *src_block, int dest_index);


/* query info over types */
void CustomData_file_write_info(int type, const char **structname, int *structnum);
int CustomData_sizeof(int type);

/* get the name of a layer type */
const char *CustomData_layertype_name(int type);
bool        CustomData_layertype_is_singleton(int type);
int         CustomData_layertype_layers_max(const int type);

/* make sure the name of layer at index is unique */
void CustomData_set_layer_unique_name(struct CustomData *data, int index);

void CustomData_validate_layer_name(const struct CustomData *data, int type, const char *name, char *outname);

/* for file reading compatibility, returns false if the layer was freed,
 * only after this test passes, layer->data should be assigned */
bool CustomData_verify_versions(struct CustomData *data, int index);

/*BMesh specific customdata stuff*/
void CustomData_to_bmeshpoly(struct CustomData *fdata, struct CustomData *pdata,
                             struct CustomData *ldata, int totloop, int totpoly);
void CustomData_from_bmeshpoly(struct CustomData *fdata, struct CustomData *pdata, struct CustomData *ldata, int total);
void CustomData_bmesh_update_active_layers(struct CustomData *fdata, struct CustomData *pdata, struct CustomData *ldata);
void CustomData_bmesh_do_versions_update_active_layers(struct CustomData *fdata, struct CustomData *pdata, struct CustomData *ldata);
void CustomData_bmesh_init_pool(struct CustomData *data, int totelem, const char htype);

/* External file storage */

void CustomData_external_add(struct CustomData *data,
                             struct ID *id, int type, int totelem, const char *filename);
void CustomData_external_remove(struct CustomData *data,
                                struct ID *id, int type, int totelem);
bool CustomData_external_test(struct CustomData *data, int type);

void CustomData_external_write(struct CustomData *data,
                               struct ID *id, CustomDataMask mask, int totelem, int free);
void CustomData_external_read(struct CustomData *data,
                              struct ID *id, CustomDataMask mask, int totelem);
void CustomData_external_reload(struct CustomData *data,
                                struct ID *id, CustomDataMask mask, int totelem);

/* Mesh-to-mesh transfer data. */

struct MeshPairRemap;
typedef struct CustomDataTransferLayerMap CustomDataTransferLayerMap;

typedef void (*cd_datatransfer_interp)(
        const CustomDataTransferLayerMap *laymap, void *dest,
        void **sources, const float *weights, const int count, const float mix_factor);

/**
 * Fake CD_LAYERS (those are actually 'real' data stored directly into elements' structs, or otherwise not (directly)
 * accessible to usual CDLayer system). */
enum {
	CD_FAKE             = 1 << 8,

	/* Vertices. */
	CD_FAKE_MDEFORMVERT = CD_FAKE | CD_MDEFORMVERT,  /* *sigh* due to how vgroups are stored :( . */
	CD_FAKE_SHAPEKEY    = CD_FAKE | CD_SHAPEKEY,  /* Not available as real CD layer in non-bmesh context. */

	/* Edges. */
	CD_FAKE_SEAM        = CD_FAKE | 100,  /* UV seam flag for edges. */
	CD_FAKE_CREASE      = CD_FAKE | CD_CREASE,  /* *sigh*. */

	/* Multiple types of mesh elements... */
	CD_FAKE_BWEIGHT     = CD_FAKE | CD_BWEIGHT,  /* *sigh*. */
	CD_FAKE_UV          = CD_FAKE | CD_MLOOPUV,  /* UV flag, because we handle both loop's UVs and poly's textures. */

	CD_FAKE_SHARP       = CD_FAKE | 200,  /* Sharp flag for edges, smooth flag for faces. */
};

enum {
	ME_VERT = 1 << 0,
	ME_EDGE = 1 << 1,
	ME_POLY = 1 << 2,
	ME_LOOP = 1 << 3,
};

/**
 * How to filter out some elements (to leave untouched).
 * Note those options are highly dependent on type of transferred data! */
enum {
	CDT_MIX_NOMIX                   = -1,  /* Special case, only used because we abuse 'copy' CD callback. */
	CDT_MIX_TRANSFER                = 0,
	CDT_MIX_REPLACE_ABOVE_THRESHOLD = 1,
	CDT_MIX_REPLACE_BELOW_THRESHOLD = 2,
	CDT_MIX_MIX                     = 16,
	CDT_MIX_ADD                     = 17,
	CDT_MIX_SUB                     = 18,
	CDT_MIX_MUL                     = 19,
	/* etc. etc. */
};

typedef struct CustomDataTransferLayerMap {
	CustomDataTransferLayerMap *next, *prev;

	int data_type;
	int mix_mode;
	float mix_factor;
	const float *mix_weights;  /* If non-NULL, array of weights, one for each dest item, replaces mix_factor. */

	void *data_src;      /* Data source array (can be regular CD data, vertices/edges/etc., keyblocks...). */
	void *data_dst;      /* Data dest array (same type as dat_src). */
	int   data_src_n;    /* Index to affect in data_src (used e.g. for vgroups). */
	int   data_dst_n;    /* Index to affect in data_dst (used e.g. for vgroups). */
	size_t elem_size;    /* Size of one element of data_src/data_dst. */

	size_t data_size;    /* Size of actual data we transfer. */
	size_t data_offset;  /* Offset of actual data we transfer (in element contained in data_src/dst). */
	uint64_t data_flag;  /* For bitflag transfer, flag(s) to affect in transfered data. */

	cd_datatransfer_interp interp;
} CustomDataTransferLayerMap;

/* Those functions assume src_n and dst_n layers of given type exist in resp. src and dst. */
void CustomData_data_transfer(const struct MeshPairRemap *me_remap, const CustomDataTransferLayerMap *laymap);

#ifdef __cplusplus
}
#endif

#endif

