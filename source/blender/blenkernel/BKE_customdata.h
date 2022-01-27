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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 * \brief CustomData interface, see also DNA_customdata_types.h.
 */

#pragma once

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AnonymousAttributeID;
struct BMesh;
struct BlendDataReader;
struct BlendWriter;
struct CustomData;
struct CustomData_MeshMasks;
struct ID;
typedef uint64_t CustomDataMask;

/* A data type large enough to hold 1 element from any custom-data layer type. */
typedef struct {
  unsigned char data[64];
} CDBlockBytes;

extern const CustomData_MeshMasks CD_MASK_BAREMESH;
extern const CustomData_MeshMasks CD_MASK_BAREMESH_ORIGINDEX;
extern const CustomData_MeshMasks CD_MASK_MESH;
extern const CustomData_MeshMasks CD_MASK_EDITMESH;
extern const CustomData_MeshMasks CD_MASK_DERIVEDMESH;
extern const CustomData_MeshMasks CD_MASK_BMESH;
extern const CustomData_MeshMasks CD_MASK_FACECORNERS;
extern const CustomData_MeshMasks CD_MASK_EVERYTHING;

/* for ORIGINDEX layer type, indicates no original index for this element */
#define ORIGINDEX_NONE -1

/* initializes a CustomData object with the same layer setup as source and
 * memory space for totelem elements. mask must be an array of length
 * CD_NUMTYPES elements, that indicate if a layer can be copied. */

/** Add/copy/merge allocation types. */
typedef enum eCDAllocType {
  /** Use the data pointer. */
  CD_ASSIGN = 0,
  /** Allocate blank memory. */
  CD_CALLOC = 1,
  /** Allocate and set to default. */
  CD_DEFAULT = 2,
  /** Use data pointers, set layer flag NOFREE. */
  CD_REFERENCE = 3,
  /** Do a full copy of all layers, only allowed if source has same number of elements. */
  CD_DUPLICATE = 4,
} eCDAllocType;

#define CD_TYPE_AS_MASK(_type) (CustomDataMask)((CustomDataMask)1 << (CustomDataMask)(_type))

void customData_mask_layers__print(const struct CustomData_MeshMasks *mask);

typedef void (*cd_interp)(
    const void **sources, const float *weights, const float *sub_weights, int count, void *dest);
typedef void (*cd_copy)(const void *source, void *dest, int count);
typedef bool (*cd_validate)(void *item, uint totitems, bool do_fixes);

/**
 * Update mask_dst with layers defined in mask_src (equivalent to a bit-wise OR).
 */
void CustomData_MeshMasks_update(CustomData_MeshMasks *mask_dst,
                                 const CustomData_MeshMasks *mask_src);
/**
 * Return True if all layers set in \a mask_required are also set in \a mask_ref
 */
bool CustomData_MeshMasks_are_matching(const CustomData_MeshMasks *mask_ref,
                                       const CustomData_MeshMasks *mask_required);

/**
 * Checks if the layer at physical offset \a layer_n (in data->layers) support math
 * the below operations.
 */
bool CustomData_layer_has_math(const struct CustomData *data, int layer_n);
bool CustomData_layer_has_interp(const struct CustomData *data, int layer_n);

/**
 * Checks if any of the custom-data layers has math.
 */
bool CustomData_has_math(const struct CustomData *data);
bool CustomData_has_interp(const struct CustomData *data);
/**
 * A non bmesh version would have to check `layer->data`.
 */
bool CustomData_bmesh_has_free(const struct CustomData *data);

/**
 * Checks if any of the custom-data layers is referenced.
 */
bool CustomData_has_referenced(const struct CustomData *data);

/**
 * Copies the "value" (e.g. mloopuv uv or mloopcol colors) from one block to
 * another, while not overwriting anything else (e.g. flags).  probably only
 * implemented for mloopuv/mloopcol, for now.
 */
void CustomData_data_copy_value(int type, const void *source, void *dest);

/**
 * Mixes the "value" (e.g. mloopuv uv or mloopcol colors) from one block into
 * another, while not overwriting anything else (e.g. flags).
 */
void CustomData_data_mix_value(
    int type, const void *source, void *dest, int mixmode, float mixfactor);

/**
 * Compares if data1 is equal to data2.  type is a valid CustomData type
 * enum (e.g. #CD_MLOOPUV). the layer type's equal function is used to compare
 * the data, if it exists, otherwise #memcmp is used.
 */
bool CustomData_data_equals(int type, const void *data1, const void *data2);
void CustomData_data_initminmax(int type, void *min, void *max);
void CustomData_data_dominmax(int type, const void *data, void *min, void *max);
void CustomData_data_multiply(int type, void *data, float fac);
void CustomData_data_add(int type, void *data1, const void *data2);

/**
 * Initializes a CustomData object with the same layer setup as source.
 * mask is a bitfield where `(mask & (1 << (layer type)))` indicates
 * if a layer should be copied or not. alloctype must be one of the above.
 */
void CustomData_copy(const struct CustomData *source,
                     struct CustomData *dest,
                     CustomDataMask mask,
                     eCDAllocType alloctype,
                     int totelem);

/* BMESH_TODO, not really a public function but readfile.c needs it */
void CustomData_update_typemap(struct CustomData *data);

/**
 * Same as the above, except that this will preserve existing layers, and only
 * add the layers that were not there yet.
 */
bool CustomData_merge(const struct CustomData *source,
                      struct CustomData *dest,
                      CustomDataMask mask,
                      eCDAllocType alloctype,
                      int totelem);

/**
 * Reallocate custom data to a new element count.
 * Only affects on data layers which are owned by the CustomData itself,
 * referenced data is kept unchanged,
 *
 * \note Take care of referenced layers by yourself!
 */
void CustomData_realloc(struct CustomData *data, int totelem);

/**
 * BMesh version of CustomData_merge; merges the layouts of source and `dest`,
 * then goes through the mesh and makes sure all the custom-data blocks are
 * consistent with the new layout.
 */
bool CustomData_bmesh_merge(const struct CustomData *source,
                            struct CustomData *dest,
                            CustomDataMask mask,
                            eCDAllocType alloctype,
                            struct BMesh *bm,
                            char htype);

/**
 * NULL's all members and resets the #CustomData.typemap.
 */
void CustomData_reset(struct CustomData *data);

/**
 * Frees data associated with a CustomData object (doesn't free the object itself, though).
 */
void CustomData_free(struct CustomData *data, int totelem);

/**
 * Same as above, but only frees layers which matches the given mask.
 */
void CustomData_free_typemask(struct CustomData *data, int totelem, CustomDataMask mask);

/**
 * Frees all layers with #CD_FLAG_TEMPORARY.
 */
void CustomData_free_temporary(struct CustomData *data, int totelem);

/**
 * Adds a data layer of the given type to the #CustomData object, optionally
 * backed by an external data array. the different allocation types are
 * defined above. returns the data of the layer.
 */
void *CustomData_add_layer(
    struct CustomData *data, int type, eCDAllocType alloctype, void *layer, int totelem);
/**
 * Same as above but accepts a name.
 */
void *CustomData_add_layer_named(struct CustomData *data,
                                 int type,
                                 eCDAllocType alloctype,
                                 void *layer,
                                 int totelem,
                                 const char *name);
void *CustomData_add_layer_anonymous(struct CustomData *data,
                                     int type,
                                     eCDAllocType alloctype,
                                     void *layer,
                                     int totelem,
                                     const struct AnonymousAttributeID *anonymous_id);

/**
 * Frees the active or first data layer with the give type.
 * returns 1 on success, 0 if no layer with the given type is found
 *
 * In edit-mode, use #EDBM_data_layer_free instead of this function.
 */
bool CustomData_free_layer(struct CustomData *data, int type, int totelem, int index);

/**
 * Frees the layer index with the give type.
 * returns 1 on success, 0 if no layer with the given type is found.
 *
 * In edit-mode, use #EDBM_data_layer_free instead of this function.
 */
bool CustomData_free_layer_active(struct CustomData *data, int type, int totelem);

/**
 * Same as above, but free all layers with type.
 */
void CustomData_free_layers(struct CustomData *data, int type, int totelem);

/**
 * Returns true if a layer with the specified type exists.
 */
bool CustomData_has_layer(const struct CustomData *data, int type);

/**
 * Returns the number of layers with this type.
 */
int CustomData_number_of_layers(const struct CustomData *data, int type);
int CustomData_number_of_layers_typemask(const struct CustomData *data, CustomDataMask mask);

/**
 * Duplicate data of a layer with flag NOFREE, and remove that flag.
 * \return the layer data.
 */
void *CustomData_duplicate_referenced_layer(struct CustomData *data, int type, int totelem);
void *CustomData_duplicate_referenced_layer_n(struct CustomData *data,
                                              int type,
                                              int n,
                                              int totelem);
void *CustomData_duplicate_referenced_layer_named(struct CustomData *data,
                                                  int type,
                                                  const char *name,
                                                  int totelem);
void *CustomData_duplicate_referenced_layer_anonymous(
    CustomData *data, int type, const struct AnonymousAttributeID *anonymous_id, int totelem);
bool CustomData_is_referenced_layer(struct CustomData *data, int type);

/**
 * Duplicate all the layers with flag NOFREE, and remove the flag from duplicated layers.
 */
void CustomData_duplicate_referenced_layers(CustomData *data, int totelem);

/**
 * Set the #CD_FLAG_NOCOPY flag in custom data layers where the mask is
 * zero for the layer type, so only layer types specified by the mask will be copied
 */
void CustomData_set_only_copy(const struct CustomData *data, CustomDataMask mask);

/**
 * Copies data from one CustomData object to another
 * objects need not be compatible, each source layer is copied to the
 * first dest layer of correct type (if there is none, the layer is skipped).
 */
void CustomData_copy_data(const struct CustomData *source,
                          struct CustomData *dest,
                          int source_index,
                          int dest_index,
                          int count);
void CustomData_copy_data_layer(const CustomData *source,
                                CustomData *dest,
                                int src_layer_index,
                                int dst_layer_index,
                                int src_index,
                                int dst_index,
                                int count);
void CustomData_copy_data_named(const struct CustomData *source,
                                struct CustomData *dest,
                                int source_index,
                                int dest_index,
                                int count);
void CustomData_copy_elements(int type, void *src_data_ofs, void *dst_data_ofs, int count);
void CustomData_bmesh_copy_data(const struct CustomData *source,
                                struct CustomData *dest,
                                void *src_block,
                                void **dest_block);
void CustomData_bmesh_copy_data_exclude_by_type(const struct CustomData *source,
                                                struct CustomData *dest,
                                                void *src_block,
                                                void **dest_block,
                                                CustomDataMask mask_exclude);

/**
 * Copies data of a single layer of a given type.
 */
void CustomData_copy_layer_type_data(const struct CustomData *source,
                                     struct CustomData *destination,
                                     int type,
                                     int source_index,
                                     int destination_index,
                                     int count);

/**
 * Frees data in a #CustomData object.
 */
void CustomData_free_elem(struct CustomData *data, int index, int count);

/**
 * Interpolate given custom data source items into a single destination one.
 *
 * \param src_indices: Indices of every source items to interpolate into the destination one.
 * \param weights: The weight to apply to each source value individually. If NULL, they will be
 * averaged.
 * \param sub_weights: The weights of sub-items, only used to affect each corners of a
 * tessellated face data (should always be and array of four values).
 * \param count: The number of source items to interpolate.
 * \param dest_index: Index of the destination item, in which to put the result of the
 * interpolation.
 */
void CustomData_interp(const struct CustomData *source,
                       struct CustomData *dest,
                       const int *src_indices,
                       const float *weights,
                       const float *sub_weights,
                       int count,
                       int dest_index);
/**
 * \note src_blocks_ofs & dst_block_ofs
 * must be pointers to the data, offset by layer->offset already.
 */
void CustomData_bmesh_interp_n(struct CustomData *data,
                               const void **src_blocks,
                               const float *weights,
                               const float *sub_weights,
                               int count,
                               void *dst_block_ofs,
                               int n);
void CustomData_bmesh_interp(struct CustomData *data,
                             const void **src_blocks,
                             const float *weights,
                             const float *sub_weights,
                             int count,
                             void *dst_block);

/**
 * Swap data inside each item, for all layers.
 * This only applies to item types that may store several sub-item data
 * (e.g. corner data [UVs, VCol, ...] of tessellated faces).
 *
 * \param corner_indices: A mapping 'new_index -> old_index' of sub-item data.
 */
void CustomData_swap_corners(struct CustomData *data, int index, const int *corner_indices);

/**
 * Swap two items of given custom data, in all available layers.
 */
void CustomData_swap(struct CustomData *data, int index_a, int index_b);

/**
 * Gets a pointer to the data element at index from the first layer of type.
 * \return NULL if there is no layer of type.
 */
void *CustomData_get(const struct CustomData *data, int index, int type);
void *CustomData_get_n(const struct CustomData *data, int type, int index, int n);

/* BMesh Custom Data Functions.
 * Should replace edit-mesh ones with these as well, due to more efficient memory alloc. */

void *CustomData_bmesh_get(const struct CustomData *data, void *block, int type);
void *CustomData_bmesh_get_n(const struct CustomData *data, void *block, int type, int n);

/**
 * Gets from the layer at physical index `n`,
 * \note doesn't check type.
 */
void *CustomData_bmesh_get_layer_n(const struct CustomData *data, void *block, int n);

bool CustomData_set_layer_name(const struct CustomData *data, int type, int n, const char *name);
const char *CustomData_get_layer_name(const struct CustomData *data, int type, int n);

/**
 * Gets a pointer to the active or first layer of type.
 * \return NULL if there is no layer of type.
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

/**
 * Returns name of the active layer of the given type or NULL
 * if no such active layer is defined.
 */
const char *CustomData_get_active_layer_name(const struct CustomData *data, int type);

/**
 * Copies the data from source to the data element at index in the first layer of type
 * no effect if there is no layer of type.
 */
void CustomData_set(const struct CustomData *data, int index, int type, const void *source);

void CustomData_bmesh_set(const struct CustomData *data,
                          void *block,
                          int type,
                          const void *source);

void CustomData_bmesh_set_n(
    struct CustomData *data, void *block, int type, int n, const void *source);
/**
 * Sets the data of the block at physical layer n.
 * no real type checking is performed.
 */
void CustomData_bmesh_set_layer_n(struct CustomData *data, void *block, int n, const void *source);

/**
 * Set the pointer of to the first layer of type. the old data is not freed.
 * returns the value of `ptr` if the layer is found, NULL otherwise.
 */
void *CustomData_set_layer(const struct CustomData *data, int type, void *ptr);
void *CustomData_set_layer_n(const struct CustomData *data, int type, int n, void *ptr);

/**
 * Sets the nth layer of type as active.
 */
void CustomData_set_layer_active(struct CustomData *data, int type, int n);
void CustomData_set_layer_render(struct CustomData *data, int type, int n);
void CustomData_set_layer_clone(struct CustomData *data, int type, int n);
void CustomData_set_layer_stencil(struct CustomData *data, int type, int n);

/**
 * For using with an index from #CustomData_get_active_layer_index and
 * #CustomData_get_render_layer_index.
 */
void CustomData_set_layer_active_index(struct CustomData *data, int type, int n);
void CustomData_set_layer_render_index(struct CustomData *data, int type, int n);
void CustomData_set_layer_clone_index(struct CustomData *data, int type, int n);
void CustomData_set_layer_stencil_index(struct CustomData *data, int type, int n);

/**
 * Adds flag to the layer flags.
 */
void CustomData_set_layer_flag(struct CustomData *data, int type, int flag);
void CustomData_clear_layer_flag(struct CustomData *data, int type, int flag);

void CustomData_bmesh_set_default(struct CustomData *data, void **block);
void CustomData_bmesh_free_block(struct CustomData *data, void **block);
/**
 * Same as #CustomData_bmesh_free_block but zero the memory rather than freeing.
 */
void CustomData_bmesh_free_block_data(struct CustomData *data, void *block);
/**
 * A selective version of #CustomData_bmesh_free_block_data.
 */
void CustomData_bmesh_free_block_data_exclude_by_type(struct CustomData *data,
                                                      void *block,
                                                      CustomDataMask mask_exclude);

/**
 * Copy custom data to/from layers as in mesh/derived-mesh, to edit-mesh
 * blocks of data. the CustomData's must not be compatible.
 *
 * \param use_default_init: initializes data which can't be copied,
 * typically you'll want to use this if the BM_xxx create function
 * is called with BM_CREATE_SKIP_CD flag
 */
void CustomData_to_bmesh_block(const struct CustomData *source,
                               struct CustomData *dest,
                               int src_index,
                               void **dest_block,
                               bool use_default_init);
void CustomData_from_bmesh_block(const struct CustomData *source,
                                 struct CustomData *dest,
                                 void *src_block,
                                 int dest_index);

/**
 * Query info over types.
 */
void CustomData_file_write_info(int type, const char **r_struct_name, int *r_struct_num);
int CustomData_sizeof(int type);

/**
 * Get the name of a layer type.
 */
const char *CustomData_layertype_name(int type);
/**
 * Can only ever be one of these.
 */
bool CustomData_layertype_is_singleton(int type);
/**
 * Has dynamically allocated members.
 * This is useful to know if operations such as #memcmp are
 * valid when comparing data from two layers.
 */
bool CustomData_layertype_is_dynamic(int type);
/**
 * \return Maximum number of layers of given \a type, -1 means 'no limit'.
 */
int CustomData_layertype_layers_max(int type);

/**
 * Make sure the name of layer at index is unique.
 */
void CustomData_set_layer_unique_name(struct CustomData *data, int index);

void CustomData_validate_layer_name(const struct CustomData *data,
                                    int type,
                                    const char *name,
                                    char *outname);

/**
 * For file reading compatibility, returns false if the layer was freed,
 * only after this test passes, `layer->data` should be assigned.
 */
bool CustomData_verify_versions(struct CustomData *data, int index);

/* BMesh specific custom-data stuff.
 *
 * Needed to convert to/from different face representation (for versioning). */

void CustomData_to_bmeshpoly(struct CustomData *fdata, struct CustomData *ldata, int totloop);
void CustomData_from_bmeshpoly(struct CustomData *fdata, struct CustomData *ldata, int total);
void CustomData_bmesh_update_active_layers(struct CustomData *fdata, struct CustomData *ldata);
/**
 * Update active indices for active/render/clone/stencil custom data layers
 * based on indices from fdata layers
 * used by do_versions in `readfile.c` when creating pdata and ldata for pre-bmesh
 * meshes and needed to preserve active/render/clone/stencil flags set in pre-bmesh files.
 */
void CustomData_bmesh_do_versions_update_active_layers(struct CustomData *fdata,
                                                       struct CustomData *ldata);
void CustomData_bmesh_init_pool(struct CustomData *data, int totelem, char htype);

#ifndef NDEBUG
/**
 * Debug check, used to assert when we expect layers to be in/out of sync.
 *
 * \param fallback: Use when there are no layers to handle,
 * since callers may expect success or failure.
 */
bool CustomData_from_bmeshpoly_test(CustomData *fdata, CustomData *ldata, bool fallback);
#endif

/**
 * Validate and fix data of \a layer,
 * if possible (needs relevant callback in layer's type to be defined).
 *
 * \return True if some errors were found.
 */
bool CustomData_layer_validate(struct CustomDataLayer *layer, uint totitems, bool do_fixes);
void CustomData_layers__print(struct CustomData *data);

/* External file storage */

void CustomData_external_add(
    struct CustomData *data, struct ID *id, int type, int totelem, const char *filename);
void CustomData_external_remove(struct CustomData *data, struct ID *id, int type, int totelem);
bool CustomData_external_test(struct CustomData *data, int type);

void CustomData_external_write(
    struct CustomData *data, struct ID *id, CustomDataMask mask, int totelem, int free);
void CustomData_external_read(struct CustomData *data,
                              struct ID *id,
                              CustomDataMask mask,
                              int totelem);
void CustomData_external_reload(struct CustomData *data,
                                struct ID *id,
                                CustomDataMask mask,
                                int totelem);

/* Mesh-to-mesh transfer data. */

struct CustomDataTransferLayerMap;
struct MeshPairRemap;

typedef void (*cd_datatransfer_interp)(const struct CustomDataTransferLayerMap *laymap,
                                       void *dest,
                                       const void **sources,
                                       const float *weights,
                                       int count,
                                       float mix_factor);

/**
 * Fake CD_LAYERS (those are actually 'real' data stored directly into elements' structs,
 * or otherwise not (directly) accessible to usual CDLayer system). */
enum {
  CD_FAKE = 1 << 8,

  /* Vertices. */
  CD_FAKE_MDEFORMVERT = CD_FAKE | CD_MDEFORMVERT, /* *sigh* due to how vgroups are stored :(. */
  CD_FAKE_SHAPEKEY = CD_FAKE |
                     CD_SHAPEKEY, /* Not available as real CD layer in non-bmesh context. */

  /* Edges. */
  CD_FAKE_SEAM = CD_FAKE | 100,         /* UV seam flag for edges. */
  CD_FAKE_CREASE = CD_FAKE | CD_CREASE, /* *sigh*. */

  /* Multiple types of mesh elements... */
  CD_FAKE_BWEIGHT = CD_FAKE | CD_BWEIGHT, /* *sigh*. */
  CD_FAKE_UV = CD_FAKE |
               CD_MLOOPUV, /* UV flag, because we handle both loop's UVs and poly's textures. */

  CD_FAKE_LNOR = CD_FAKE |
                 CD_CUSTOMLOOPNORMAL, /* Because we play with clnor and temp lnor layers here. */

  CD_FAKE_SHARP = CD_FAKE | 200, /* Sharp flag for edges, smooth flag for faces. */
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
  CDT_MIX_NOMIX = -1, /* Special case, only used because we abuse 'copy' CD callback. */
  CDT_MIX_TRANSFER = 0,
  CDT_MIX_REPLACE_ABOVE_THRESHOLD = 1,
  CDT_MIX_REPLACE_BELOW_THRESHOLD = 2,
  CDT_MIX_MIX = 16,
  CDT_MIX_ADD = 17,
  CDT_MIX_SUB = 18,
  CDT_MIX_MUL = 19,
  /* Etc. */
};

typedef struct CustomDataTransferLayerMap {
  struct CustomDataTransferLayerMap *next, *prev;

  int data_type;
  int mix_mode;
  float mix_factor;
  /** If non-NULL, array of weights, one for each dest item, replaces mix_factor. */
  const float *mix_weights;

  /** Data source array (can be regular CD data, vertices/edges/etc., keyblocks...). */
  const void *data_src;
  /** Data dest array (same type as dat_src). */
  void *data_dst;
  /** Index to affect in data_src (used e.g. for vgroups). */
  int data_src_n;
  /** Index to affect in data_dst (used e.g. for vgroups). */
  int data_dst_n;
  /** Size of one element of data_src/data_dst. */
  size_t elem_size;

  /** Size of actual data we transfer. */
  size_t data_size;
  /** Offset of actual data we transfer (in element contained in data_src/dst). */
  size_t data_offset;
  /** For bitflag transfer, flag(s) to affect in transferred data. */
  uint64_t data_flag;

  /** Opaque pointer, to be used by specific interp callback (e.g. transformspace for normals). */
  void *interp_data;

  cd_datatransfer_interp interp;
} CustomDataTransferLayerMap;

/**
 * Those functions assume src_n and dst_n layers of given type exist in resp. src and dst.
 */
void CustomData_data_transfer(const struct MeshPairRemap *me_remap,
                              const CustomDataTransferLayerMap *laymap);

/* .blend file I/O */

/**
 * Prepare given custom data for file writing.
 *
 * \param data: the custom-data to tweak for .blend file writing (modified in place).
 * \param r_write_layers: contains a reduced set of layers to be written to file,
 * use it with #writestruct_at_address()
 * (caller must free it if != \a write_layers_buff).
 *
 * \param write_layers_buff: An optional buffer for r_write_layers (to avoid allocating it).
 * \param write_layers_size: The size of pre-allocated \a write_layer_buff.
 *
 * \warning After this funcion has ran, given custom data is no more valid from Blender POV
 * (its `totlayer` is invalid). This function shall always be called with localized data
 * (as it is in write_meshes()).
 *
 * \note `data->typemap` is not updated here, since it is always rebuilt on file read anyway.
 * This means written `typemap` does not match written layers (as returned by \a r_write_layers).
 * Trivial to fix is ever needed.
 */
void CustomData_blend_write_prepare(struct CustomData *data,
                                    struct CustomDataLayer **r_write_layers,
                                    struct CustomDataLayer *write_layers_buff,
                                    size_t write_layers_size);

/**
 * \param layers: The layers argument assigned by #CustomData_blend_write_prepare.
 */
void CustomData_blend_write(struct BlendWriter *writer,
                            struct CustomData *data,
                            CustomDataLayer *layers,
                            int count,
                            CustomDataMask cddata_mask,
                            struct ID *id);
void CustomData_blend_read(struct BlendDataReader *reader, struct CustomData *data, int count);

#ifndef NDEBUG
struct DynStr;
/** Use to inspect mesh data when debugging. */
void CustomData_debug_info_from_layers(const struct CustomData *data,
                                       const char *indent,
                                       struct DynStr *dynstr);
#endif /* NDEBUG */

#ifdef __cplusplus
}
#endif
