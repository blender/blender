/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * \brief CustomData interface, see also DNA_customdata_types.h.
 */

#pragma once

#include <optional>

#include "BLI_generic_virtual_array.hh"
#include "BLI_implicit_sharing.h"
#include "BLI_memory_counter_fwd.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_sys_types.h"
#include "BLI_vector.hh"

#include "BKE_attribute_storage.hh"
#include "BKE_volume_enums.hh"

#include "DNA_customdata_types.h"

struct BlendDataReader;
struct BlendWriter;
struct BMesh;
struct CustomData_MeshMasks;
struct CustomData;
struct CustomDataTransferLayerMap;
struct ID;
struct MeshPairRemap;

namespace blender::bke {
enum class AttrDomain : int8_t;
}

/**
 * These names are used as prefixes for UV layer names to find the associated boolean
 * layers. They should never be longer than 2 chars, as #MAX_CUSTOMDATA_LAYER_NAME
 * has 4 extra bytes above what can be used for the base layer name, and these
 * prefixes are placed between 2 '.'s at the start of the layer name.
 * For example The uv vert selection layer of a layer named `UVMap.001`
 * will be called `.pn.UVMap.001`.
 */
#define UV_PINNED_NAME "pn"

/**
 * UV map related customdata offsets into BMesh attribute blocks. See #BM_uv_map_offsets_get.
 * Defined in #BKE_customdata.hh to avoid including bmesh.hh in many unrelated areas.
 * An offset of -1 means that the corresponding layer does not exist.
 */
struct BMUVOffsets {
  int uv;
  int pin;
};

/** All values reference none layers. */
#define BMUVOFFSETS_NONE {-1, -1}

/** A data type large enough to hold 1 element from any custom-data layer type. */
struct CDBlockBytes {
  unsigned char data[64];
};

extern const CustomData_MeshMasks CD_MASK_BAREMESH;
extern const CustomData_MeshMasks CD_MASK_BAREMESH_ORIGINDEX;
extern const CustomData_MeshMasks CD_MASK_MESH;
extern const CustomData_MeshMasks CD_MASK_DERIVEDMESH;
extern const CustomData_MeshMasks CD_MASK_BMESH;
extern const CustomData_MeshMasks CD_MASK_EVERYTHING;

/** For ORIGINDEX layer type, indicates no original index for this element. */
#define ORIGINDEX_NONE -1

/* Initializes a CustomData object with the same layer setup as source and
 * memory space for totelem elements. mask must be an array of length
 * CD_NUMTYPES elements, that indicate if a layer can be copied. */

/** Add/copy/merge allocation types. */
enum eCDAllocType {
  /** Allocate and set to default, which is usually just zeroed memory. */
  CD_SET_DEFAULT = 2,
  /**
   * Default construct new layer values. Does nothing for trivial types. This should be used
   * if all layer values will be set by the caller after creating the layer.
   */
  CD_CONSTRUCT = 5,
};

inline eCustomDataMask CD_TYPE_AS_MASK(eCustomDataType type)
{
  return eCustomDataMask(1) << eCustomDataMask(type);
}

void customData_mask_layers__print(const CustomData_MeshMasks *mask);

using cd_interp = void (*)(const void **sources, const float *weights, int count, void *dest);
using cd_copy = void (*)(const void *source, void *dest, int count);
using cd_set_default_value = void (*)(void *data, int count);
using cd_free = void (*)(void *data, int count);
using cd_validate = bool (*)(void *item, uint totitems, bool do_fixes);

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
bool CustomData_layer_has_math(const CustomData *data, int layer_n);
bool CustomData_layer_has_interp(const CustomData *data, int layer_n);

/**
 * Checks if any of the custom-data layers has math.
 */
bool CustomData_has_math(const CustomData *data);
bool CustomData_has_interp(const CustomData *data);
/**
 * A non bmesh version would have to check `layer->data`.
 */
bool CustomData_bmesh_has_free(const CustomData *data);

/**
 * Copies the "value" (e.g. `uv_map` UV or `mloopcol` colors) from one block to
 * another, while not overwriting anything else (e.g. flags).  probably only
 * implemented for `uv_map/mloopcol`, for now.
 */
void CustomData_data_copy_value(eCustomDataType type, const void *source, void *dest);
void CustomData_data_set_default_value(eCustomDataType type, void *elem);

/**
 * Mixes the "value" (e.g. `uv_map` UV or `mloopcol` colors) from one block into
 * another, while not overwriting anything else (e.g. flags).
 */
void CustomData_data_mix_value(
    eCustomDataType type, const void *source, void *dest, int mixmode, float mixfactor);

/**
 * Compares if data1 is equal to data2.  type is a valid #CustomData type
 * enum (e.g. #CD_PROP_FLOAT). the layer type's equal function is used to compare
 * the data, if it exists, otherwise #memcmp is used.
 */
bool CustomData_data_equals(eCustomDataType type, const void *data1, const void *data2);
void CustomData_data_initminmax(eCustomDataType type, void *min, void *max);
void CustomData_data_dominmax(eCustomDataType type, const void *data, void *min, void *max);
void CustomData_data_multiply(eCustomDataType type, void *data, float fac);
void CustomData_data_add(eCustomDataType type, void *data1, const void *data2);

/**
 * Initializes a CustomData object with the same layer setup as source. `mask` is a bit-field where
 * `(mask & (1 << (layer type)))` indicates if a layer should be copied or not. Data layers using
 * implicit-sharing will not actually be copied but will be shared between source and destination.
 *
 * \warning Does not free or release any internal resources in `dest` CustomData, code must call
 * #CustomData_free first if needed.
 */
void CustomData_init_from(const CustomData *source,
                          CustomData *dest,
                          eCustomDataMask mask,
                          int totelem);
/**
 * Initializes a CustomData object with the same layers as source. The data is not copied from the
 * source. Instead, the new layers are initialized using the given `alloctype`.
 *
 * \warning Does not free or release any internal resources in `dest` CustomData, code must call
 * #CustomData_free first if needed.
 */
void CustomData_init_layout_from(const CustomData *source,
                                 CustomData *dest,
                                 eCustomDataMask mask,
                                 eCDAllocType alloctype,
                                 int totelem);

/**
 * \note Ideally this would not be a public function but versioning needs it.
 */
void CustomData_update_typemap(CustomData *data);

/**
 * Copies all layers from source to destination that don't exist there yet.
 */
bool CustomData_merge(const CustomData *source,
                      CustomData *dest,
                      eCustomDataMask mask,
                      int totelem);
/**
 * Copies all layers from source to destination that don't exist there yet. The layer data is not
 * copied. Instead the newly created layers are initialized using the given `alloctype`.
 */
bool CustomData_merge_layout(const CustomData *source,
                             CustomData *dest,
                             eCustomDataMask mask,
                             eCDAllocType alloctype,
                             int totelem);

/**
 * Reallocate custom data to a new element count. If the new size is larger, the new values use
 * the #CD_CONSTRUCT behavior, so trivial types must be initialized by the caller. After being
 * resized, the #CustomData does not contain any referenced layers.
 */
void CustomData_realloc(CustomData *data,
                        int old_size,
                        int new_size,
                        eCDAllocType alloctype = CD_CONSTRUCT);

/**
 * BMesh version of CustomData_merge_layout; merges the layouts of source and `dest`,
 * then goes through the mesh and makes sure all the custom-data blocks are
 * consistent with the new layout.
 */
bool CustomData_bmesh_merge_layout(const CustomData *source,
                                   CustomData *dest,
                                   eCustomDataMask mask,
                                   eCDAllocType alloctype,
                                   BMesh *bm,
                                   char htype);

/**
 * Remove layers that aren't stored in BMesh or are stored as flags on BMesh.
 * The `layers` array of the returned #CustomData must be freed, but may be null.
 * Used during conversion of #Mesh data to #BMesh storage format.
 */
CustomData CustomData_shallow_copy_remove_non_bmesh_attributes(const CustomData *src,
                                                               eCustomDataMask mask);

/**
 * NULL's all members and resets the #CustomData.typemap.
 *
 * \warning Does not free or release any internal resources.
 */
void CustomData_reset(CustomData *data);

/**
 * Frees data associated with a CustomData object (doesn't free the object itself, though).
 */
void CustomData_free(CustomData *data);

/**
 * Adds a layer of the given type to the #CustomData object. The new layer is initialized based on
 * the given alloctype.
 * \return The layer data.
 */
void *CustomData_add_layer(CustomData *data,
                           eCustomDataType type,
                           eCDAllocType alloctype,
                           int totelem);

/**
 * Adds a layer of the given type to the #CustomData object. The new layer takes ownership of the
 * passed in `layer_data`. If a #ImplicitSharingInfo is passed in, its user count is increased.
 */
const void *CustomData_add_layer_with_data(CustomData *data,
                                           eCustomDataType type,
                                           void *layer_data,
                                           int totelem,
                                           const blender::ImplicitSharingInfo *sharing_info);

/**
 * Same as #CustomData_add_layer but accepts a name.
 */
void *CustomData_add_layer_named(CustomData *data,
                                 eCustomDataType type,
                                 eCDAllocType alloctype,
                                 int totelem,
                                 blender::StringRef name);

const void *CustomData_add_layer_named_with_data(CustomData *data,
                                                 eCustomDataType type,
                                                 void *layer_data,
                                                 int totelem,
                                                 blender::StringRef name,
                                                 const blender::ImplicitSharingInfo *sharing_info);

/**
 * Frees the active or first data layer with the give type.
 * returns 1 on success, 0 if no layer with the given type is found
 *
 * In edit-mode, use #EDBM_data_layer_free instead of this function.
 */
bool CustomData_free_layer(CustomData *data, eCustomDataType type, int index);
bool CustomData_free_layer_named(CustomData *data, blender::StringRef name);

/**
 * Frees the layer index with the give type.
 * returns 1 on success, 0 if no layer with the given type is found.
 *
 * In edit-mode, use #EDBM_data_layer_free instead of this function.
 */
bool CustomData_free_layer_active(CustomData *data, eCustomDataType type);

/**
 * Same as #CustomData_free_layer_active, but free all layers with type.
 */
void CustomData_free_layers(CustomData *data, eCustomDataType type);

/**
 * Returns true if a layer with the specified type exists.
 */
bool CustomData_has_layer(const CustomData *data, eCustomDataType type);
bool CustomData_has_layer_named(const CustomData *data,
                                eCustomDataType type,
                                blender::StringRef name);

/**
 * Returns the number of layers with this type.
 */
int CustomData_number_of_layers(const CustomData *data, eCustomDataType type);
int CustomData_number_of_anonymous_layers(const CustomData *data, eCustomDataType type);
int CustomData_number_of_layers_typemask(const CustomData *data, eCustomDataMask mask);

/**
 * Set the #CD_FLAG_NOCOPY flag in custom data layers where the mask is
 * zero for the layer type, so only layer types specified by the mask will be copied
 */
void CustomData_set_only_copy(const CustomData *data, eCustomDataMask mask);

/**
 * Copies data from one CustomData object to another
 * objects need not be compatible, each source layer is copied to the
 * first dest layer of correct type (if there is none, the layer is skipped).
 *
 * NOTE: It's expected that the destination layers are mutable
 * (#CustomData_ensure_layers_are_mutable). These copy-functions could ensure that internally, but
 * that would cause additional overhead when copying few elements at a time. It would also be
 * necessary to pass the total size of the destination layers as parameter to make them mutable
 * though. In most cases, these functions are used right after creating a new geometry, in which
 * case there are no shared layers anyway.
 */
void CustomData_copy_data(
    const CustomData *source, CustomData *dest, int source_index, int dest_index, int count);
void CustomData_copy_data_layer(const CustomData *source,
                                CustomData *dest,
                                int src_layer_index,
                                int dst_layer_index,
                                int src_index,
                                int dst_index,
                                int count);
void CustomData_copy_elements(eCustomDataType type,
                              const void *src_data,
                              void *dst_data,
                              int count);

/**
 * Copy all layers from the source to the destination block.
 * Allocate the result block if necessary, otherwise free its existing layer data.
 */
void CustomData_bmesh_copy_block(CustomData &data, void *src_block, void **dst_block);

/** Holds the minimal data necessary to copy data blocks from one custom data format to another. */
struct BMCustomDataCopyMap {
  struct TrivialCopy {
    int size;
    int src_offset;
    int dst_offset;
  };
  struct Copy {
    cd_copy fn;
    int src_offset;
    int dst_offset;
  };
  struct TrivialDefault {
    int size;
    int dst_offset;
  };
  struct Default {
    cd_set_default_value fn;
    int dst_offset;
  };
  struct Free {
    cd_free fn;
    int dst_offset;
  };
  blender::Vector<TrivialCopy> trivial_copies;
  blender::Vector<Copy> copies;
  blender::Vector<TrivialDefault> trivial_defaults;
  blender::Vector<Default> defaults;
  blender::Vector<Free> free;
};

/** Precalculate a map for more efficient copying between custom data formats. */
BMCustomDataCopyMap CustomData_bmesh_copy_map_calc(const CustomData &src,
                                                   const CustomData &dst,
                                                   eCustomDataMask mask_exclude = 0);

/**
 * Copy custom data layers for one element between two potentially different formats with a
 * precalculated map.
 */
void CustomData_bmesh_copy_block(CustomData &dst_data,
                                 const BMCustomDataCopyMap &map,
                                 const void *src_block,
                                 void **dst_block);

/**
 * Copies data of a single layer of a given type.
 */
void CustomData_copy_layer_type_data(const CustomData *source,
                                     CustomData *destination,
                                     eCustomDataType type,
                                     int source_index,
                                     int destination_index,
                                     int count);

/**
 * Frees data in a #CustomData object. This is only expected to be called if the data layers are
 * not shared (#CustomData_ensure_layers_are_mutable).
 */
void CustomData_free_elem(CustomData *data, int index, int count);

/**
 * Interpolate given custom data source items into a single destination one.
 *
 * \param src_indices: Indices of every source items to interpolate into the destination one.
 * \param weights: The weight to apply to each source value individually. If NULL, they will be
 * averaged.
 * \param count: The number of source items to interpolate.
 * \param dest_index: Index of the destination item, in which to put the result of the
 * interpolation.
 */
void CustomData_interp(const CustomData *source,
                       CustomData *dest,
                       const int *src_indices,
                       const float *weights,
                       int count,
                       int dest_index);
/**
 * \note src_blocks_ofs & dst_block_ofs
 * must be pointers to the data, offset by layer->offset already.
 */
void CustomData_bmesh_interp_n(CustomData *data,
                               const void **src_blocks,
                               const float *weights,
                               int count,
                               void *dst_block_ofs,
                               int n);
void CustomData_bmesh_interp(
    CustomData *data, const void **src_blocks, const float *weights, int count, void *dst_block);

/**
 * Swap data inside each item, for all layers.
 * This only applies to item types that may store several sub-item data
 * (e.g. corner data [UVs, VCol, ...] of tessellated faces).
 *
 * \param corner_indices: A mapping 'new_index -> old_index' of sub-item data.
 */
void CustomData_swap_corners(CustomData *data, int index, const int *corner_indices);

/**
 * Custom data layers can be shared through implicit sharing (`BLI_implicit_sharing.h`). This
 * function makes sure that the layer is unshared if it was shared, which makes it mutable.
 */
void CustomData_ensure_data_is_mutable(CustomDataLayer *layer, int totelem);
void CustomData_ensure_layers_are_mutable(CustomData *data, int totelem);

/**
 * Retrieve a pointer to an element of the active layer of the given \a type, chosen by the
 * \a index, if it exists.
 */
void *CustomData_get_for_write(CustomData *data, int index, eCustomDataType type, int totelem);
/**
 * Retrieve a pointer to an element of the \a nth layer of the given \a type, chosen by the
 * \a index, if it exists.
 */
void *CustomData_get_n_for_write(
    CustomData *data, eCustomDataType type, int index, int n, int totelem);

/* BMesh Custom Data Functions.
 * Should replace edit-mesh ones with these as well, due to more efficient memory alloc. */

void *CustomData_bmesh_get(const CustomData *data, void *block, eCustomDataType type);
void *CustomData_bmesh_get_n(const CustomData *data, void *block, eCustomDataType type, int n);

/**
 * Gets from the layer at physical index `n`,
 * \note doesn't check type.
 */
void *CustomData_bmesh_get_layer_n(const CustomData *data, void *block, int n);

bool CustomData_set_layer_name(CustomData *data,
                               eCustomDataType type,
                               int n,
                               blender::StringRef name);
const char *CustomData_get_layer_name(const CustomData *data, eCustomDataType type, int n);

/**
 * Retrieve the data array of the active layer of the given \a type, if it exists. Return null
 * otherwise.
 */
const void *CustomData_get_layer(const CustomData *data, eCustomDataType type);
void *CustomData_get_layer_for_write(CustomData *data, eCustomDataType type, int totelem);

/**
 * Retrieve the data array of the \a nth layer of the given \a type, if it exists. Return null
 * otherwise.
 */
const void *CustomData_get_layer_n(const CustomData *data, eCustomDataType type, int n);
void *CustomData_get_layer_n_for_write(CustomData *data, eCustomDataType type, int n, int totelem);

/**
 * Retrieve the data array of the layer with the given \a name and \a type, if it exists. Return
 * null otherwise.
 */
const void *CustomData_get_layer_named(const CustomData *data,
                                       eCustomDataType type,
                                       blender::StringRef name);
void *CustomData_get_layer_named_for_write(CustomData *data,
                                           eCustomDataType type,
                                           blender::StringRef name,
                                           int totelem);

int CustomData_get_offset(const CustomData *data, eCustomDataType type);
int CustomData_get_offset_named(const CustomData *data,
                                eCustomDataType type,
                                blender::StringRef name);
int CustomData_get_n_offset(const CustomData *data, eCustomDataType type, int n);

int CustomData_get_layer_index(const CustomData *data, eCustomDataType type);
int CustomData_get_layer_index_n(const CustomData *data, eCustomDataType type, int n);
int CustomData_get_named_layer_index(const CustomData *data,
                                     eCustomDataType type,
                                     blender::StringRef name);
int CustomData_get_named_layer_index_notype(const CustomData *data, blender::StringRef name);
int CustomData_get_active_layer_index(const CustomData *data, eCustomDataType type);
int CustomData_get_render_layer_index(const CustomData *data, eCustomDataType type);
int CustomData_get_clone_layer_index(const CustomData *data, eCustomDataType type);
int CustomData_get_stencil_layer_index(const CustomData *data, eCustomDataType type);
int CustomData_get_named_layer(const CustomData *data,
                               eCustomDataType type,
                               blender::StringRef name);
int CustomData_get_active_layer(const CustomData *data, eCustomDataType type);
int CustomData_get_render_layer(const CustomData *data, eCustomDataType type);
int CustomData_get_clone_layer(const CustomData *data, eCustomDataType type);
int CustomData_get_stencil_layer(const CustomData *data, eCustomDataType type);

/**
 * Returns name of the active layer of the given type or NULL
 * if no such active layer is defined.
 */
const char *CustomData_get_active_layer_name(const CustomData *data, eCustomDataType type);

/**
 * Returns name of the default layer of the given type or NULL
 * if no such active layer is defined.
 */
const char *CustomData_get_render_layer_name(const CustomData *data, eCustomDataType type);

bool CustomData_layer_is_anonymous(const CustomData *data, eCustomDataType type, int n);

void CustomData_bmesh_set_n(
    CustomData *data, void *block, eCustomDataType type, int n, const void *source);

/**
 * Sets the nth layer of type as active.
 */
void CustomData_set_layer_active(CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_render(CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_clone(CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_stencil(CustomData *data, eCustomDataType type, int n);

/**
 * For using with an index from #CustomData_get_active_layer_index and
 * #CustomData_get_render_layer_index.
 */
void CustomData_set_layer_active_index(CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_render_index(CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_clone_index(CustomData *data, eCustomDataType type, int n);

/**
 * Adds flag to the layer flags.
 */
void CustomData_set_layer_flag(CustomData *data, eCustomDataType type, int flag);

void CustomData_bmesh_set_default(CustomData *data, void **block);
void CustomData_bmesh_free_block(CustomData *data, void **block);
void CustomData_bmesh_alloc_block(CustomData *data, void **block);

/**
 * Same as #CustomData_bmesh_free_block but zero the memory rather than freeing.
 */
void CustomData_bmesh_free_block_data(CustomData *data, void *block);

int CustomData_sizeof(eCustomDataType type);

/**
 * Get the name of a layer type.
 */
const char *CustomData_layertype_name(eCustomDataType type);
/**
 * Can only ever be one of these.
 */
bool CustomData_layertype_is_singleton(eCustomDataType type);
/**
 * Has dynamically allocated members.
 * This is useful to know if operations such as #memcmp are
 * valid when comparing data from two layers.
 */
bool CustomData_layertype_is_dynamic(eCustomDataType type);
/**
 * \return Maximum number of layers of given \a type, -1 means 'no limit'.
 */
int CustomData_layertype_layers_max(eCustomDataType type);

/** \return The maximum size in bytes needed for a layer name with the given prefix. */
int CustomData_name_maxncpy_calc(blender::StringRef name);

/**
 * Make sure the name of layer at index is unique.
 */
void CustomData_set_layer_unique_name(CustomData *data, int index);

/**
 * For file reading compatibility, returns false if the layer was freed,
 * only after this test passes, `layer->data` should be assigned.
 */
bool CustomData_verify_versions(CustomData *data, int index);

/* BMesh specific custom-data stuff. */

void CustomData_bmesh_init_pool(CustomData *data, int totelem, char htype);

/**
 * Validate and fix data of \a layer,
 * if possible (needs relevant callback in layer's type to be defined).
 *
 * \return True if some errors were found.
 */
bool CustomData_layer_validate(CustomDataLayer *layer, uint totitems, bool do_fixes);

/* External file storage */

void CustomData_external_add(
    CustomData *data, ID *id, eCustomDataType type, int totelem, const char *filepath);
void CustomData_external_remove(CustomData *data, ID *id, eCustomDataType type, int totelem);
bool CustomData_external_test(CustomData *data, eCustomDataType type);

void CustomData_external_write(
    CustomData *data, ID *id, eCustomDataMask mask, int totelem, int free);
void CustomData_external_read(CustomData *data, ID *id, eCustomDataMask mask, int totelem);
void CustomData_external_reload(CustomData *data, ID *id, eCustomDataMask mask, int totelem);

/* Mesh-to-mesh transfer data. */

using cd_datatransfer_interp = void (*)(const CustomDataTransferLayerMap *laymap,
                                        void *dest,
                                        const void **sources,
                                        const float *weights,
                                        int count,
                                        float mix_factor);

enum {
  ME_VERT = 1 << 0,
  ME_EDGE = 1 << 1,
  ME_POLY = 1 << 2,
  ME_LOOP = 1 << 3,
};

struct CustomDataTransferLayerMap {
  int data_type = 0;
  int mix_mode = 0;
  float mix_factor = 0.0f;
  /** If non-NULL, array of weights, one for each dest item, replaces mix_factor. */
  const float *mix_weights = nullptr;

  /** Data source array (can be regular CD data, vertices/edges/etc., key-blocks...). */
  std::variant<const void *, blender::GVArray> data_src;
  /** Data dest array (same type as dat_src). */
  std::variant<void *, blender::GMutableVArraySpan> data_dst = nullptr;
  /** Split from #bke::GSpanAttributeWriter to avoid including BKE_attribute.hh. */
  std::function<void()> tag_modified_fn;
  /** Index to affect in data_src (used e.g. for vgroups). */
  int data_src_n = 0;
  /** Index to affect in data_dst (used e.g. for vgroups). */
  int data_dst_n = 0;
  /** Size of one element of data_src/data_dst. */
  size_t elem_size = 0;

  /** Size of actual data we transfer. */
  size_t data_size = 0;
  /** Offset of actual data we transfer (in element contained in data_src/dst). */
  size_t data_offset = 0;

  /** Opaque pointer, to be used by specific interp callback (e.g. transform-space for normals). */
  void *interp_data = nullptr;

  cd_datatransfer_interp interp = nullptr;
};

/**
 * Those functions assume src_n and dst_n layers of given type exist in resp. src and dst.
 */
void CustomData_data_transfer(const MeshPairRemap *me_remap, CustomDataTransferLayerMap *laymap);

/* .blend file I/O */

/**
 * Prepare given custom data for file writing.
 *
 * \param data: The custom-data to tweak for .blend file writing (modified in place).
 * \param layers_to_write: A reduced set of layers to be written to file.
 * \param write_data: #AttributeStorage data to write, to support the option for writing the new
 * format even when it isn't used at runtime.
 *
 * \warning This function invalidates the custom data struct by changing the layer counts and the
 * #layers pointer, and by invalidating the type map. It expects to work on a shallow copy of
 * the struct.
 */
void CustomData_blend_write_prepare(CustomData &data,
                                    blender::bke::AttrDomain domain,
                                    int domain_size,
                                    blender::Vector<CustomDataLayer, 16> &layers_to_write,
                                    blender::bke::AttributeStorage::BlendWriteData &write_data);

/**
 * \param layers_to_write: Layers created by #CustomData_blend_write_prepare.
 */
void CustomData_blend_write(BlendWriter *writer,
                            CustomData *data,
                            blender::Span<CustomDataLayer> layers_to_write,
                            int count,
                            eCustomDataMask cddata_mask,
                            ID *id);

void CustomData_blend_read(BlendDataReader *reader, CustomData *data, int count);

size_t CustomData_get_elem_size(const CustomDataLayer *layer);

void CustomData_count_memory(const CustomData &data, int totelem, blender::MemoryCounter &memory);

#ifndef NDEBUG
struct DynStr;
/** Use to inspect mesh data when debugging. */
void CustomData_debug_info_from_layers(const CustomData *data, const char *indent, DynStr *dynstr);
#endif /* !NDEBUG */

namespace blender::bke {
std::optional<VolumeGridType> custom_data_type_to_volume_grid_type(eCustomDataType type);
std::optional<eCustomDataType> volume_grid_type_to_custom_data_type(VolumeGridType type);
}  // namespace blender::bke
