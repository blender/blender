/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include <algorithm>
#include <variant>

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_key_types.h"
#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_array_utils.h"
#include "BLI_implicit_sharing.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_undo_system.hh"

#include "DEG_depsgraph.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_undo.hh"
#include "ED_util.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#define USE_ARRAY_STORE

#ifdef USE_ARRAY_STORE
// #  define DEBUG_PRINT
// #  define DEBUG_TIME
#  ifdef DEBUG_TIME
#    include "BLI_time_utildefines.h"
#  endif

#  include "BLI_array_store.h"
#  include "BLI_array_store_utils.h"
/**
 * This used to be much smaller (256), but this caused too much overhead
 * when selection moved to boolean arrays. Especially with high-poly meshes
 * where managing a large number of small chunks could be slow, blocking user interactivity.
 * Use a larger value (in bytes) which calculates the chunk size using #array_chunk_size_calc.
 * See: #105046 & #105205.
 */
#  define ARRAY_CHUNK_SIZE_IN_BYTES 65536
#  define ARRAY_CHUNK_NUM_MIN 256

#  define USE_ARRAY_STORE_THREAD

/**
 * Use run length encoding for boolean custom-data.
 *
 * Avoid poor performance caused by boolean arrays not having enough *uniqueness* to
 * efficiently de-duplicate, see: #136737.
 *
 * NOTE(@ideasman42): This has the down-side that creating undo steps needs to encode
 * data *before* comparing it with the previous state (when creating each undo step).
 * Adding additional work even when nothing change.
 *
 * Since there is overhead for RLE encoding this is only used on boolean array,
 * typically used for storing selection/hidden state as well as edge flags.
 * The encoding has also been optimized for performance instead of "compression"
 * which would pack bits into the smallest possible space.
 *
 * In practice, arrays of 32 million booleans (on an AMD TRX 3990X):
 * - ~0.11 seconds for random data.
 * - ~0.0025 seconds uniform arrays.
 * ... so the tradeoff seems reasonable.
 *
 * There is also the benefit of reduced memory use, although that isn't the goal.
 */
#  define USE_ARRAY_STORE_RLE
#endif

#ifdef USE_ARRAY_STORE_THREAD
#  include "BLI_task.h"
#endif

/** We only need this locally. */
static CLG_LogRef LOG = {"undo.mesh"};

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

#ifdef USE_ARRAY_STORE

static size_t array_chunk_size_calc(const size_t stride)
{
  /* Return a chunk size that targets a size in bytes,
   * this is done so boolean arrays don't add so much overhead and
   * larger arrays aren't so big as to waste memory, see: #105205. */
  return std::max(ARRAY_CHUNK_NUM_MIN, ARRAY_CHUNK_SIZE_IN_BYTES / power_of_2_max_i(stride));
}

/* Single linked list of layers stored per type */
struct BArrayCustomData {
  BArrayCustomData *next;
  eCustomDataType type;
  blender::Array<std::variant<BArrayState *, blender::ImplicitSharingInfoAndData>> states;
};

#  ifdef USE_ARRAY_STORE_RLE
static bool um_customdata_layer_use_rle(const BArrayCustomData *bcd)
{
  /* NOTE(@ideasman42): This could be enabled for all byte sized layers.
   * for now only use for boolean layers to address: #136737. */
  if (bcd->type == CD_PROP_BOOL) {
    BLI_assert(CustomData_sizeof(bcd->type) == 1);
    return true;
  }
  return false;
}
#  endif

#endif

struct UndoMesh {
  /**
   * This undo-meshes in `um_arraystore.local_links`.
   * Not to be confused with the next and previous undo steps.
   */
  UndoMesh *local_next, *local_prev;

  Mesh *mesh;
  char selectmode;

  /**
   * The active shape key associated with this mesh.
   *
   * NOTE(@ideasman42): This isn't a perfect solution, if you edit keys and change shapes this
   * works well (fixing #32442), but editing shape keys, going into object mode, removing or
   * changing their order, then go back into edit-mode and undo will give issues - where the old
   * index will be out of sync with the new object index.
   *
   * There are a few ways this could be made to work but for now its a known limitation with mixing
   * object and edit-mode operations.
   */
  int shapenr;

#ifdef USE_ARRAY_STORE
  /* Null arrays are considered empty. */
  struct { /* most data is stored as 'custom' data */
    BArrayCustomData *vdata, *edata, *ldata, *pdata;
    BArrayState *face_offset_indices;
    BArrayState **keyblocks;
    BArrayState *mselect;
  } store;
#endif /* USE_ARRAY_STORE */

  size_t undo_size;
};

#ifdef USE_ARRAY_STORE

/* -------------------------------------------------------------------- */
/** \name Array Store
 * \{ */

/**
 * Store separate #BArrayStore_AtSize so multiple threads
 * can access array stores without locking.
 */
enum {
  ARRAY_STORE_INDEX_VERT = 0,
  ARRAY_STORE_INDEX_EDGE,
  ARRAY_STORE_INDEX_LOOP,
  ARRAY_STORE_INDEX_POLY,
  ARRAY_STORE_INDEX_POLY_OFFSETS,
  ARRAY_STORE_INDEX_SHAPE,
  ARRAY_STORE_INDEX_MSEL,
};
#  define ARRAY_STORE_INDEX_NUM (ARRAY_STORE_INDEX_MSEL + 1)

static struct {
  BArrayStore_AtSize bs_stride[ARRAY_STORE_INDEX_NUM];
  int users;

  /**
   * A list of #UndoMesh items ordered from oldest to newest
   * used to access previous undo data for a mesh.
   */
  ListBase local_links;

#  ifdef USE_ARRAY_STORE_THREAD
  TaskPool *task_pool;
#  endif

} um_arraystore = {{{nullptr}}};

static void um_arraystore_cd_compact(CustomData *cdata,
                                     const size_t data_len,
                                     const bool create,
                                     const int bs_index,
                                     const BArrayCustomData *bcd_reference,
                                     BArrayCustomData **r_bcd_first)
{
  using namespace blender;
  if (data_len == 0) {
    if (create) {
      *r_bcd_first = nullptr;
    }
  }

  const BArrayCustomData *bcd_reference_current = bcd_reference;
  BArrayCustomData *bcd = nullptr, *bcd_first = nullptr, *bcd_prev = nullptr;
  for (int layer_start = 0, layer_end; layer_start < cdata->totlayer; layer_start = layer_end) {
    const eCustomDataType type = eCustomDataType(cdata->layers[layer_start].type);

    /* Perform a full copy on dynamic layers.
     *
     * Unfortunately we can't compare dynamic layer types as they contain allocated pointers,
     * which burns CPU cycles looking for duplicate data that doesn't exist.
     * The array data isn't comparable once copied from the mesh,
     * this bottlenecks on high poly meshes, see #84114.
     *
     * Ideally the data would be expanded into a format that could be de-duplicated effectively,
     * this would require a flat representation of each dynamic custom-data layer.
     *
     * Instead, these non-trivial custom data layer are stored in the undo system using implicit
     * sharing, to avoid the copy from the undo mesh.
     */
    const bool layer_type_is_dynamic = CustomData_layertype_is_dynamic(type);

    layer_end = layer_start + 1;
    while ((layer_end < cdata->totlayer) && (type == cdata->layers[layer_end].type)) {
      layer_end++;
    }

    const int stride = CustomData_sizeof(type);
    BArrayStore *bs = create ? BLI_array_store_at_size_ensure(&um_arraystore.bs_stride[bs_index],
                                                              stride,
                                                              array_chunk_size_calc(stride)) :
                               nullptr;
    const int layer_len = layer_end - layer_start;

    if (create) {
      if (bcd_reference_current && (bcd_reference_current->type == type)) {
        /* common case, the reference is aligned */
      }
      else {
        bcd_reference_current = nullptr;

        /* Do a full lookup when unaligned. */
        if (bcd_reference) {
          const BArrayCustomData *bcd_iter = bcd_reference;
          while (bcd_iter) {
            if (bcd_iter->type == type) {
              bcd_reference_current = bcd_iter;
              break;
            }
            bcd_iter = bcd_iter->next;
          }
        }
      }
    }

    if (create) {
      bcd = MEM_new<BArrayCustomData>(__func__);
      bcd->next = nullptr;
      bcd->type = type;
      bcd->states.reinitialize(layer_end - layer_start);

      if (bcd_prev) {
        bcd_prev->next = bcd;
        bcd_prev = bcd;
      }
      else {
        bcd_first = bcd;
        bcd_prev = bcd;
      }
    }

    CustomDataLayer *layer = &cdata->layers[layer_start];
    for (int i = 0; i < layer_len; i++, layer++) {
      if (create) {
        if (layer->data) {
          if (layer_type_is_dynamic) {
            /* See comment on `layer_type_is_dynamic` above. */
            const ImplicitSharingInfo *sharing_info = layer->sharing_info;
            sharing_info->add_user();
            bcd->states[i] = ImplicitSharingInfoAndData{sharing_info, layer->data};
          }
          else {
            const BArrayState *state_reference = nullptr;
            if (bcd_reference_current && i < bcd_reference_current->states.size()) {
              state_reference = std::get<BArrayState *>(bcd_reference_current->states[i]);
            }

            void *data_final = layer->data;
            size_t data_final_size = size_t(data_len) * stride;

#  ifdef USE_ARRAY_STORE_RLE
            const bool use_rle = um_customdata_layer_use_rle(bcd);
            uint8_t *data_enc = nullptr;
            if (use_rle) {
              /* Store the size in the encoded data (for convenience). */
              size_t data_enc_extra_size = sizeof(size_t);
              size_t data_enc_len;
              data_enc = BLI_array_store_rle_encode(reinterpret_cast<const uint8_t *>(data_final),
                                                    data_final_size,
                                                    data_enc_extra_size,
                                                    &data_enc_len);
              memcpy(data_enc, &data_final_size, data_enc_extra_size);
              data_final = data_enc;
              data_final_size = data_enc_extra_size + data_enc_len;
            }
#  endif

            bcd->states[i] = {
                BLI_array_store_state_add(bs, data_final, data_final_size, state_reference),
            };

#  ifdef USE_ARRAY_STORE_RLE
            if (use_rle) {
              MEM_freeN(data_enc);
            }
#  endif
          }
        }
        else {
          bcd->states[i] = nullptr;
        }
      }

      if (layer->data) {
        layer->sharing_info->remove_user_and_delete_if_last();
        layer->sharing_info = nullptr;
        layer->data = nullptr;
      }
    }

    if (create) {
      if (bcd_reference_current) {
        bcd_reference_current = bcd_reference_current->next;
      }
    }
  }

  if (create) {
    *r_bcd_first = bcd_first;
  }
}

/**
 * \note There is no room for data going out of sync here.
 * The layers and the states are stored together so this can be kept working.
 */
static void um_arraystore_cd_expand(const BArrayCustomData *bcd,
                                    CustomData *cdata,
                                    const size_t data_len)
{
  using namespace blender;
  CustomDataLayer *layer = cdata->layers;
  while (bcd) {
    const int stride = CustomData_sizeof(bcd->type);
    for (int i = 0; i < bcd->states.size(); i++) {
      BLI_assert(bcd->type == layer->type);
      if (std::holds_alternative<BArrayState *>(bcd->states[i])) {
        const BArrayState *state = std::get<BArrayState *>(bcd->states[i]);
        if (state) {
          size_t state_len;
          void *data = BLI_array_store_state_data_get_alloc(state, &state_len);

#  ifdef USE_ARRAY_STORE_RLE
          const bool use_rle = um_customdata_layer_use_rle(bcd);
          if (use_rle) {
            /* Store the size in the encoded data (for convenience). */
            size_t data_enc_extra_size = sizeof(size_t);
            const uint8_t *data_enc = reinterpret_cast<uint8_t *>(data);
            size_t data_dec_len;
            memcpy(&data_dec_len, data_enc, sizeof(size_t));
            uint8_t *data_dec = MEM_malloc_arrayN<uint8_t>(data_dec_len, __func__);
            BLI_array_store_rle_decode(data_enc + data_enc_extra_size,
                                       state_len - data_enc_extra_size,
                                       data_dec,
                                       data_dec_len);
            MEM_freeN(data);
            data = static_cast<void *>(data_dec);
            /* Just for the assert to succeed. */
            state_len = data_dec_len;
          }
#  endif

          layer->data = data;
          layer->sharing_info = implicit_sharing::info_for_mem_free(layer->data);
          BLI_assert(stride * data_len == state_len);
          UNUSED_VARS_NDEBUG(stride, data_len);
        }
        else {
          layer->data = nullptr;
        }
      }
      else {
        ImplicitSharingInfoAndData state = std::get<ImplicitSharingInfoAndData>(bcd->states[i]);
        layer->data = const_cast<void *>(state.data);
        layer->sharing_info = state.sharing_info;
        layer->sharing_info->add_user();
      }
      layer++;
    }
    bcd = bcd->next;
  }
}

static void um_arraystore_cd_free(BArrayCustomData *bcd, const int bs_index)
{
  using namespace blender;
  while (bcd) {
    BArrayCustomData *bcd_next = bcd->next;
    const int stride = CustomData_sizeof(bcd->type);
    BArrayStore *bs = BLI_array_store_at_size_get(&um_arraystore.bs_stride[bs_index], stride);
    for (int i = 0; i < bcd->states.size(); i++) {
      if (std::holds_alternative<BArrayState *>(bcd->states[i])) {
        if (BArrayState *state = std::get<BArrayState *>(bcd->states[i])) {
          BLI_array_store_state_remove(bs, state);
        }
      }
      else {
        ImplicitSharingInfoAndData state = std::get<ImplicitSharingInfoAndData>(bcd->states[i]);
        state.sharing_info->remove_user_and_delete_if_last();
      }
    }
    MEM_delete(bcd);
    bcd = bcd_next;
  }
}

/**
 * \param create: When false, only free the arrays.
 * This is done since when reading from an undo state, they must be temporarily expanded.
 * then discarded afterwards, having this argument avoids having 2x code paths.
 */
static void um_arraystore_compact_ex(UndoMesh *um, const UndoMesh *um_ref, bool create)
{
  Mesh *mesh = um->mesh;

  /* Compacting can be time consuming, run in parallel.
   *
   * NOTE(@ideasman42): this could be further parallelized with every custom-data layer
   * running in its own thread. If this is a bottleneck it's worth considering.
   * At the moment it seems fast enough to split by domain.
   * Since this is itself a background thread, using too many threads here could
   * interfere with foreground tasks. */
  blender::threading::parallel_invoke(
      4096 < (mesh->verts_num + mesh->edges_num + mesh->corners_num + mesh->faces_num),
      [&]() {
        um_arraystore_cd_compact(&mesh->vert_data,
                                 mesh->verts_num,
                                 create,
                                 ARRAY_STORE_INDEX_VERT,
                                 um_ref ? um_ref->store.vdata : nullptr,
                                 &um->store.vdata);
      },
      [&]() {
        um_arraystore_cd_compact(&mesh->edge_data,
                                 mesh->edges_num,
                                 create,
                                 ARRAY_STORE_INDEX_EDGE,
                                 um_ref ? um_ref->store.edata : nullptr,
                                 &um->store.edata);
      },
      [&]() {
        um_arraystore_cd_compact(&mesh->corner_data,
                                 mesh->corners_num,
                                 create,
                                 ARRAY_STORE_INDEX_LOOP,
                                 um_ref ? um_ref->store.ldata : nullptr,
                                 &um->store.ldata);
      },
      [&]() {
        um_arraystore_cd_compact(&mesh->face_data,
                                 mesh->faces_num,
                                 create,
                                 ARRAY_STORE_INDEX_POLY,
                                 um_ref ? um_ref->store.pdata : nullptr,
                                 &um->store.pdata);
      },
      [&]() {
        if (mesh->face_offset_indices) {
          BLI_assert(create == (um->store.face_offset_indices == nullptr));
          if (create) {
            const BArrayState *state_reference = um_ref ? um_ref->store.face_offset_indices :
                                                          nullptr;
            const size_t stride = sizeof(*mesh->face_offset_indices);
            BArrayStore *bs = BLI_array_store_at_size_ensure(
                &um_arraystore.bs_stride[ARRAY_STORE_INDEX_POLY_OFFSETS],
                stride,
                array_chunk_size_calc(stride));
            um->store.face_offset_indices = BLI_array_store_state_add(bs,
                                                                      mesh->face_offset_indices,
                                                                      size_t(mesh->faces_num + 1) *
                                                                          stride,
                                                                      state_reference);
          }
          blender::implicit_sharing::free_shared_data(&mesh->face_offset_indices,
                                                      &mesh->runtime->face_offsets_sharing_info);
        }
      },
      [&]() {
        if (mesh->key && mesh->key->totkey) {
          const size_t stride = mesh->key->elemsize;
          BArrayStore *bs = create ? BLI_array_store_at_size_ensure(
                                         &um_arraystore.bs_stride[ARRAY_STORE_INDEX_SHAPE],
                                         stride,
                                         array_chunk_size_calc(stride)) :
                                     nullptr;
          if (create) {
            um->store.keyblocks = static_cast<BArrayState **>(
                MEM_mallocN(mesh->key->totkey * sizeof(*um->store.keyblocks), __func__));
          }
          KeyBlock *keyblock = static_cast<KeyBlock *>(mesh->key->block.first);
          for (int i = 0; i < mesh->key->totkey; i++, keyblock = keyblock->next) {
            if (create) {
              const BArrayState *state_reference = (um_ref && um_ref->mesh->key &&
                                                    (i < um_ref->mesh->key->totkey)) ?
                                                       um_ref->store.keyblocks[i] :
                                                       nullptr;
              um->store.keyblocks[i] = BLI_array_store_state_add(
                  bs, keyblock->data, size_t(keyblock->totelem) * stride, state_reference);
            }

            if (keyblock->data) {
              MEM_freeN(keyblock->data);
              keyblock->data = nullptr;
            }
          }
        }
      },
      [&]() {
        if (mesh->mselect && mesh->totselect) {
          BLI_assert(create == (um->store.mselect == nullptr));
          if (create) {
            const BArrayState *state_reference = um_ref ? um_ref->store.mselect : nullptr;
            const size_t stride = sizeof(*mesh->mselect);
            BArrayStore *bs = BLI_array_store_at_size_ensure(
                &um_arraystore.bs_stride[ARRAY_STORE_INDEX_MSEL],
                stride,
                array_chunk_size_calc(stride));
            um->store.mselect = BLI_array_store_state_add(
                bs, mesh->mselect, size_t(mesh->totselect) * stride, state_reference);
          }

          /* keep mesh->totselect for validation */
          MEM_freeN(mesh->mselect);
          mesh->mselect = nullptr;
        }
      });

  if (create) {
    um_arraystore.users += 1;
  }
}

/**
 * Move data from allocated arrays to de-duplicated states and clear arrays.
 */
static void um_arraystore_compact(UndoMesh *um, const UndoMesh *um_ref)
{
  um_arraystore_compact_ex(um, um_ref, true);
}

static void um_arraystore_compact_with_info(UndoMesh *um, const UndoMesh *um_ref)
{
#  ifdef DEBUG_PRINT
  size_t size_expanded_prev = 0, size_compacted_prev = 0;

  for (int bs_index = 0; bs_index < ARRAY_STORE_INDEX_NUM; bs_index++) {
    size_t size_expanded_prev_iter, size_compacted_prev_iter;
    BLI_array_store_at_size_calc_memory_usage(
        &um_arraystore.bs_stride[bs_index], &size_expanded_prev_iter, &size_compacted_prev_iter);
    size_expanded_prev += size_expanded_prev_iter;
    size_compacted_prev += size_compacted_prev_iter;
  }
#  endif

#  ifdef DEBUG_TIME
  TIMEIT_START(mesh_undo_compact);
#  endif

  um_arraystore_compact(um, um_ref);

#  ifdef DEBUG_TIME
  TIMEIT_END(mesh_undo_compact);
#  endif

#  ifdef DEBUG_PRINT
  {
    size_t size_expanded = 0, size_compacted = 0;

    for (int bs_index = 0; bs_index < ARRAY_STORE_INDEX_NUM; bs_index++) {
      size_t size_expanded_iter, size_compacted_iter;
      BLI_array_store_at_size_calc_memory_usage(
          &um_arraystore.bs_stride[bs_index], &size_expanded_iter, &size_compacted_iter);
      size_expanded += size_expanded_iter;
      size_compacted += size_compacted_iter;
    }

    const double percent_total = size_expanded ?
                                     ((double(size_compacted) / double(size_expanded)) * 100.0) :
                                     -1.0;

    size_t size_expanded_step = size_expanded - size_expanded_prev;
    size_t size_compacted_step = size_compacted - size_compacted_prev;
    const double percent_step = size_expanded_step ?
                                    ((double(size_compacted_step) / double(size_expanded_step)) *
                                     100.0) :
                                    -1.0;

    printf("overall memory use: %.8f%% of expanded size\n", percent_total);
    printf("step memory use:    %.8f%% of expanded size\n", percent_step);
  }
#  endif
}

#  ifdef USE_ARRAY_STORE_THREAD

struct UMArrayData {
  UndoMesh *um;
  const UndoMesh *um_ref; /* can be nullptr */
};
static void um_arraystore_compact_cb(TaskPool *__restrict /*pool*/, void *taskdata)
{
  UMArrayData *um_data = static_cast<UMArrayData *>(taskdata);
  um_arraystore_compact_with_info(um_data->um, um_data->um_ref);
}

#  endif /* USE_ARRAY_STORE_THREAD */

/**
 * Remove data we only expanded for temporary use.
 */
static void um_arraystore_expand_clear(UndoMesh *um)
{
  um_arraystore_compact_ex(um, nullptr, false);
}

static void um_arraystore_expand(UndoMesh *um)
{
  Mesh *mesh = um->mesh;

  um_arraystore_cd_expand(um->store.vdata, &mesh->vert_data, mesh->verts_num);
  um_arraystore_cd_expand(um->store.edata, &mesh->edge_data, mesh->edges_num);
  um_arraystore_cd_expand(um->store.ldata, &mesh->corner_data, mesh->corners_num);
  um_arraystore_cd_expand(um->store.pdata, &mesh->face_data, mesh->faces_num);

  if (um->store.keyblocks) {
    const size_t stride = mesh->key->elemsize;
    KeyBlock *keyblock = static_cast<KeyBlock *>(mesh->key->block.first);
    for (int i = 0; i < mesh->key->totkey; i++, keyblock = keyblock->next) {
      const BArrayState *state = um->store.keyblocks[i];
      size_t state_len;
      keyblock->data = BLI_array_store_state_data_get_alloc(state, &state_len);
      BLI_assert(keyblock->totelem == (state_len / stride));
      UNUSED_VARS_NDEBUG(stride);
    }
  }

  if (um->store.face_offset_indices) {
    const size_t stride = sizeof(*mesh->face_offset_indices);
    const BArrayState *state = um->store.face_offset_indices;
    size_t state_len;
    mesh->face_offset_indices = static_cast<int *>(
        BLI_array_store_state_data_get_alloc(state, &state_len));
    mesh->runtime->face_offsets_sharing_info = blender::implicit_sharing::info_for_mem_free(
        mesh->face_offset_indices);
    BLI_assert((mesh->faces_num + 1) == (state_len / stride));
    UNUSED_VARS_NDEBUG(stride);
  }
  if (um->store.mselect) {
    const size_t stride = sizeof(*mesh->mselect);
    const BArrayState *state = um->store.mselect;
    size_t state_len;
    mesh->mselect = static_cast<MSelect *>(
        BLI_array_store_state_data_get_alloc(state, &state_len));
    BLI_assert(mesh->totselect == (state_len / stride));
    UNUSED_VARS_NDEBUG(stride);
  }
}

static void um_arraystore_free(UndoMesh *um)
{
  Mesh *mesh = um->mesh;

  um_arraystore_cd_free(um->store.vdata, ARRAY_STORE_INDEX_VERT);
  um_arraystore_cd_free(um->store.edata, ARRAY_STORE_INDEX_EDGE);
  um_arraystore_cd_free(um->store.ldata, ARRAY_STORE_INDEX_LOOP);
  um_arraystore_cd_free(um->store.pdata, ARRAY_STORE_INDEX_POLY);

  if (um->store.keyblocks) {
    const size_t stride = mesh->key->elemsize;
    BArrayStore *bs = BLI_array_store_at_size_get(
        &um_arraystore.bs_stride[ARRAY_STORE_INDEX_SHAPE], stride);
    for (int i = 0; i < mesh->key->totkey; i++) {
      BArrayState *state = um->store.keyblocks[i];
      BLI_array_store_state_remove(bs, state);
    }
    MEM_freeN(um->store.keyblocks);
    um->store.keyblocks = nullptr;
  }

  if (um->store.face_offset_indices) {
    const size_t stride = sizeof(*mesh->face_offset_indices);
    BArrayStore *bs = BLI_array_store_at_size_get(
        &um_arraystore.bs_stride[ARRAY_STORE_INDEX_POLY_OFFSETS], stride);
    BArrayState *state = um->store.face_offset_indices;
    BLI_array_store_state_remove(bs, state);
    um->store.face_offset_indices = nullptr;
  }
  if (um->store.mselect) {
    const size_t stride = sizeof(*mesh->mselect);
    BArrayStore *bs = BLI_array_store_at_size_get(&um_arraystore.bs_stride[ARRAY_STORE_INDEX_MSEL],
                                                  stride);
    BArrayState *state = um->store.mselect;
    BLI_array_store_state_remove(bs, state);
    um->store.mselect = nullptr;
  }

  um_arraystore.users -= 1;

  BLI_assert(um_arraystore.users >= 0);

  if (um_arraystore.users == 0) {
#  ifdef DEBUG_PRINT
    printf("mesh undo store: freeing all data!\n");
#  endif
    for (int bs_index = 0; bs_index < ARRAY_STORE_INDEX_NUM; bs_index++) {
      BLI_array_store_at_size_clear(&um_arraystore.bs_stride[bs_index]);
    }
#  ifdef USE_ARRAY_STORE_THREAD
    BLI_task_pool_free(um_arraystore.task_pool);
    um_arraystore.task_pool = nullptr;
#  endif
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Array Store Utilities
 * \{ */

/**
 * Create an array of #UndoMesh from `objects`.
 *
 * where each element in the resulting array is the most recently created
 * undo-mesh for the object's mesh.
 * When no undo-mesh can be found that array index is nullptr.
 *
 * This is used for de-duplicating memory between undo steps,
 * failure to find the undo step will store a full duplicate in memory.
 * define `DEBUG_PRINT` to check memory is de-duplicating as expected.
 */
static UndoMesh **mesh_undostep_reference_elems_from_objects(Object **object, int object_len)
{
  /* Map: `Mesh.id.session_uid` -> `UndoMesh`. */
  GHash *uuid_map = BLI_ghash_ptr_new_ex(__func__, object_len);
  UndoMesh **um_references = MEM_calloc_arrayN<UndoMesh *>(object_len, __func__);
  for (int i = 0; i < object_len; i++) {
    const Mesh *mesh = static_cast<const Mesh *>(object[i]->data);
    BLI_ghash_insert(uuid_map, POINTER_FROM_INT(mesh->id.session_uid), &um_references[i]);
  }
  int uuid_map_len = object_len;

  /* Loop backwards over all previous mesh undo data until either:
   * - All elements have been found (where `um_references` we'll have every element set).
   * - There are no undo steps left to look for. */
  UndoMesh *um_iter = static_cast<UndoMesh *>(um_arraystore.local_links.last);
  while (um_iter && (uuid_map_len != 0)) {
    UndoMesh **um_p;
    if ((um_p = static_cast<UndoMesh **>(BLI_ghash_popkey(
             uuid_map, POINTER_FROM_INT(um_iter->mesh->id.session_uid), nullptr))))
    {
      *um_p = um_iter;
      uuid_map_len--;
    }
    um_iter = um_iter->local_prev;
  }
  BLI_assert(uuid_map_len == BLI_ghash_len(uuid_map));
  BLI_ghash_free(uuid_map, nullptr, nullptr);
  if (uuid_map_len == object_len) {
    MEM_freeN(um_references);
    um_references = nullptr;
  }
  return um_references;
}

/** \} */

#endif /* USE_ARRAY_STORE */

/* for callbacks */
/* undo simply makes copies of a bmesh */
/**
 *
 * Copy data from `em` into `um`.
 *
 * \param um_ref: The reference to use for de-duplicating memory between undo-steps.
 *
 * \note See #undomesh_to_editmesh for an explanation for why passing in data-blocks is avoided.
 */
static void *undomesh_from_editmesh(UndoMesh *um,
                                    BMEditMesh *em,
                                    Key *key,
                                    const ListBase *vertex_group_names,
                                    const int vertex_group_active_index,
                                    UndoMesh *um_ref)
{
  BLI_assert(BLI_array_is_zeroed(um, 1));
#ifdef USE_ARRAY_STORE_THREAD
  /* changes this waits is low, but must have finished */
  if (um_arraystore.task_pool) {
    BLI_task_pool_work_and_wait(um_arraystore.task_pool);
  }
#endif

  um->mesh = blender::bke::mesh_new_no_attributes(0, 0, 0, 0);

  /* make sure shape keys work */
  if (key != nullptr) {
    um->mesh->key = (Key *)BKE_id_copy_ex(
        nullptr, &key->id, nullptr, LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA);
  }
  else {
    um->mesh->key = nullptr;
  }

  /* Uncomment for troubleshooting. */
  if (false) {
    BM_mesh_is_valid(em->bm);

    /* Ensure UV's are in a valid state. */
    if (em->bm->uv_select_sync_valid) {
      const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);
      bool check_flush = true;
      /* This should check the sticky mode too (currently the scene isn't available). */
      bool check_contiguous = (cd_loop_uv_offset != -1);
      UVSelectValidateInfo info;
      bool is_valid = BM_mesh_uvselect_is_valid(
          em->bm, cd_loop_uv_offset, true, check_flush, check_contiguous, &info);
      if (is_valid == false) {
        fprintf(stderr, "ERROR: UV sync check failed!\n");
      }
      // BLI_assert(is_valid);
    }
  }

  CustomData_MeshMasks cd_mask_extra{};
  cd_mask_extra.vmask = CD_MASK_SHAPE_KEYINDEX;
  BMeshToMeshParams params{};
  /* Undo code should not be manipulating 'G_MAIN->object' hooks/vertex-parent. */
  params.calc_object_remap = false;
  params.update_shapekey_indices = false;
  params.cd_mask_extra = cd_mask_extra;
  params.active_shapekey_to_mvert = true;
  BM_mesh_bm_to_me(nullptr, em->bm, um->mesh, &params);
  BKE_defgroup_copy_list(&um->mesh->vertex_group_names, vertex_group_names);
  um->mesh->vertex_group_active_index = vertex_group_active_index;

  um->selectmode = em->selectmode;
  um->shapenr = em->bm->shapenr;

#ifdef USE_ARRAY_STORE
  {
    /* Add ourselves. */
    BLI_addtail(&um_arraystore.local_links, um);

#  ifdef USE_ARRAY_STORE_THREAD
    if (um_arraystore.task_pool == nullptr) {
      um_arraystore.task_pool = BLI_task_pool_create_background(nullptr, TASK_PRIORITY_LOW);
    }

    UMArrayData *um_data = MEM_mallocN<UMArrayData>(__func__);
    um_data->um = um;
    um_data->um_ref = um_ref;

    BLI_task_pool_push(um_arraystore.task_pool, um_arraystore_compact_cb, um_data, true, nullptr);
#  else
    um_arraystore_compact_with_info(um, um_ref);
#  endif
  }
#else
  UNUSED_VARS(um_ref);
#endif

  return um;
}

/**
 * Copy data from `um` into `em`.
 *
 * \note while `em` defines the "edit-mesh" there are some exceptions which are intentionally
 * kept as separate arguments instead of passing in the #Object or #Mesh data blocks.
 * This is done to avoid confusion from passing in multiple meshes, where it's not always clear
 * what the source of truth is for mesh data - which can make the logic difficult to reason about.
 */
static void undomesh_to_editmesh(UndoMesh *um,
                                 BMEditMesh *em,
                                 ListBase *vertex_group_names,
                                 int *vertex_group_active_index)
{
  BMEditMesh *em_tmp;
  BMesh *bm;

#ifdef USE_ARRAY_STORE
#  ifdef USE_ARRAY_STORE_THREAD
  /* changes this waits is low, but must have finished */
  BLI_task_pool_work_and_wait(um_arraystore.task_pool);
#  endif

#  ifdef DEBUG_TIME
  TIMEIT_START(mesh_undo_expand);
#  endif

  um_arraystore_expand(um);

#  ifdef DEBUG_TIME
  TIMEIT_END(mesh_undo_expand);
#  endif
#endif /* USE_ARRAY_STORE */

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(um->mesh);

  em->bm->shapenr = um->shapenr;

  EDBM_mesh_free_data(em);

  BMeshCreateParams create_params{};
  create_params.use_toolflags = true;
  bm = BM_mesh_create(&allocsize, &create_params);

  BMeshFromMeshParams convert_params{};
  /* Handled with tessellation. */
  convert_params.calc_face_normal = false;
  convert_params.calc_vert_normal = false;
  convert_params.active_shapekey = um->shapenr;
  BM_mesh_bm_from_me(bm, um->mesh, &convert_params);
  BLI_freelistN(vertex_group_names);
  BKE_defgroup_copy_list(vertex_group_names, &um->mesh->vertex_group_names);
  *vertex_group_active_index = um->mesh->vertex_group_active_index;

  em_tmp = BKE_editmesh_create(bm);
  *em = *em_tmp;

  /* Calculate face normals and tessellation at once since it's multi-threaded. */
  BKE_editmesh_looptris_and_normals_calc(em);

  em->selectmode = um->selectmode;
  bm->selectmode = um->selectmode;

  bm->spacearr_dirty = BM_SPACEARR_DIRTY_ALL;

  MEM_delete(em_tmp);

#ifdef USE_ARRAY_STORE
  um_arraystore_expand_clear(um);
#endif
}

static void undomesh_free_data(UndoMesh *um)
{
  Mesh *mesh = um->mesh;

#ifdef USE_ARRAY_STORE

#  ifdef USE_ARRAY_STORE_THREAD
  /* Chances this waits is low, but must have finished. */
  BLI_task_pool_work_and_wait(um_arraystore.task_pool);
#  endif

  /* We need to expand so any allocations in custom-data are freed with the mesh. */
  um_arraystore_expand(um);

  BLI_assert(BLI_findindex(&um_arraystore.local_links, um) != -1);
  BLI_remlink(&um_arraystore.local_links, um);

  um_arraystore_free(um);
#endif

  if (mesh->key) {
    BKE_id_free(nullptr, mesh->key);
    mesh->key = nullptr;
  }

  BKE_id_free(nullptr, mesh);
  um->mesh = nullptr;
}

static Object *editmesh_object_from_context(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  if (obedit && obedit->type == OB_MESH) {
    const Mesh *mesh = static_cast<Mesh *>(obedit->data);
    if (mesh->runtime->edit_mesh != nullptr) {
      return obedit;
    }
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 *
 * \note This is similar for all edit-mode types.
 * \{ */

struct MeshUndoStep_Elem {
  UndoRefID_Object obedit_ref;
  UndoMesh data;
};

/**
 * Scene & tool-setting data.
 * Used so edit-mesh selection settings follow the underlying mesh data.
 */
struct MeshUndoStep_SceneData {
  char selectmode;
  char uv_selectmode;
  char uv_sticky;
  char uv_flag;
};

struct MeshUndoStep {
  UndoStep step;
  /** See #ED_undo_object_editmode_validate_scene_from_windows code comment for details. */
  UndoRefID_Scene scene_ref;

  MeshUndoStep_SceneData scene_data;
  MeshUndoStep_Elem *elems;
  uint elems_len;
};

static bool mesh_undosys_poll(bContext *C)
{
  return editmesh_object_from_context(C) != nullptr;
}

static bool mesh_undosys_step_encode(bContext *C, Main *bmain, UndoStep *us_p)
{
  MeshUndoStep *us = (MeshUndoStep *)us_p;

  /* Important not to use the 3D view when getting objects because all objects
   * outside of this list will be moved out of edit-mode when reading back undo steps. */
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const ToolSettings *ts = scene->toolsettings;
  blender::Vector<Object *> objects = ED_undo_editmode_objects_from_view_layer(scene, view_layer);

  us->scene_ref.ptr = scene;
  us->elems = MEM_calloc_arrayN<MeshUndoStep_Elem>(objects.size(), __func__);
  us->elems_len = objects.size();

  UndoMesh **um_references = nullptr;

#ifdef USE_ARRAY_STORE
  um_references = mesh_undostep_reference_elems_from_objects(objects.data(), objects.size());
#endif

  {
    MeshUndoStep_SceneData &scene_data = us->scene_data;
    scene_data.selectmode = ts->selectmode;
    scene_data.uv_selectmode = ts->uv_selectmode;
    scene_data.uv_sticky = ts->uv_sticky;
    scene_data.uv_flag = ts->uv_flag;
  }

  for (uint i = 0; i < objects.size(); i++) {
    Object *obedit = objects[i];
    MeshUndoStep_Elem *elem = &us->elems[i];

    elem->obedit_ref.ptr = obedit;
    Mesh *mesh = static_cast<Mesh *>(elem->obedit_ref.ptr->data);
    BMEditMesh *em = mesh->runtime->edit_mesh.get();
    undomesh_from_editmesh(&elem->data,
                           em,
                           mesh->key,
                           &mesh->vertex_group_names,
                           mesh->vertex_group_active_index,
                           um_references ? um_references[i] : nullptr);

    em->needs_flush_to_id = 1;
    us->step.data_size += elem->data.undo_size;

#ifdef USE_ARRAY_STORE
    /** As this is only data storage it is safe to set the session ID here. */
    elem->data.mesh->id.session_uid = mesh->id.session_uid;
#endif
  }

  if (um_references != nullptr) {
    MEM_freeN(um_references);
  }

  bmain->is_memfile_undo_flush_needed = true;

  return true;
}

static void mesh_undosys_step_decode(
    bContext *C, Main *bmain, UndoStep *us_p, const eUndoStepDir /*dir*/, bool /*is_final*/)
{
  MeshUndoStep *us = (MeshUndoStep *)us_p;
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  ED_undo_object_editmode_validate_scene_from_windows(
      CTX_wm_manager(C), us->scene_ref.ptr, &scene, &view_layer);
  ED_undo_object_editmode_restore_helper(
      scene, view_layer, &us->elems[0].obedit_ref.ptr, us->elems_len, sizeof(*us->elems));

  BLI_assert(BKE_object_is_in_editmode(us->elems[0].obedit_ref.ptr));

  for (uint i = 0; i < us->elems_len; i++) {
    MeshUndoStep_Elem *elem = &us->elems[i];
    Object *obedit = elem->obedit_ref.ptr;
    Mesh *mesh = static_cast<Mesh *>(obedit->data);
    if (mesh->runtime->edit_mesh == nullptr) {
      /* Should never fail, may not crash but can give odd behavior. */
      CLOG_ERROR(&LOG,
                 "name='%s', failed to enter edit-mode for object '%s', undo state invalid",
                 us_p->name,
                 obedit->id.name);
      continue;
    }
    BMEditMesh *em = mesh->runtime->edit_mesh.get();
    undomesh_to_editmesh(
        &elem->data, em, &mesh->vertex_group_names, &mesh->vertex_group_active_index);

    obedit->shapenr = em->bm->shapenr;

    em->needs_flush_to_id = 1;
    DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
    /* The object update tag is necessary to cause modifiers to reevaluate after vertex group
     * changes. */
    DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
  }

  /* The first element is always active */
  ED_undo_object_set_active_or_warn(
      scene, view_layer, us->elems[0].obedit_ref.ptr, us_p->name, &LOG);

  /* Check after setting active (unless undoing into another scene). */
  BLI_assert(mesh_undosys_poll(C) || (scene != CTX_data_scene(C)));

  {
    /* Follow settings related to selection.
     * While other flags could be included too: it's important the user doesn't
     * undo into a state where the scene settings would show a different selection
     * to the selection the user was previously editing. */
    constexpr char uv_flag_undo = UV_FLAG_SELECT_SYNC | UV_FLAG_SELECT_ISLAND;

    ToolSettings *ts = scene->toolsettings;
    const MeshUndoStep_SceneData &scene_data = us->scene_data;
    ts->selectmode = scene_data.selectmode;
    ts->uv_selectmode = scene_data.uv_selectmode;
    ts->uv_sticky = scene_data.uv_sticky;
    ts->uv_flag = (ts->uv_flag & ~uv_flag_undo) | (scene_data.uv_flag & uv_flag_undo);
  }

  bmain->is_memfile_undo_flush_needed = true;

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, nullptr);
}

static void mesh_undosys_step_free(UndoStep *us_p)
{
  MeshUndoStep *us = (MeshUndoStep *)us_p;

  for (uint i = 0; i < us->elems_len; i++) {
    MeshUndoStep_Elem *elem = &us->elems[i];
    undomesh_free_data(&elem->data);
  }
  MEM_freeN(us->elems);
}

static void mesh_undosys_foreach_ID_ref(UndoStep *us_p,
                                        UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                        void *user_data)
{
  MeshUndoStep *us = (MeshUndoStep *)us_p;

  foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->scene_ref));
  for (uint i = 0; i < us->elems_len; i++) {
    MeshUndoStep_Elem *elem = &us->elems[i];
    foreach_ID_ref_fn(user_data, ((UndoRefID *)&elem->obedit_ref));
  }
}

void ED_mesh_undosys_type(UndoType *ut)
{
  ut->name = "Edit Mesh";
  ut->poll = mesh_undosys_poll;
  ut->step_encode = mesh_undosys_step_encode;
  ut->step_decode = mesh_undosys_step_decode;
  ut->step_free = mesh_undosys_step_free;

  ut->step_foreach_ID_ref = mesh_undosys_foreach_ID_ref;

  ut->flags = UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE;

  ut->step_size = sizeof(MeshUndoStep);
}

/** \} */
