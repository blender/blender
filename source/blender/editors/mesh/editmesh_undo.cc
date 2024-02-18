/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

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
#include "BLI_string.h"
#include "BLI_task.hh"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
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
#endif

#ifdef USE_ARRAY_STORE_THREAD
#  include "BLI_task.h"
#endif

/** We only need this locally. */
static CLG_LogRef LOG = {"ed.undo.mesh"};

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
  int states_len; /* number of layers for each type */
  BArrayState *states[0];
};

#endif

struct UndoMesh {
  /**
   * This undo-meshes in `um_arraystore.local_links`.
   * Not to be confused with the next and previous undo steps.
   */
  UndoMesh *local_next, *local_prev;

  Mesh mesh;
  int selectmode;
  char uv_selectmode;

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
     * Notes:
     *
     * - Ideally the data would be expanded into a format that could be de-duplicated effectively,
     *   this would require a flat representation of each dynamic custom-data layer.
     *
     * - The data in the layer could be kept as-is to save on the extra copy,
     *   it would complicate logic in this function.
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
      bcd = static_cast<BArrayCustomData *>(
          MEM_callocN(sizeof(BArrayCustomData) + (layer_len * sizeof(BArrayState *)), __func__));
      bcd->next = nullptr;
      bcd->type = type;
      bcd->states_len = layer_end - layer_start;

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
          BArrayState *state_reference = (bcd_reference_current &&
                                          i < bcd_reference_current->states_len) ?
                                             bcd_reference_current->states[i] :
                                             nullptr;
          /* See comment on `layer_type_is_dynamic` above. */
          if (layer_type_is_dynamic) {
            state_reference = nullptr;
          }

          bcd->states[i] = BLI_array_store_state_add(
              bs, layer->data, size_t(data_len) * stride, state_reference);
        }
        else {
          bcd->states[i] = nullptr;
        }
      }

      if (layer->data) {
        if (layer->sharing_info) {
          /* This assumes that the layer is not shared, which it is not here because it has just
           * been created in #BM_mesh_bm_to_me. The situation is a bit tricky here, because the
           * layer data may be freed partially below for e.g. vertex groups. A potentially better
           * solution might be to not pass "dynamic" layers (see `layer_type_is_dynamic`) to the
           * array store at all. */
          BLI_assert(layer->sharing_info->is_mutable());
          /* Intentionally don't call #MEM_delete, because we want to free the sharing info without
           * the data here. In general this would not be allowed because one can't be sure how to
           * free the data without the sharing info. */
          MEM_freeN(const_cast<blender::ImplicitSharingInfo *>(layer->sharing_info));
        }
        MEM_freeN(layer->data);
        layer->data = nullptr;
        layer->sharing_info = nullptr;
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
  CustomDataLayer *layer = cdata->layers;
  while (bcd) {
    const int stride = CustomData_sizeof(bcd->type);
    for (int i = 0; i < bcd->states_len; i++) {
      BLI_assert(bcd->type == layer->type);
      if (bcd->states[i]) {
        size_t state_len;
        layer->data = BLI_array_store_state_data_get_alloc(bcd->states[i], &state_len);
        BLI_assert(stride * data_len == state_len);
        UNUSED_VARS_NDEBUG(stride, data_len);
      }
      else {
        layer->data = nullptr;
      }
      layer++;
    }
    bcd = bcd->next;
  }
}

static void um_arraystore_cd_free(BArrayCustomData *bcd, const int bs_index)
{
  while (bcd) {
    BArrayCustomData *bcd_next = bcd->next;
    const int stride = CustomData_sizeof(bcd->type);
    BArrayStore *bs = BLI_array_store_at_size_get(&um_arraystore.bs_stride[bs_index], stride);
    for (int i = 0; i < bcd->states_len; i++) {
      if (bcd->states[i]) {
        BLI_array_store_state_remove(bs, bcd->states[i]);
      }
    }
    MEM_freeN(bcd);
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
  Mesh *mesh = &um->mesh;

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
            BArrayState *state_reference = um_ref ? um_ref->store.face_offset_indices : nullptr;
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
              BArrayState *state_reference = (um_ref && um_ref->mesh.key &&
                                              (i < um_ref->mesh.key->totkey)) ?
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
            BArrayState *state_reference = um_ref ? um_ref->store.mselect : nullptr;
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
  Mesh *mesh = &um->mesh;

  um_arraystore_cd_expand(um->store.vdata, &mesh->vert_data, mesh->verts_num);
  um_arraystore_cd_expand(um->store.edata, &mesh->edge_data, mesh->edges_num);
  um_arraystore_cd_expand(um->store.ldata, &mesh->corner_data, mesh->corners_num);
  um_arraystore_cd_expand(um->store.pdata, &mesh->face_data, mesh->faces_num);

  if (um->store.keyblocks) {
    const size_t stride = mesh->key->elemsize;
    KeyBlock *keyblock = static_cast<KeyBlock *>(mesh->key->block.first);
    for (int i = 0; i < mesh->key->totkey; i++, keyblock = keyblock->next) {
      BArrayState *state = um->store.keyblocks[i];
      size_t state_len;
      keyblock->data = BLI_array_store_state_data_get_alloc(state, &state_len);
      BLI_assert(keyblock->totelem == (state_len / stride));
      UNUSED_VARS_NDEBUG(stride);
    }
  }

  if (um->store.face_offset_indices) {
    const size_t stride = sizeof(*mesh->face_offset_indices);
    BArrayState *state = um->store.face_offset_indices;
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
    BArrayState *state = um->store.mselect;
    size_t state_len;
    mesh->mselect = static_cast<MSelect *>(
        BLI_array_store_state_data_get_alloc(state, &state_len));
    BLI_assert(mesh->totselect == (state_len / stride));
    UNUSED_VARS_NDEBUG(stride);
  }
}

static void um_arraystore_free(UndoMesh *um)
{
  Mesh *mesh = &um->mesh;

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
  UndoMesh **um_references = static_cast<UndoMesh **>(
      MEM_callocN(sizeof(UndoMesh *) * object_len, __func__));
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
    if ((um_p = static_cast<UndoMesh **>(
             BLI_ghash_popkey(uuid_map, POINTER_FROM_INT(um_iter->mesh.id.session_uid), nullptr))))
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
 * \param um_ref: The reference to use for de-duplicating memory between undo-steps.
 */
static void *undomesh_from_editmesh(UndoMesh *um, BMEditMesh *em, Key *key, UndoMesh *um_ref)
{
  BLI_assert(BLI_array_is_zeroed(um, 1));
#ifdef USE_ARRAY_STORE_THREAD
  /* changes this waits is low, but must have finished */
  if (um_arraystore.task_pool) {
    BLI_task_pool_work_and_wait(um_arraystore.task_pool);
  }
#endif
  /* make sure shape keys work */
  if (key != nullptr) {
    um->mesh.key = (Key *)BKE_id_copy_ex(
        nullptr, &key->id, nullptr, LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA);
  }
  else {
    um->mesh.key = nullptr;
  }

  /* Uncomment for troubleshooting. */
  // BM_mesh_validate(em->bm);

  /* Copy the ID name characters to the mesh so code that depends on accessing the ID type can work
   * on it. Necessary to use the attribute API. */
  STRNCPY(um->mesh.id.name, "MEundomesh_from_editmesh");

  /* Runtime data is necessary for some asserts in other code, and the overhead of creating it for
   * undo meshes should be low. */
  BLI_assert(um->mesh.runtime == nullptr);
  um->mesh.runtime = new blender::bke::MeshRuntime();

  CustomData_MeshMasks cd_mask_extra{};
  cd_mask_extra.vmask = CD_MASK_SHAPE_KEYINDEX;
  BMeshToMeshParams params{};
  /* Undo code should not be manipulating 'G_MAIN->object' hooks/vertex-parent. */
  params.calc_object_remap = false;
  params.update_shapekey_indices = false;
  params.cd_mask_extra = cd_mask_extra;
  params.active_shapekey_to_mvert = true;
  BM_mesh_bm_to_me(nullptr, em->bm, &um->mesh, &params);

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

    UMArrayData *um_data = static_cast<UMArrayData *>(MEM_mallocN(sizeof(*um_data), __func__));
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

static void undomesh_to_editmesh(UndoMesh *um, Object *ob, BMEditMesh *em)
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

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(&um->mesh);

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
  BM_mesh_bm_from_me(bm, &um->mesh, &convert_params);

  em_tmp = BKE_editmesh_create(bm);
  *em = *em_tmp;

  /* Calculate face normals and tessellation at once since it's multi-threaded. */
  BKE_editmesh_looptris_and_normals_calc(em);

  em->selectmode = um->selectmode;
  bm->selectmode = um->selectmode;

  bm->spacearr_dirty = BM_SPACEARR_DIRTY_ALL;

  ob->shapenr = um->shapenr;

  MEM_freeN(em_tmp);

#ifdef USE_ARRAY_STORE
  um_arraystore_expand_clear(um);
#endif
}

static void undomesh_free_data(UndoMesh *um)
{
  Mesh *mesh = &um->mesh;

#ifdef USE_ARRAY_STORE

#  ifdef USE_ARRAY_STORE_THREAD
  /* changes this waits is low, but must have finished */
  BLI_task_pool_work_and_wait(um_arraystore.task_pool);
#  endif

  /* we need to expand so any allocations in custom-data are freed with the mesh */
  um_arraystore_expand(um);

  BLI_assert(BLI_findindex(&um_arraystore.local_links, um) != -1);
  BLI_remlink(&um_arraystore.local_links, um);

  um_arraystore_free(um);
#endif

  if (mesh->key) {
    BKE_key_free_data(mesh->key);
    MEM_freeN(mesh->key);
  }

  BKE_mesh_free_data_for_undo(mesh);
}

static Object *editmesh_object_from_context(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  if (obedit && obedit->type == OB_MESH) {
    const Mesh *mesh = static_cast<Mesh *>(obedit->data);
    if (mesh->edit_mesh != nullptr) {
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

struct MeshUndoStep {
  UndoStep step;
  /** See #ED_undo_object_editmode_validate_scene_from_windows code comment for details. */
  UndoRefID_Scene scene_ref;
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
  uint objects_len = 0;
  Object **objects = ED_undo_editmode_objects_from_view_layer(scene, view_layer, &objects_len);

  us->scene_ref.ptr = scene;
  us->elems = static_cast<MeshUndoStep_Elem *>(
      MEM_callocN(sizeof(*us->elems) * objects_len, __func__));
  us->elems_len = objects_len;

  UndoMesh **um_references = nullptr;

#ifdef USE_ARRAY_STORE
  um_references = mesh_undostep_reference_elems_from_objects(objects, objects_len);
#endif

  for (uint i = 0; i < objects_len; i++) {
    Object *ob = objects[i];
    MeshUndoStep_Elem *elem = &us->elems[i];

    elem->obedit_ref.ptr = ob;
    Mesh *mesh = static_cast<Mesh *>(elem->obedit_ref.ptr->data);
    BMEditMesh *em = mesh->edit_mesh;
    undomesh_from_editmesh(
        &elem->data, mesh->edit_mesh, mesh->key, um_references ? um_references[i] : nullptr);
    em->needs_flush_to_id = 1;
    us->step.data_size += elem->data.undo_size;
    elem->data.uv_selectmode = ts->uv_selectmode;

#ifdef USE_ARRAY_STORE
    /** As this is only data storage it is safe to set the session ID here. */
    elem->data.mesh.id.session_uid = mesh->id.session_uid;
#endif
  }
  MEM_freeN(objects);

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
    if (mesh->edit_mesh == nullptr) {
      /* Should never fail, may not crash but can give odd behavior. */
      CLOG_ERROR(&LOG,
                 "name='%s', failed to enter edit-mode for object '%s', undo state invalid",
                 us_p->name,
                 obedit->id.name);
      continue;
    }
    BMEditMesh *em = mesh->edit_mesh;
    undomesh_to_editmesh(&elem->data, obedit, em);
    em->needs_flush_to_id = 1;
    DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
  }

  /* The first element is always active */
  ED_undo_object_set_active_or_warn(
      scene, view_layer, us->elems[0].obedit_ref.ptr, us_p->name, &LOG);

  /* Check after setting active (unless undoing into another scene). */
  BLI_assert(mesh_undosys_poll(C) || (scene != CTX_data_scene(C)));

  scene->toolsettings->selectmode = us->elems[0].data.selectmode;
  scene->toolsettings->uv_selectmode = us->elems[0].data.uv_selectmode;

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
