/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * \brief CustomData interface, see also DNA_customdata_types.h.
 */

#pragma once

#include "BLI_implicit_sharing.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
#ifdef __cplusplus
#  include "BLI_set.hh"
#  include "BLI_span.hh"
#  include "BLI_string_ref.hh"
#  include "BLI_vector.hh"
#endif

#include "DNA_customdata_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BMesh;
struct BlendDataReader;
struct BlendWriter;
struct CustomData;
struct CustomData_MeshMasks;
struct ID;
typedef uint64_t eCustomDataMask;

/* These names are used as prefixes for UV layer names to find the associated boolean
 * layers. They should never be longer than 2 chars, as MAX_CUSTOMDATA_LAYER_NAME
 * has 4 extra bytes above what can be used for the base layer name, and these
 * prefixes are placed between 2 '.'s at the start of the layer name.
 * For example The uv vert selection layer of a layer named 'UVMap.001'
 * will be called '.vs.UVMap.001' . */
#define UV_VERTSEL_NAME "vs"
#define UV_EDGESEL_NAME "es"
#define UV_PINNED_NAME "pn"

/**
 * UV map related customdata offsets into BMesh attribute blocks. See #BM_uv_map_get_offsets.
 * Defined in #BKE_customdata.h to avoid including bmesh.h in many unrelated areas.
 * An offset of -1 means that the corresponding layer does not exist.
 */
typedef struct BMUVOffsets {
  int uv;
  int select_vert;
  int select_edge;
  int pin;
} BMUVOffsets;

/* A data type large enough to hold 1 element from any custom-data layer type. */
typedef struct {
  unsigned char data[64];
} CDBlockBytes;

extern const CustomData_MeshMasks CD_MASK_BAREMESH;
extern const CustomData_MeshMasks CD_MASK_BAREMESH_ORIGINDEX;
extern const CustomData_MeshMasks CD_MASK_MESH;
extern const CustomData_MeshMasks CD_MASK_DERIVEDMESH;
extern const CustomData_MeshMasks CD_MASK_BMESH;
extern const CustomData_MeshMasks CD_MASK_EVERYTHING;

/* for ORIGINDEX layer type, indicates no original index for this element */
#define ORIGINDEX_NONE -1

/* initializes a CustomData object with the same layer setup as source and
 * memory space for totelem elements. mask must be an array of length
 * CD_NUMTYPES elements, that indicate if a layer can be copied. */

/** Add/copy/merge allocation types. */
typedef enum eCDAllocType {
  /** Allocate and set to default, which is usually just zeroed memory. */
  CD_SET_DEFAULT = 2,
  /**
   * Default construct new layer values. Does nothing for trivial types. This should be used
   * if all layer values will be set by the caller after creating the layer.
   */
  CD_CONSTRUCT = 5,
} eCDAllocType;

#define CD_TYPE_AS_MASK(_type) (eCustomDataMask)((eCustomDataMask)1 << (eCustomDataMask)(_type))

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
 * Copies the "value" (e.g. `mloopuv` UV or `mloopcol` colors) from one block to
 * another, while not overwriting anything else (e.g. flags).  probably only
 * implemented for `mloopuv/mloopcol`, for now.
 */
void CustomData_data_copy_value(eCustomDataType type, const void *source, void *dest);
void CustomData_data_set_default_value(eCustomDataType type, void *elem);

/**
 * Mixes the "value" (e.g. `mloopuv` UV or `mloopcol` colors) from one block into
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
 */
void CustomData_copy(const struct CustomData *source,
                     struct CustomData *dest,
                     eCustomDataMask mask,
                     int totelem);
/**
 * Initializes a CustomData object with the same layers as source. The data is not copied from the
 * source. Instead, the new layers are initialized using the given `alloctype`.
 */
void CustomData_copy_layout(const struct CustomData *source,
                            struct CustomData *dest,
                            eCustomDataMask mask,
                            eCDAllocType alloctype,
                            int totelem);

/* BMESH_TODO, not really a public function but `readfile.cc` needs it. */
void CustomData_update_typemap(struct CustomData *data);

/**
 * Copies all layers from source to destination that don't exist there yet.
 */
bool CustomData_merge(const struct CustomData *source,
                      struct CustomData *dest,
                      eCustomDataMask mask,
                      int totelem);
/**
 * Copies all layers from source to destination that don't exist there yet. The layer data is not
 * copied. Instead the newly created layers are initialized using the given `alloctype`.
 */
bool CustomData_merge_layout(const struct CustomData *source,
                             struct CustomData *dest,
                             eCustomDataMask mask,
                             eCDAllocType alloctype,
                             int totelem);

/**
 * Reallocate custom data to a new element count. If the new size is larger, the new values use
 * the #CD_CONSTRUCT behavior, so trivial types must be initialized by the caller. After being
 * resized, the #CustomData does not contain any referenced layers.
 */
void CustomData_realloc(struct CustomData *data, int old_size, int new_size);

/**
 * BMesh version of CustomData_merge_layout; merges the layouts of source and `dest`,
 * then goes through the mesh and makes sure all the custom-data blocks are
 * consistent with the new layout.
 */
bool CustomData_bmesh_merge_layout(const struct CustomData *source,
                                   struct CustomData *dest,
                                   eCustomDataMask mask,
                                   eCDAllocType alloctype,
                                   struct BMesh *bm,
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
 */
void CustomData_reset(struct CustomData *data);

/**
 * Frees data associated with a CustomData object (doesn't free the object itself, though).
 */
void CustomData_free(struct CustomData *data, int totelem);

/**
 * Same as above, but only frees layers which matches the given mask.
 */
void CustomData_free_typemask(struct CustomData *data, int totelem, eCustomDataMask mask);

/**
 * Frees all layers with #CD_FLAG_TEMPORARY.
 */
void CustomData_free_temporary(struct CustomData *data, int totelem);

/**
 * Adds a layer of the given type to the #CustomData object. The new layer is initialized based on
 * the given alloctype.
 * \return The layer data.
 */
void *CustomData_add_layer(struct CustomData *data,
                           eCustomDataType type,
                           eCDAllocType alloctype,
                           int totelem);

/**
 * Adds a layer of the given type to the #CustomData object. The new layer takes ownership of the
 * passed in `layer_data`. If a #ImplicitSharingInfoHandle is passed in, its user count is
 * increased.
 */
const void *CustomData_add_layer_with_data(struct CustomData *data,
                                           eCustomDataType type,
                                           void *layer_data,
                                           int totelem,
                                           const ImplicitSharingInfoHandle *sharing_info);

/**
 * Same as above but accepts a name.
 */
void *CustomData_add_layer_named(struct CustomData *data,
                                 eCustomDataType type,
                                 eCDAllocType alloctype,
                                 int totelem,
                                 const char *name);

const void *CustomData_add_layer_named_with_data(struct CustomData *data,
                                                 eCustomDataType type,
                                                 void *layer_data,
                                                 int totelem,
                                                 const char *name,
                                                 const ImplicitSharingInfoHandle *sharing_info);

void *CustomData_add_layer_anonymous(struct CustomData *data,
                                     eCustomDataType type,
                                     eCDAllocType alloctype,
                                     int totelem,
                                     const AnonymousAttributeIDHandle *anonymous_id);
const void *CustomData_add_layer_anonymous_with_data(
    struct CustomData *data,
    eCustomDataType type,
    const AnonymousAttributeIDHandle *anonymous_id,
    int totelem,
    void *layer_data,
    const ImplicitSharingInfoHandle *sharing_info);

/**
 * Frees the active or first data layer with the give type.
 * returns 1 on success, 0 if no layer with the given type is found
 *
 * In edit-mode, use #EDBM_data_layer_free instead of this function.
 */
bool CustomData_free_layer(struct CustomData *data, eCustomDataType type, int totelem, int index);
bool CustomData_free_layer_named(struct CustomData *data, const char *name, const int totelem);

/**
 * Frees the layer index with the give type.
 * returns 1 on success, 0 if no layer with the given type is found.
 *
 * In edit-mode, use #EDBM_data_layer_free instead of this function.
 */
bool CustomData_free_layer_active(struct CustomData *data, eCustomDataType type, int totelem);

/**
 * Same as above, but free all layers with type.
 */
void CustomData_free_layers(struct CustomData *data, eCustomDataType type, int totelem);

/**
 * Returns true if a layer with the specified type exists.
 */
bool CustomData_has_layer(const struct CustomData *data, eCustomDataType type);
bool CustomData_has_layer_named(const struct CustomData *data,
                                eCustomDataType type,
                                const char *name);

/**
 * Returns the number of layers with this type.
 */
int CustomData_number_of_layers(const struct CustomData *data, eCustomDataType type);
int CustomData_number_of_anonymous_layers(const struct CustomData *data, eCustomDataType type);
int CustomData_number_of_layers_typemask(const struct CustomData *data, eCustomDataMask mask);

/**
 * Set the #CD_FLAG_NOCOPY flag in custom data layers where the mask is
 * zero for the layer type, so only layer types specified by the mask will be copied
 */
void CustomData_set_only_copy(const struct CustomData *data, eCustomDataMask mask);

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
void CustomData_copy_data_layer(const struct CustomData *source,
                                struct CustomData *dest,
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
void CustomData_copy_elements(eCustomDataType type,
                              void *src_data_ofs,
                              void *dst_data_ofs,
                              int count);
void CustomData_bmesh_copy_data(const struct CustomData *source,
                                struct CustomData *dest,
                                void *src_block,
                                void **dest_block);
void CustomData_bmesh_copy_data_exclude_by_type(const struct CustomData *source,
                                                struct CustomData *dest,
                                                void *src_block,
                                                void **dest_block,
                                                eCustomDataMask mask_exclude);

/**
 * Copies data of a single layer of a given type.
 */
void CustomData_copy_layer_type_data(const struct CustomData *source,
                                     struct CustomData *destination,
                                     eCustomDataType type,
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
 * Retrieve a pointer to an element of the active layer of the given \a type, chosen by the
 * \a index, if it exists.
 */
void *CustomData_get_for_write(struct CustomData *data,
                               int index,
                               eCustomDataType type,
                               int totelem);
/**
 * Retrieve a pointer to an element of the \a nth layer of the given \a type, chosen by the
 * \a index, if it exists.
 */
void *CustomData_get_n_for_write(
    struct CustomData *data, eCustomDataType type, int index, int n, int totelem);

/* BMesh Custom Data Functions.
 * Should replace edit-mesh ones with these as well, due to more efficient memory alloc. */

void *CustomData_bmesh_get(const struct CustomData *data, void *block, eCustomDataType type);
void *CustomData_bmesh_get_n(const struct CustomData *data,
                             void *block,
                             eCustomDataType type,
                             int n);

/**
 * Gets from the layer at physical index `n`,
 * \note doesn't check type.
 */
void *CustomData_bmesh_get_layer_n(const struct CustomData *data, void *block, int n);

bool CustomData_set_layer_name(struct CustomData *data,
                               eCustomDataType type,
                               int n,
                               const char *name);
const char *CustomData_get_layer_name(const struct CustomData *data, eCustomDataType type, int n);

/**
 * Retrieve the data array of the active layer of the given \a type, if it exists. Return null
 * otherwise.
 */
const void *CustomData_get_layer(const struct CustomData *data, eCustomDataType type);
void *CustomData_get_layer_for_write(struct CustomData *data, eCustomDataType type, int totelem);

/**
 * Retrieve the data array of the \a nth layer of the given \a type, if it exists. Return null
 * otherwise.
 */
const void *CustomData_get_layer_n(const struct CustomData *data, eCustomDataType type, int n);
void *CustomData_get_layer_n_for_write(struct CustomData *data,
                                       eCustomDataType type,
                                       int n,
                                       int totelem);

/**
 * Retrieve the data array of the layer with the given \a name and \a type, if it exists. Return
 * null otherwise.
 */
const void *CustomData_get_layer_named(const struct CustomData *data,
                                       eCustomDataType type,
                                       const char *name);
void *CustomData_get_layer_named_for_write(CustomData *data,
                                           eCustomDataType type,
                                           const char *name,
                                           int totelem);

int CustomData_get_offset(const struct CustomData *data, eCustomDataType type);
int CustomData_get_offset_named(const struct CustomData *data,
                                eCustomDataType type,
                                const char *name);
int CustomData_get_n_offset(const struct CustomData *data, eCustomDataType type, int n);

int CustomData_get_layer_index(const struct CustomData *data, eCustomDataType type);
int CustomData_get_layer_index_n(const struct CustomData *data, eCustomDataType type, int n);
int CustomData_get_named_layer_index(const struct CustomData *data,
                                     eCustomDataType type,
                                     const char *name);
int CustomData_get_named_layer_index_notype(const struct CustomData *data, const char *name);
int CustomData_get_active_layer_index(const struct CustomData *data, eCustomDataType type);
int CustomData_get_render_layer_index(const struct CustomData *data, eCustomDataType type);
int CustomData_get_clone_layer_index(const struct CustomData *data, eCustomDataType type);
int CustomData_get_stencil_layer_index(const struct CustomData *data, eCustomDataType type);
int CustomData_get_named_layer(const struct CustomData *data,
                               eCustomDataType type,
                               const char *name);
int CustomData_get_active_layer(const struct CustomData *data, eCustomDataType type);
int CustomData_get_render_layer(const struct CustomData *data, eCustomDataType type);
int CustomData_get_clone_layer(const struct CustomData *data, eCustomDataType type);
int CustomData_get_stencil_layer(const struct CustomData *data, eCustomDataType type);

/**
 * Returns name of the active layer of the given type or NULL
 * if no such active layer is defined.
 */
const char *CustomData_get_active_layer_name(const struct CustomData *data, eCustomDataType type);

/**
 * Returns name of the default layer of the given type or NULL
 * if no such active layer is defined.
 */
const char *CustomData_get_render_layer_name(const struct CustomData *data, eCustomDataType type);

bool CustomData_layer_is_anonymous(const struct CustomData *data, eCustomDataType type, int n);

void CustomData_bmesh_set(const struct CustomData *data,
                          void *block,
                          eCustomDataType type,
                          const void *source);

void CustomData_bmesh_set_n(
    struct CustomData *data, void *block, eCustomDataType type, int n, const void *source);

/**
 * Sets the nth layer of type as active.
 */
void CustomData_set_layer_active(struct CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_render(struct CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_clone(struct CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_stencil(struct CustomData *data, eCustomDataType type, int n);

/**
 * For using with an index from #CustomData_get_active_layer_index and
 * #CustomData_get_render_layer_index.
 */
void CustomData_set_layer_active_index(struct CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_render_index(struct CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_clone_index(struct CustomData *data, eCustomDataType type, int n);
void CustomData_set_layer_stencil_index(struct CustomData *data, eCustomDataType type, int n);

/**
 * Adds flag to the layer flags.
 */
void CustomData_set_layer_flag(struct CustomData *data, eCustomDataType type, int flag);
void CustomData_clear_layer_flag(struct CustomData *data, eCustomDataType type, int flag);

void CustomData_bmesh_set_default(struct CustomData *data, void **block);
void CustomData_bmesh_free_block(struct CustomData *data, void **block);
void CustomData_bmesh_alloc_block(struct CustomData *data, void **block);

/**
 * Same as #CustomData_bmesh_free_block but zero the memory rather than freeing.
 */
void CustomData_bmesh_free_block_data(struct CustomData *data, void *block);
/**
 * A selective version of #CustomData_bmesh_free_block_data.
 */
void CustomData_bmesh_free_block_data_exclude_by_type(struct CustomData *data,
                                                      void *block,
                                                      eCustomDataMask mask_exclude);

/**
 * Query info over types.
 */
void CustomData_file_write_info(eCustomDataType type,
                                const char **r_struct_name,
                                int *r_struct_num);
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

#ifdef __cplusplus

/** \return The maximum size in bytes needed for a layer name with the given prefix. */
int CustomData_name_maxncpy_calc(blender::StringRef name);

#endif

/**
 * Make sure the name of layer at index is unique.
 */
void CustomData_set_layer_unique_name(struct CustomData *data, int index);

void CustomData_validate_layer_name(const struct CustomData *data,
                                    eCustomDataType type,
                                    const char *name,
                                    char *outname);

/**
 * For file reading compatibility, returns false if the layer was freed,
 * only after this test passes, `layer->data` should be assigned.
 */
bool CustomData_verify_versions(struct CustomData *data, int index);

/* BMesh specific custom-data stuff. */

void CustomData_bmesh_init_pool(struct CustomData *data, int totelem, char htype);

/**
 * Validate and fix data of \a layer,
 * if possible (needs relevant callback in layer's type to be defined).
 *
 * \return True if some errors were found.
 */
bool CustomData_layer_validate(struct CustomDataLayer *layer, uint totitems, bool do_fixes);

/* External file storage */

void CustomData_external_add(struct CustomData *data,
                             struct ID *id,
                             eCustomDataType type,
                             int totelem,
                             const char *filepath);
void CustomData_external_remove(struct CustomData *data,
                                struct ID *id,
                                eCustomDataType type,
                                int totelem);
bool CustomData_external_test(struct CustomData *data, eCustomDataType type);

void CustomData_external_write(
    struct CustomData *data, struct ID *id, eCustomDataMask mask, int totelem, int free);
void CustomData_external_read(struct CustomData *data,
                              struct ID *id,
                              eCustomDataMask mask,
                              int totelem);
void CustomData_external_reload(struct CustomData *data,
                                struct ID *id,
                                eCustomDataMask mask,
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
  CD_FAKE_SEAM = CD_FAKE | 100, /* UV seam flag for edges. */

  /* Multiple types of mesh elements... */
  CD_FAKE_UV =
      CD_FAKE |
      CD_PROP_FLOAT2, /* UV flag, because we handle both loop's UVs and face's textures. */

  CD_FAKE_LNOR = CD_FAKE |
                 CD_CUSTOMLOOPNORMAL, /* Because we play with clnor and temp lnor layers here. */

  CD_FAKE_SHARP = CD_FAKE | 200, /* Sharp flag for edges, smooth flag for faces. */

  CD_FAKE_BWEIGHT = CD_FAKE | 300,
  CD_FAKE_CREASE = CD_FAKE | 400,
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

  /** Data source array (can be regular CD data, vertices/edges/etc., key-blocks...). */
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
  /** For bit-flag transfer, flag(s) to affect in transferred data. */
  uint64_t data_flag;

  /** Opaque pointer, to be used by specific interp callback (e.g. transform-space for normals). */
  void *interp_data;

  cd_datatransfer_interp interp;
} CustomDataTransferLayerMap;

/**
 * Those functions assume src_n and dst_n layers of given type exist in resp. src and dst.
 */
void CustomData_data_transfer(const struct MeshPairRemap *me_remap,
                              const CustomDataTransferLayerMap *laymap);

/* .blend file I/O */

#ifdef __cplusplus

/**
 * Prepare given custom data for file writing.
 *
 * \param data: The custom-data to tweak for .blend file writing (modified in place).
 * \param layers_to_write: A reduced set of layers to be written to file.
 *
 * \warning This function invalidates the custom data struct by changing the layer counts and the
 * #layers pointer, and by invalidating the type map. It expects to work on a shallow copy of
 * the struct.
 */
void CustomData_blend_write_prepare(CustomData &data,
                                    blender::Vector<CustomDataLayer, 16> &layers_to_write,
                                    const blender::Set<std::string> &skip_names = {});

/**
 * \param layers_to_write: Layers created by #CustomData_blend_write_prepare.
 */
void CustomData_blend_write(BlendWriter *writer,
                            CustomData *data,
                            blender::Span<CustomDataLayer> layers_to_write,
                            int count,
                            eCustomDataMask cddata_mask,
                            ID *id);

#endif

void CustomData_blend_read(struct BlendDataReader *reader, struct CustomData *data, int count);

size_t CustomData_get_elem_size(const struct CustomDataLayer *layer);

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

#ifdef __cplusplus
#  include "BLI_cpp_type.hh"

namespace blender::bke {
const CPPType *custom_data_type_to_cpp_type(eCustomDataType type);
eCustomDataType cpp_type_to_custom_data_type(const CPPType &type);
}  // namespace blender::bke
#endif
