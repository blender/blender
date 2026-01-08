/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <optional>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_task.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_ID.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_anim_data.hh"
#include "BKE_attribute.hh"
#include "BKE_curve.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_idtype.hh"
#include "BKE_key.hh"
#include "BKE_lattice.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_scene.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "BLO_read_write.hh"

namespace blender {

static void shapekey_copy_data(Main * /*bmain*/,
                               std::optional<Library *> /*owner_library*/,
                               ID *id_dst,
                               const ID *id_src,
                               const int /*flag*/)
{
  Key *key_dst = id_cast<Key *>(id_dst);
  const Key *key_src = id_cast<const Key *>(id_src);
  BLI_duplicatelist(&key_dst->block, &key_src->block);

  KeyBlock *kb_dst, *kb_src;
  for (kb_src = static_cast<KeyBlock *>(key_src->block.first),
      kb_dst = static_cast<KeyBlock *>(key_dst->block.first);
       kb_dst;
       kb_src = kb_src->next, kb_dst = kb_dst->next)
  {
    if (kb_dst->data) {
      kb_dst->data = MEM_dupalloc_void(kb_dst->data);
    }
    if (kb_src == key_src->refkey) {
      key_dst->refkey = kb_dst;
    }
  }
}

static void shapekey_free_data(ID *id)
{
  Key *key = id_cast<Key *>(id);
  while (KeyBlock *kb = static_cast<KeyBlock *>(BLI_pophead(&key->block))) {
    if (kb->data) {
      MEM_delete_void(kb->data);
    }
    MEM_delete(kb);
  }
}

static void shapekey_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Key *key = reinterpret_cast<Key *>(id);
  BKE_LIB_FOREACHID_PROCESS_ID(data, key->from, IDWALK_CB_LOOPBACK);
}

static ID **shapekey_owner_pointer_get(ID *id, const bool debug_relationship_assert)
{
  Key *key = id_cast<Key *>(id);

  if (debug_relationship_assert) {
    BLI_assert(key->from != nullptr);
    BLI_assert(BKE_key_from_id(key->from) == key);
  }

  return &key->from;
}

static void shapekey_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Key *key = id_cast<Key *>(id);
  const bool is_undo = BLO_write_is_undo(writer);

  /* Write LibData. */
  writer->write_id_struct(id_address, key);
  BKE_id_blend_write(writer, &key->id);

  /* Direct data. */
  for (KeyBlock &kb : key->block) {
    KeyBlock tmp_kb = kb;
    /* Do not store actual geometry data in case this is a library override ID. */
    if (ID_IS_OVERRIDE_LIBRARY(key) && !is_undo) {
      tmp_kb.totelem = 0;
      tmp_kb.data = nullptr;
    }
    writer->write_struct_at_address(&kb, &tmp_kb);
    if (tmp_kb.data != nullptr) {
      BLO_write_raw(writer, tmp_kb.totelem * key->elemsize, tmp_kb.data);
    }
  }
}

/* Old defines from DNA_ipo_types.h for data-type, stored in DNA - don't modify! */
#define IPO_FLOAT 4
#define IPO_BEZTRIPLE 100
#define IPO_BPOINT 101

static void shapekey_blend_read_data(BlendDataReader *reader, ID *id)
{
  Key *key = id_cast<Key *>(id);
  BLO_read_struct_list(reader, KeyBlock, &(key->block));

  BLO_read_struct(reader, KeyBlock, &key->refkey);

  for (KeyBlock &kb : key->block) {
    BLO_read_data_address(reader, &kb.data);

    /* NOTE: This is endianness-sensitive. */
    /* Keyblock data would need specific endian switching depending of the exact type of data it
     * contain. */
  }
}

static void shapekey_blend_read_after_liblink(BlendLibReader * /*reader*/, ID *id)
{
  /* ShapeKeys should always only be linked indirectly through their user ID (mesh, Curve etc.), or
   * be fully local data. */
  BLI_assert((id->tag & ID_TAG_EXTERN) == 0);
  UNUSED_VARS_NDEBUG(id);
}

IDTypeInfo IDType_ID_KE = {
    /*id_code*/ Key::id_type,
    /*id_filter*/ FILTER_ID_KE,
    /* Warning! key->from, could be more types in future? */
    /*dependencies_id_types*/ FILTER_ID_ME | FILTER_ID_CU_LEGACY | FILTER_ID_LT,
    /*main_listbase_index*/ INDEX_ID_KE,
    /*struct_size*/ sizeof(Key),
    /*name*/ "Key",
    /*name_plural*/ N_("shape_keys"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_SHAPEKEY,
    /*flags*/ IDTYPE_FLAGS_NO_LIBLINKING,
    /*asset_type_info*/ nullptr,

    /*init_data*/ nullptr,
    /*copy_data*/ shapekey_copy_data,
    /*free_data*/ shapekey_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ shapekey_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
    /* A bit weird, due to shape-keys not being strictly speaking embedded data... But they also
     * share a lot with those (non linkable, only ever used by one owner ID, etc.). */
    /*owner_pointer_get*/ shapekey_owner_pointer_get,

    /*blend_write*/ shapekey_blend_write,
    /*blend_read_data*/ shapekey_blend_read_data,
    /*blend_read_after_liblink*/ shapekey_blend_read_after_liblink,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

#define KEY_MODE_DUMMY 0 /* Use where mode isn't checked for. */
#define KEY_MODE_BPOINT 1
#define KEY_MODE_BEZTRIPLE 2

/* Internal use only. */
struct WeightsArrayCache {
  int num_defgroup_weights;
  float **defgroup_weights;
};

void BKE_key_free_nolib(Key *key)
{
  while (KeyBlock *kb = static_cast<KeyBlock *>(BLI_pophead(&key->block))) {
    if (kb->data) {
      MEM_delete_void(kb->data);
    }
    MEM_delete(kb);
  }
}

Key *BKE_key_add(Main *bmain, ID *id) /* Common function. */
{
  Key *key = BKE_id_new<Key>(bmain, "Key");

  key->type = KEY_NORMAL;
  key->from = id;

  key->uidgen = 1;

  char *el;
  /* XXX The code here uses some defines which will soon be deprecated... (comment probably Joshua
   * in 2009). */
  switch (GS(id->name)) {
    case ID_ME:
      el = key->elemstr;

      el[0] = KEYELEM_FLOAT_LEN_COORD;
      el[1] = IPO_FLOAT;
      el[2] = 0;

      key->elemsize = sizeof(float[KEYELEM_FLOAT_LEN_COORD]);

      break;
    case ID_LT:
      el = key->elemstr;

      el[0] = KEYELEM_FLOAT_LEN_COORD;
      el[1] = IPO_FLOAT;
      el[2] = 0;

      key->elemsize = sizeof(float[KEYELEM_FLOAT_LEN_COORD]);

      break;
    case ID_CU_LEGACY:
      el = key->elemstr;

      el[0] = KEYELEM_ELEM_SIZE_CURVE;
      el[1] = IPO_BPOINT;
      el[2] = 0;

      key->elemsize = sizeof(float[KEYELEM_ELEM_SIZE_CURVE]);

      break;

    default:
      break;
  }

  return key;
}

void BKE_key_sort(Key *key)
{
  KeyBlock *kb;

  /* Locate the key which is out of position. */
  for (kb = static_cast<KeyBlock *>(key->block.first); kb; kb = kb->next) {
    if ((kb->next) && (kb->pos > kb->next->pos)) {
      break;
    }
  }

  /* If we find a key, move it. */
  if (kb) {
    kb = kb->next; /* next key is the out-of-order one */
    BLI_remlink(&key->block, kb);

    /* Find the right location and insert before. */
    for (KeyBlock &kb2 : key->block) {
      if (kb2.pos > kb->pos) {
        BLI_insertlinkafter(&key->block, kb2.prev, kb);
        break;
      }
    }
  }

  /* New rule; first key is refkey, this to match drawing channels... */
  key->refkey = static_cast<KeyBlock *>(key->block.first);
}

/**************** do the key ****************/

void key_curve_position_weights(float t, float data[4], KeyInterpolationType type)
{
  float t2, t3, fc;

  switch (type) {
    case KEY_LINEAR:
      data[0] = 0.0f;
      data[1] = -t + 1.0f;
      data[2] = t;
      data[3] = 0.0f;
      break;
    case KEY_CARDINAL:
      t2 = t * t;
      t3 = t2 * t;
      fc = 0.71f;

      data[0] = -fc * t3 + 2.0f * fc * t2 - fc * t;
      data[1] = (2.0f - fc) * t3 + (fc - 3.0f) * t2 + 1.0f;
      data[2] = (fc - 2.0f) * t3 + (3.0f - 2.0f * fc) * t2 + fc * t;
      data[3] = fc * t3 - fc * t2;
      break;
    case KEY_BSPLINE:
      t2 = t * t;
      t3 = t2 * t;

      data[0] = -0.16666666f * t3 + 0.5f * t2 - 0.5f * t + 0.16666666f;
      data[1] = 0.5f * t3 - t2 + 0.66666666f;
      data[2] = -0.5f * t3 + 0.5f * t2 + 0.5f * t + 0.16666666f;
      data[3] = 0.16666666f * t3;
      break;
    case KEY_CATMULL_ROM:
      t2 = t * t;
      t3 = t2 * t;
      fc = 0.5f;

      data[0] = -fc * t3 + 2.0f * fc * t2 - fc * t;
      data[1] = (2.0f - fc) * t3 + (fc - 3.0f) * t2 + 1.0f;
      data[2] = (fc - 2.0f) * t3 + (3.0f - 2.0f * fc) * t2 + fc * t;
      data[3] = fc * t3 - fc * t2;
      break;
  }
}

void key_curve_tangent_weights(float t, float data[4], KeyInterpolationType type)
{
  float t2, fc;

  switch (type) {
    case KEY_LINEAR:
      data[0] = 0.0f;
      data[1] = -1.0f;
      data[2] = 1.0f;
      data[3] = 0.0f;
      break;
    case KEY_CARDINAL:
      t2 = t * t;
      fc = 0.71f;

      data[0] = -3.0f * fc * t2 + 4.0f * fc * t - fc;
      data[1] = 3.0f * (2.0f - fc) * t2 + 2.0f * (fc - 3.0f) * t;
      data[2] = 3.0f * (fc - 2.0f) * t2 + 2.0f * (3.0f - 2.0f * fc) * t + fc;
      data[3] = 3.0f * fc * t2 - 2.0f * fc * t;
      break;
    case KEY_BSPLINE:
      t2 = t * t;

      data[0] = -0.5f * t2 + t - 0.5f;
      data[1] = 1.5f * t2 - t * 2.0f;
      data[2] = -1.5f * t2 + t + 0.5f;
      data[3] = 0.5f * t2;
      break;
    case KEY_CATMULL_ROM:
      t2 = t * t;
      fc = 0.5f;

      data[0] = -3.0f * fc * t2 + 4.0f * fc * t - fc;
      data[1] = 3.0f * (2.0f - fc) * t2 + 2.0f * (fc - 3.0f) * t;
      data[2] = 3.0f * (fc - 2.0f) * t2 + 2.0f * (3.0f - 2.0f * fc) * t + fc;
      data[3] = 3.0f * fc * t2 - 2.0f * fc * t;
      break;
  }
}

void key_curve_normal_weights(const float t, float data[4], const KeyInterpolationType type)
{
  float fc;

  switch (type) {
    case KEY_LINEAR:
      data[0] = 0.0f;
      data[1] = 0.0f;
      data[2] = 0.0f;
      data[3] = 0.0f;
      break;
    case KEY_CARDINAL:
      fc = 0.71f;

      data[0] = -6.0f * fc * t + 4.0f * fc;
      data[1] = 6.0f * (2.0f - fc) * t + 2.0f * (fc - 3.0f);
      data[2] = 6.0f * (fc - 2.0f) * t + 2.0f * (3.0f - 2.0f * fc);
      data[3] = 6.0f * fc * t - 2.0f * fc;
      break;
    case KEY_BSPLINE:
      data[0] = -1.0f * t + 1.0f;
      data[1] = 3.0f * t - 2.0f;
      data[2] = -3.0f * t + 1.0f;
      data[3] = 1.0f * t;
      break;
    case KEY_CATMULL_ROM:
      fc = 0.5f;

      data[0] = -6.0f * fc * t + 4.0f * fc;
      data[1] = 6.0f * (2.0f - fc) * t + 2.0f * (fc - 3.0f);
      data[2] = 6.0f * (fc - 2.0f) * t + 2.0f * (3.0f - 2.0f * fc);
      data[3] = 6.0f * fc * t - 2.0f * fc;
      break;
  }
}

/**
 * Determine the keys to use for absolute shapekey evaluation.
 * The values in `r_weights` indicate how much of a given shapekey should be used.
 * This is done to support different interpolation modes.
 *
 * The key in `r_target_keys[2]` is the key just after the factor threshold.
 *
 * \return true means the key in `r_target_keys[2]` should be copied as is,
 * false means interpolate.
 */
static bool get_keys_for_absolute_eval(float eval_time,
                                       const ListBaseT<KeyBlock> *keyblocks,
                                       KeyBlock *r_target_keys[4],
                                       float r_weights[4])
{
  KeyBlock *firstkey = static_cast<KeyBlock *>(keyblocks->first);
  KeyBlock *lastkey = static_cast<KeyBlock *>(keyblocks->last);
  eval_time = clamp_f(eval_time, firstkey->pos, lastkey->pos);

  r_target_keys[0] = r_target_keys[1] = r_target_keys[2] = r_target_keys[3] = firstkey;
  r_weights[0] = r_weights[1] = r_weights[2] = r_weights[3] = firstkey->pos;

  if (firstkey->next == nullptr) {
    /* There is only a single shapekey, we cannot do interpolation in this case. */
    return true;
  }

  r_target_keys[2] = firstkey->next;
  r_weights[2] = r_target_keys[2]->pos;
  r_target_keys[3] = r_target_keys[2]->next;
  if (r_target_keys[3] == nullptr) {
    r_target_keys[3] = r_target_keys[2];
  }
  r_weights[3] = r_target_keys[3]->pos;
  KeyBlock *key_iter = r_target_keys[3];

  /* Find the correct shapekeys. */
  while (r_weights[2] < eval_time) {
    if (key_iter->next == nullptr) {
      /* This triggers when `key_iter->next` has been a nullptr for one loop. */
      if (r_weights[2] == r_weights[3]) {
        break;
      }
    }
    else {
      key_iter = key_iter->next;
    }

    r_weights[0] = r_weights[1];
    r_target_keys[0] = r_target_keys[1];
    r_weights[1] = r_weights[2];
    r_target_keys[1] = r_target_keys[2];
    r_weights[2] = r_weights[3];
    r_target_keys[2] = r_target_keys[3];
    r_weights[3] = key_iter->pos;
    r_target_keys[3] = key_iter;
  }

  bool bsplinetype = false;
  if (r_target_keys[1]->type == KEY_BSPLINE || r_target_keys[2]->type == KEY_BSPLINE) {
    bsplinetype = true;
  }

  if (bsplinetype == false) { /* B spline doesn't go through the control points. */
    if (eval_time <= r_weights[1]) {
      /* This can happen if there are only 2 shapekeys. */
      r_weights[2] = r_weights[1];
      r_target_keys[2] = r_target_keys[1];
      return true;
    }
    if (eval_time >= r_weights[2]) { /* `eval_time` after 2nd key. */
      return true;
    }
  }
  else if (eval_time > r_weights[2]) { /* Last key. */
    eval_time = r_weights[2];
    r_target_keys[3] = r_target_keys[2];
    r_weights[3] = r_weights[2];
  }

  float delta = r_weights[2] - r_weights[1];
  if (delta == 0.0f) {
    if (bsplinetype == false) {
      return true; /* Both keys equal. */
    }
  }
  else {
    delta = (eval_time - r_weights[1]) / delta;
  }

  /* Interpolation. */
  key_curve_position_weights(delta, r_weights, KeyInterpolationType(r_target_keys[1]->type));

  if (r_target_keys[1]->type != r_target_keys[2]->type) {
    float t_other[4];
    key_curve_position_weights(delta, t_other, KeyInterpolationType(r_target_keys[2]->type));
    interp_v4_v4v4(r_weights, r_weights, t_other, delta);
  }

  return false;
}

static char *key_block_get_data(Key *key, KeyBlock *actkb, KeyBlock *kb, char **freedata)
{
  if (kb == actkb) {
    /* This hack makes it possible to edit shape keys in
     * edit mode with shape keys blending applied. */
    if (GS(key->from->name) == ID_ME) {

      Mesh *mesh = id_cast<Mesh *>(key->from);

      if (mesh->runtime->edit_mesh && mesh->runtime->edit_mesh->bm->totvert == kb->totelem) {
        int a = 0;
        float (*co)[3];
        co = MEM_new_array_uninitialized<float[3]>(size_t(mesh->runtime->edit_mesh->bm->totvert),
                                                   "key_block_get_data");

        BMVert *eve;
        BMIter iter;
        BM_ITER_MESH (eve, &iter, mesh->runtime->edit_mesh->bm, BM_VERTS_OF_MESH) {
          copy_v3_v3(co[a], eve->co);
          a++;
        }

        *freedata = reinterpret_cast<char *>(co);
        return reinterpret_cast<char *>(co);
      }
    }
  }

  *freedata = nullptr;
  return static_cast<char *>(kb->data);
}

/**
 * Move the point in `r_targets` along the vector of ab by a factor of `weight`.
 *
 * \param start_index points to the x value in the flat float array. Indices of +1 and +2 from this
 * are accessed.
 */
static void add_weighted_vector(
    const int start_index, const float weight, const float *a, const float *b, float *r_target)
{
  r_target[start_index + 0] += weight * (b[start_index + 0] - a[start_index + 0]);
  r_target[start_index + 1] += weight * (b[start_index + 1] - a[start_index + 1]);
  r_target[start_index + 2] += weight * (b[start_index + 2] - a[start_index + 2]);
}

/**
 * Blend the given float3 arrays f1-3 into `r_out` with the given weights.
 */
static void flerp(const float f0[3],
                  const float f1[3],
                  const float f2[3],
                  const float f3[3],
                  const float weights[4],
                  float r_out[3])
{
  r_out[0] = weights[0] * f0[0] + weights[1] * f1[0] + weights[2] * f2[0] + weights[3] * f3[0];
  r_out[1] = weights[0] * f0[1] + weights[1] * f1[1] + weights[2] * f2[1] + weights[3] * f3[1];
  r_out[2] = weights[0] * f0[2] + weights[1] * f1[2] + weights[2] * f2[2] + weights[3] * f3[2];
}

/**
 * Copy the shapekey data of `source` into the output array of `r_target`.
 */
static void copy_key_float3(
    const int vertex_count, Key *key, KeyBlock *active_keyblock, KeyBlock *source, float *r_target)
{
  if (vertex_count == 0 || source->totelem == 0) {
    return;
  }
  char *free_keyblock_data;
  float *keyblock_data = reinterpret_cast<float *>(
      key_block_get_data(key, active_keyblock, source, &free_keyblock_data));

  if (vertex_count == source->totelem) {
    memcpy(r_target, keyblock_data, vertex_count * 3 * sizeof(float));
  }
  else {
    /* In case of KeyBlocks that have a different number of elements than the original data.
     * Maintained for backwards compatibility even though this state should not be reachable
     * through normal interactions with Blender. */
    const float step_rate = source->totelem / float(vertex_count);
    for (int i = 0; i < vertex_count; i++) {
      /* Rounding down to avoid exceeding the bounds. */
      const int source_index = int(step_rate * i);
      memcpy(&r_target[i * 3], &keyblock_data[source_index * 3], 3 * sizeof(float));
    }
  }

  if (free_keyblock_data) {
    MEM_delete(free_keyblock_data);
  }
}

/**
 * Copy the shapekey data of `source` into the output array of `r_target`.
 *
 * \param weights is a float array of size `vertex_count`. It determines how much of `source` is
 * blended into the result. The base for it is the reference key. If this is passed as a nullptr,
 * `source` is copied at full weight.
 */
static void copy_key_float3_weighted(const int vertex_count,
                                     Key *key,
                                     KeyBlock *active_keyblock,
                                     KeyBlock *source,
                                     const float *weights,
                                     float *r_target)
{
  if (!weights) {
    copy_key_float3(vertex_count, key, active_keyblock, source, r_target);
    return;
  }

  if (vertex_count != source->totelem) {
    /* There was a system in place before that worked with keys that only have partial data. I
     * (christoph) removed that since there is no known case for that. */
    BLI_assert_unreachable();
    return;
  }

  char *free_source_data, *free_refkey_data;
  const float *source_data = reinterpret_cast<float *>(
      key_block_get_data(key, active_keyblock, source, &free_source_data));
  const float *refkey_data = reinterpret_cast<float *>(
      key_block_get_data(key, active_keyblock, key->refkey, &free_refkey_data));

  for (int i = 0; i < vertex_count; i++) {
    const int vector_index = i * 3;
    memcpy(&r_target[vector_index], &refkey_data[vector_index], 3 * sizeof(float));
    if (weights[i] != 0.0f) {
      add_weighted_vector(vector_index, weights[i], refkey_data, source_data, r_target);
    }
  }

  if (free_source_data) {
    MEM_delete(free_source_data);
  }
  if (free_refkey_data) {
    MEM_delete(free_refkey_data);
  }
}

/**
 * Shapekey evaluation for data of 3 floats (Vector3).
 *
 * \param target_data is the float array into which the result of the evaluation is written.
 * \param per_keyblock_weights is a 2d array which gives a per KeyBlock per Vertex weight. Can be a
 * nullptr.
 */
static void key_evaluate_relative_float3(Key *key,
                                         KeyBlock *active_keyblock,
                                         const int vertex_count,
                                         float **per_keyblock_weights,
                                         float *target_data)
{
  /* Creates the basis values of the reference key in target_data. */
  copy_key_float3(vertex_count, key, active_keyblock, key->refkey, target_data);

  /* Cannot use auto [keyblock_index, kb] here because that would throw a warning at the
   * parallel_for. */
  for (std::pair<int, KeyBlock &> enumerator : key->block.enumerate()) {
    KeyBlock &kb = enumerator.second;
    if (&kb == key->refkey) {
      continue;
    }
    /* No difference in vertex count allowed. */
    if (kb.flag & KEYBLOCK_MUTE || kb.totelem != vertex_count) {
      continue;
    }
    const float kb_influence = kb.curval;
    if (kb_influence == 0.0f) {
      continue;
    }
    /* Reference can be any block. */
    KeyBlock *reference_kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, kb.relative));
    if (reference_kb == nullptr) {
      continue;
    }

    const float *weights = per_keyblock_weights ? per_keyblock_weights[enumerator.first] : nullptr;

    char *freefrom = nullptr;
    const float *from = reinterpret_cast<float *>(
        key_block_get_data(key, active_keyblock, &kb, &freefrom));

    /* For meshes, use the original values instead of the bmesh values to
     * maintain a constant offset. */
    const float *reffrom = static_cast<float *>(reference_kb->data);
    threading::parallel_for(IndexRange(vertex_count), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        const float weight = weights ? (weights[i] * kb.curval) : kb.curval;
        /* Each vertex has 3 floats. */
        const int vector_index = i * 3;
        add_weighted_vector(vector_index, weight, reffrom, from, target_data);
      }
    });

    if (freefrom) {
      MEM_delete(freefrom);
    }
  }
}

/**
 * Absolute interpolation between up to 4 shapekeys. The resulting data is stored in `r_target`.
 */
static void key_evaluate_absolute(const int vertex_count,
                                  Key *key,
                                  KeyBlock *active_keyblock,
                                  KeyBlock *shapekeys[4],
                                  const float weights[4],
                                  float *r_target)
{
  char *freek1, *freek2, *freek3, *freek4;
  float *k1 = reinterpret_cast<float *>(
      key_block_get_data(key, active_keyblock, shapekeys[0], &freek1));
  float *k2 = reinterpret_cast<float *>(
      key_block_get_data(key, active_keyblock, shapekeys[1], &freek2));
  float *k3 = reinterpret_cast<float *>(
      key_block_get_data(key, active_keyblock, shapekeys[2], &freek3));
  float *k4 = reinterpret_cast<float *>(
      key_block_get_data(key, active_keyblock, shapekeys[3], &freek4));

  /* The step rate will be 1 in normal cases. We have to account for shapekeys with different
   * element counts though since that case used to be supported. */
  const float step_k1 = shapekeys[0]->totelem / float(vertex_count);
  const float step_k2 = shapekeys[1]->totelem / float(vertex_count);
  const float step_k3 = shapekeys[2]->totelem / float(vertex_count);
  const float step_k4 = shapekeys[3]->totelem / float(vertex_count);

  threading::parallel_for(IndexRange(vertex_count), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      flerp(&k1[int(i * step_k1) * 3],
            &k2[int(i * step_k2) * 3],
            &k3[int(i * step_k3) * 3],
            &k4[int(i * step_k4) * 3],
            weights,
            &r_target[i * 3]);
    }
  });

  if (freek1) {
    MEM_delete(freek1);
  }
  if (freek2) {
    MEM_delete(freek2);
  }
  if (freek3) {
    MEM_delete(freek3);
  }
  if (freek4) {
    MEM_delete(freek4);
  }
}

static float *get_weights_array(Object *ob, const char *vgroup, WeightsArrayCache *cache)
{
  /* No vgroup string set? */
  if (vgroup[0] == 0) {
    return nullptr;
  }

  const MDeformVert *dvert = nullptr;
  int totvert = 0;

  /* Gather dvert and totvert. */
  BMEditMesh *em = nullptr;
  if (ob->type == OB_MESH) {
    Mesh *mesh = id_cast<Mesh *>(ob->data);
    dvert = mesh->deform_verts().data();
    totvert = mesh->verts_num;

    if (mesh->runtime->edit_mesh && mesh->runtime->edit_mesh->bm->totvert == totvert) {
      em = mesh->runtime->edit_mesh.get();
    }
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = id_cast<Lattice *>(ob->data);
    dvert = lt->dvert;
    totvert = lt->pntsu * lt->pntsv * lt->pntsw;
  }

  if (dvert == nullptr) {
    return nullptr;
  }

  /* Find the group (weak loop-in-loop). */
  const int defgrp_index = BKE_object_defgroup_name_index(ob, vgroup);
  if (defgrp_index != -1) {
    float *weights;

    if (cache) {
      if (cache->defgroup_weights == nullptr) {
        int num_defgroup = BKE_object_defgroup_count(ob);
        cache->defgroup_weights = MEM_new_array_zeroed<float *>(num_defgroup,
                                                                "cached defgroup weights");
        cache->num_defgroup_weights = num_defgroup;
      }

      if (cache->defgroup_weights[defgrp_index]) {
        return cache->defgroup_weights[defgrp_index];
      }
    }

    weights = MEM_new_array_uninitialized<float>(size_t(totvert), "weights");

    if (em) {
      int i;
      const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
      BMIter iter;
      BMVert *eve;
      BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, i) {
        dvert = static_cast<const MDeformVert *>(BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset));
        weights[i] = BKE_defvert_find_weight(dvert, defgrp_index);
      }
    }
    else {
      for (int i = 0; i < totvert; i++, dvert++) {
        weights[i] = BKE_defvert_find_weight(dvert, defgrp_index);
      }
    }

    if (cache) {
      cache->defgroup_weights[defgrp_index] = weights;
    }

    return weights;
  }
  return nullptr;
}

static float **keyblock_get_per_block_weights(Object *ob, Key *key, WeightsArrayCache *cache)
{
  float **per_keyblock_weights = MEM_new_array_uninitialized<float *>(size_t(key->totkey),
                                                                      "per keyblock weights");

  for (const auto [keyblock_index, keyblock] : key->block.enumerate()) {
    per_keyblock_weights[keyblock_index] = get_weights_array(ob, keyblock.vgroup, cache);
  }

  return per_keyblock_weights;
}

static void keyblock_free_per_block_weights(Key *key,
                                            float **per_keyblock_weights,
                                            WeightsArrayCache *cache)
{
  if (cache) {
    if (cache->num_defgroup_weights) {
      for (int a = 0; a < cache->num_defgroup_weights; a++) {
        if (cache->defgroup_weights[a]) {
          MEM_delete(cache->defgroup_weights[a]);
        }
      }
      MEM_delete(cache->defgroup_weights);
    }
    cache->defgroup_weights = nullptr;
  }
  else {
    for (int a = 0; a < key->totkey; a++) {
      if (per_keyblock_weights[a]) {
        MEM_delete(per_keyblock_weights[a]);
      }
    }
  }

  MEM_delete(per_keyblock_weights);
}

static void do_mesh_key(Object *ob, Key *key, char *out, const int tot)
{
  KeyBlock *actkb = BKE_keyblock_from_object(ob);

  if (key->type == KEY_RELATIVE) {
    WeightsArrayCache cache = {0, nullptr};
    float **per_keyblock_weights;
    per_keyblock_weights = keyblock_get_per_block_weights(ob, key, &cache);
    key_evaluate_relative_float3(
        key, actkb, tot, per_keyblock_weights, reinterpret_cast<float *>(out));
    keyblock_free_per_block_weights(key, per_keyblock_weights, &cache);
  }
  else {
    const float ctime_scaled = key->ctime / 100.0f;
    KeyBlock *shapekeys[4];
    float weights[4];
    const bool simple_copy = get_keys_for_absolute_eval(
        ctime_scaled, &key->block, shapekeys, weights);

    if (simple_copy == false) {
      key_evaluate_absolute(tot, key, actkb, shapekeys, weights, reinterpret_cast<float *>(out));
    }
    else {
      copy_key_float3(tot, key, actkb, shapekeys[2], reinterpret_cast<float *>(out));
    }
  }
}

static void do_curve_key(Object *ob, Key *key, char *out, const int tot)
{
  KeyBlock *actkb = BKE_keyblock_from_object(ob);

  if (key->type == KEY_RELATIVE) {
    key_evaluate_relative_float3(key, actkb, tot, nullptr, reinterpret_cast<float *>(out));
  }
  else {
    const float ctime_scaled = key->ctime / 100.0f;
    KeyBlock *shapekeys[4];
    float weights[4];
    const bool simple_copy = get_keys_for_absolute_eval(
        ctime_scaled, &key->block, shapekeys, weights);

    if (simple_copy == false) {
      key_evaluate_absolute(tot, key, actkb, shapekeys, weights, reinterpret_cast<float *>(out));
    }
    else {
      copy_key_float3(tot, key, actkb, shapekeys[2], reinterpret_cast<float *>(out));
    }
  }
}

static void do_latt_key(Object *ob, Key *key, char *out, const int tot)
{
  Lattice *lt = id_cast<Lattice *>(ob->data);
  KeyBlock *actkb = BKE_keyblock_from_object(ob);

  if (key->type == KEY_RELATIVE) {
    float **per_keyblock_weights;
    per_keyblock_weights = keyblock_get_per_block_weights(ob, key, nullptr);
    key_evaluate_relative_float3(
        key, actkb, tot, per_keyblock_weights, reinterpret_cast<float *>(out));
    keyblock_free_per_block_weights(key, per_keyblock_weights, nullptr);
  }
  else {
    const float ctime_scaled = key->ctime / 100.0f;
    KeyBlock *shapekeys[4];
    float weights[4];
    const bool simple_copy = get_keys_for_absolute_eval(
        ctime_scaled, &key->block, shapekeys, weights);

    if (simple_copy == false) {
      key_evaluate_absolute(tot, key, actkb, shapekeys, weights, reinterpret_cast<float *>(out));
    }
    else {
      copy_key_float3(tot, key, actkb, shapekeys[2], reinterpret_cast<float *>(out));
    }
  }

  if (lt->flag & LT_OUTSIDE) {
    outside_lattice(lt);
  }
}

static void keyblock_data_convert_to_lattice(const float (*fp)[3],
                                             BPoint *bpoint,
                                             const int totpoint);
static void keyblock_data_convert_to_curve(const float *fp,
                                           ListBaseT<Nurb> *nurb,
                                           const int totpoint);

float *BKE_key_evaluate_object_ex(
    Object *ob, int *r_totelem, float *arr, size_t arr_size, ID *obdata)
{
  Key *key = BKE_key_from_object(ob);
  KeyBlock *actkb = BKE_keyblock_from_object(ob);

  if (key == nullptr || BLI_listbase_is_empty(&key->block)) {
    return nullptr;
  }

  /* Compute size of output array. */
  int tot = 0, size = 0;
  if (ob->type == OB_MESH) {
    Mesh *mesh = id_cast<Mesh *>(ob->data);

    tot = mesh->verts_num;
    size = tot * sizeof(float[KEYELEM_FLOAT_LEN_COORD]);
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = id_cast<Lattice *>(ob->data);

    tot = lt->pntsu * lt->pntsv * lt->pntsw;
    size = tot * sizeof(float[KEYELEM_FLOAT_LEN_COORD]);
  }
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
    Curve *cu = id_cast<Curve *>(ob->data);

    tot = BKE_keyblock_curve_element_count(&cu->nurb);
    size = tot * sizeof(float[KEYELEM_ELEM_SIZE_CURVE]);
  }

  /* If nothing to interpolate, cancel. */
  if (tot == 0 || size == 0) {
    return nullptr;
  }

  /* Allocate array. */
  char *out;
  if (arr == nullptr) {
    out = MEM_new_array_zeroed<char>(size, "BKE_key_evaluate_object out");
  }
  else {
    if (arr_size != size) {
      return nullptr;
    }

    out = reinterpret_cast<char *>(arr);
  }

  if (ob->shapeflag & OB_SHAPE_LOCK) {
    /* Shape locked, copy the locked shape instead of blending. */
    KeyBlock *kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, ob->shapenr - 1));

    if (kb && (kb->flag & KEYBLOCK_MUTE)) {
      kb = key->refkey;
    }

    if (kb == nullptr) {
      kb = static_cast<KeyBlock *>(key->block.first);
      ob->shapenr = 1;
    }

    if (OB_TYPE_SUPPORT_VGROUP(ob->type)) {
      const float *weights = get_weights_array(ob, kb->vgroup, nullptr);
      copy_key_float3_weighted(tot, key, actkb, kb, weights, reinterpret_cast<float *>(out));

      if (weights) {
        MEM_delete(weights);
      }
    }
    else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
      copy_key_float3(tot, key, actkb, kb, reinterpret_cast<float *>(out));
    }
  }
  else {
    if (ob->type == OB_MESH) {
      do_mesh_key(ob, key, out, tot);
    }
    else if (ob->type == OB_LATTICE) {
      do_latt_key(ob, key, out, tot);
    }
    else if (ob->type == OB_CURVES_LEGACY) {
      do_curve_key(ob, key, out, tot);
    }
    else if (ob->type == OB_SURF) {
      do_curve_key(ob, key, out, tot);
    }
  }

  if (obdata != nullptr) {
    switch (GS(obdata->name)) {
      case ID_ME: {
        Mesh *mesh = id_cast<Mesh *>(obdata);
        const int totvert = min_ii(tot, mesh->verts_num);
        mesh->vert_positions_for_write().take_front(totvert).copy_from(
            {reinterpret_cast<const float3 *>(out), totvert});
        mesh->tag_positions_changed();
        break;
      }
      case ID_LT: {
        Lattice *lattice = id_cast<Lattice *>(obdata);
        const int totpoint = min_ii(tot, lattice->pntsu * lattice->pntsv * lattice->pntsw);
        keyblock_data_convert_to_lattice(
            reinterpret_cast<const float (*)[3]>(out), lattice->def, totpoint);
        break;
      }
      case ID_CU_LEGACY: {
        Curve *curve = id_cast<Curve *>(obdata);
        const int totpoint = min_ii(tot, BKE_keyblock_curve_element_count(&curve->nurb));
        keyblock_data_convert_to_curve(
            reinterpret_cast<const float *>(out), &curve->nurb, totpoint);
        break;
      }
      default:
        BLI_assert_unreachable();
    }
  }

  if (r_totelem) {
    *r_totelem = tot;
  }
  return reinterpret_cast<float *>(out);
}

float *BKE_key_evaluate_object(Object *ob, int *r_totelem)
{
  return BKE_key_evaluate_object_ex(ob, r_totelem, nullptr, 0, nullptr);
}

int BKE_keyblock_element_count_from_shape(const Key *key, const int shape_index)
{
  int result = 0;

  for (const auto [index, kb] : key->block.enumerate()) {
    if (ELEM(shape_index, -1, index)) {
      result += kb.totelem;
    }
  }
  return result;
}

int BKE_keyblock_element_count(const Key *key)
{
  return BKE_keyblock_element_count_from_shape(key, -1);
}

size_t BKE_keyblock_element_calc_size_from_shape(const Key *key, const int shape_index)
{
  return size_t(BKE_keyblock_element_count_from_shape(key, shape_index)) * key->elemsize;
}

size_t BKE_keyblock_element_calc_size(const Key *key)
{
  return BKE_keyblock_element_calc_size_from_shape(key, -1);
}

/* -------------------------------------------------------------------- */
/** \name Key-Block Data Access
 *
 * Utilities for getting/setting key data as a single array,
 * use #BKE_keyblock_element_calc_size to allocate the size of the data needed.
 * \{ */

void BKE_keyblock_data_get_from_shape(const Key *key,
                                      MutableSpan<float3> arr,
                                      const int shape_index)
{
  uint8_t *elements = reinterpret_cast<uint8_t *>(arr.data());

  for (const auto [index, kb] : key->block.enumerate()) {
    if (ELEM(shape_index, -1, index)) {
      const int block_elem_len = kb.totelem * key->elemsize;
      memcpy(elements, kb.data, block_elem_len);
      elements += block_elem_len;
    }
  }
}

void BKE_keyblock_data_get(const Key *key, MutableSpan<float3> arr)
{
  BKE_keyblock_data_get_from_shape(key, arr, -1);
}

void BKE_keyblock_data_set_with_mat4(Key *key,
                                     const int shape_index,
                                     const Span<float3> coords,
                                     const float4x4 &transform)
{
  if (key->elemsize != sizeof(float[3])) {
    BLI_assert_msg(0, "Invalid elemsize");
    return;
  }

  const float3 *elements = coords.data();

  for (const auto [index, kb] : key->block.enumerate()) {
    if (ELEM(shape_index, -1, index)) {
      const int block_elem_len = kb.totelem;
      float (*block_data)[3] = static_cast<float (*)[3]>(kb.data);
      for (int data_offset = 0; data_offset < block_elem_len; ++data_offset) {
        const float *src_data = reinterpret_cast<const float *>(elements + data_offset);
        float *dst_data = reinterpret_cast<float *>(block_data + data_offset);
        mul_v3_m4v3(dst_data, transform.ptr(), src_data);
      }
      elements += block_elem_len;
    }
  }
}

void BKE_keyblock_curve_data_set_with_mat4(Key *key,
                                           const ListBaseT<Nurb> *nurb,
                                           const int shape_index,
                                           const void *data,
                                           const float4x4 &transform)
{
  const uint8_t *elements = static_cast<const uint8_t *>(data);

  for (const auto [index, kb] : key->block.enumerate()) {
    if (ELEM(shape_index, -1, index)) {
      const int block_elem_size = kb.totelem * key->elemsize;
      BKE_keyblock_curve_data_transform(nurb, transform.ptr(), elements, kb.data);
      elements += block_elem_size;
    }
  }
}

void BKE_keyblock_data_set(Key *key, const int shape_index, const void *data)
{
  const uint8_t *elements = static_cast<const uint8_t *>(data);

  for (const auto [index, kb] : key->block.enumerate()) {
    if (ELEM(shape_index, -1, index)) {
      const int block_elem_size = kb.totelem * key->elemsize;
      memcpy(kb.data, elements, block_elem_size);
      elements += block_elem_size;
    }
  }
}

/** \} */

bool BKE_key_idtype_support(const short id_type)
{
  switch (id_type) {
    case ID_ME:
    case ID_CU_LEGACY:
    case ID_LT:
      return true;
    default:
      return false;
  }
}

Key **BKE_key_from_id_p(ID *id)
{
  switch (GS(id->name)) {
    case ID_ME: {
      Mesh *mesh = id_cast<Mesh *>(id);
      return &mesh->key;
    }
    case ID_CU_LEGACY: {
      Curve *cu = id_cast<Curve *>(id);
      if (cu->ob_type != OB_FONT) {
        return &cu->key;
      }
      break;
    }
    case ID_LT: {
      Lattice *lt = id_cast<Lattice *>(id);
      return &lt->key;
    }
    default:
      break;
  }

  return nullptr;
}

Key *BKE_key_from_id(ID *id)
{
  Key **key_p;
  key_p = BKE_key_from_id_p(id);
  if (key_p) {
    return *key_p;
  }

  return nullptr;
}

Key **BKE_key_from_object_p(Object *ob)
{
  if (ob == nullptr || ob->data == nullptr) {
    return nullptr;
  }

  return BKE_key_from_id_p(ob->data);
}

Key *BKE_key_from_object(Object *ob)
{
  Key **key_p;
  key_p = BKE_key_from_object_p(ob);
  if (key_p) {
    return *key_p;
  }

  return nullptr;
}

KeyBlock *BKE_keyblock_add(Key *key, const char *name)
{
  float curpos = -0.1;

  KeyBlock *kb = static_cast<KeyBlock *>(key->block.last);
  if (kb) {
    curpos = kb->pos;
  }

  kb = MEM_new<KeyBlock>("Keyblock");
  BLI_addtail(&key->block, kb);
  kb->type = KEY_LINEAR;

  const int tot = BLI_listbase_count(&key->block);
  if (name) {
    STRNCPY_UTF8(kb->name, name);
  }
  else {
    if (tot == 1) {
      STRNCPY_UTF8(kb->name, DATA_("Basis"));
    }
    else {
      SNPRINTF_UTF8(kb->name, DATA_("Key %d"), tot - 1);
    }
  }

  BLI_uniquename(&key->block, kb, DATA_("Key"), '.', offsetof(KeyBlock, name), sizeof(kb->name));

  kb->uid = key->uidgen++;

  key->totkey++;
  if (key->totkey == 1) {
    key->refkey = kb;
  }

  kb->slidermin = 0.0f;
  kb->slidermax = 1.0f;

  /* \note The caller may want to set this to current time, but don't do it here since we need to
   * sort which could cause problems in some cases, see #BKE_keyblock_add_ctime. */
  kb->pos = curpos + 0.1f; /* Only used for absolute shape keys. */

  return kb;
}

KeyBlock *BKE_keyblock_duplicate(Key *key, KeyBlock *kb_src)
{
  BLI_assert(BLI_findindex(&key->block, kb_src) != -1);
  KeyBlock *kb_dst = BKE_keyblock_add(key, kb_src->name);
  kb_dst->totelem = kb_src->totelem;
  kb_dst->data = MEM_dupalloc_void(kb_src->data);
  BLI_remlink(&key->block, kb_dst);
  BLI_insertlinkafter(&key->block, kb_src, kb_dst);
  BKE_keyblock_copy_settings(kb_dst, kb_src);
  kb_dst->flag = kb_src->flag;
  return kb_dst;
}

KeyBlock *BKE_keyblock_add_ctime(Key *key, const char *name, const bool do_force)
{
  KeyBlock *kb = BKE_keyblock_add(key, name);
  const float cpos = key->ctime / 100.0f;

  /* In case of absolute keys, there is no point in adding more than one key with the same pos.
   * Hence only set new key-block pos to current time if none previous one already use it.
   * Now at least people just adding absolute keys without touching to ctime
   * won't have to systematically use retiming func (and have ordering issues, too). See #39897.
   */
  if (!do_force && (key->type != KEY_RELATIVE)) {
    for (KeyBlock &it_kb : key->block) {
      /* Use epsilon to avoid floating point precision issues.
       * 1e-3 because the position is stored as frame * 1e-2. */
      if (compare_ff(it_kb.pos, cpos, 1e-3f)) {
        return kb;
      }
    }
  }
  if (do_force || (key->type != KEY_RELATIVE)) {
    kb->pos = cpos;
    BKE_key_sort(key);
  }

  return kb;
}

KeyBlock *BKE_keyblock_from_object(Object *ob)
{
  Key *key = BKE_key_from_object(ob);

  return BKE_keyblock_find_by_index(key, ob->shapenr - 1);
}

KeyBlock *BKE_keyblock_from_object_reference(Object *ob)
{
  Key *key = BKE_key_from_object(ob);

  if (key) {
    return key->refkey;
  }

  return nullptr;
}

KeyBlock *BKE_keyblock_find_by_index(Key *key, int index)
{
  if (!key) {
    return nullptr;
  }

  return static_cast<KeyBlock *>(BLI_findlink(&key->block, index));
}

KeyBlock *BKE_keyblock_find_name(Key *key, const char name[])
{
  return static_cast<KeyBlock *>(BLI_findstring(&key->block, name, offsetof(KeyBlock, name)));
}

KeyBlock *BKE_keyblock_find_uid(Key *key, const int uid)
{
  for (KeyBlock &kb : key->block) {
    if (kb.uid == uid) {
      return &kb;
    }
  }
  return nullptr;
}

void BKE_keyblock_copy_settings(KeyBlock *kb_dst, const KeyBlock *kb_src)
{
  kb_dst->pos = kb_src->pos;
  kb_dst->curval = kb_src->curval;
  kb_dst->type = kb_src->type;
  kb_dst->relative = kb_src->relative;
  STRNCPY(kb_dst->vgroup, kb_src->vgroup);
  kb_dst->slidermin = kb_src->slidermin;
  kb_dst->slidermax = kb_src->slidermax;
}

std::optional<std::string> BKE_keyblock_curval_rnapath_get(const Key *key, const KeyBlock *kb)
{
  if (ELEM(nullptr, key, kb)) {
    return std::nullopt;
  }
  PointerRNA ptr = RNA_pointer_create_discrete(
      const_cast<ID *>(&key->id), RNA_ShapeKey, (KeyBlock *)kb);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "value");
  return RNA_path_from_ID_to_property(&ptr, prop);
}

/* conversion functions */

/************************* Lattice ************************/

void BKE_keyblock_update_from_lattice(const Lattice *lt, KeyBlock *kb)
{

  BLI_assert(kb->totelem == lt->pntsu * lt->pntsv * lt->pntsw);

  const int tot = kb->totelem;
  if (tot == 0) {
    return;
  }

  BPoint *bp = lt->def;
  float (*fp)[3];
  fp = static_cast<float (*)[3]>(kb->data);
  for (int a = 0; a < kb->totelem; a++, fp++, bp++) {
    copy_v3_v3(*fp, bp->vec);
  }
}

void BKE_keyblock_convert_from_lattice(const Lattice *lt, KeyBlock *kb)
{
  const int tot = lt->pntsu * lt->pntsv * lt->pntsw;
  if (tot == 0) {
    return;
  }

  MEM_SAFE_DELETE_VOID(kb->data);

  kb->data = MEM_new_array_uninitialized(size_t(tot), size_t(lt->key->elemsize), __func__);
  kb->totelem = tot;

  BKE_keyblock_update_from_lattice(lt, kb);
}

static void keyblock_data_convert_to_lattice(const float (*fp)[3],
                                             BPoint *bpoint,
                                             const int totpoint)
{
  for (int i = 0; i < totpoint; i++, fp++, bpoint++) {
    copy_v3_v3(bpoint->vec, *fp);
  }
}

void BKE_keyblock_convert_to_lattice(const KeyBlock *kb, Lattice *lt)
{
  BPoint *bp = lt->def;
  const float (*fp)[3] = static_cast<const float (*)[3]>(kb->data);
  const int tot = min_ii(kb->totelem, lt->pntsu * lt->pntsv * lt->pntsw);

  keyblock_data_convert_to_lattice(fp, bp, tot);
}

/************************* Curve ************************/

int BKE_keyblock_curve_element_count(const ListBaseT<Nurb> *nurb)
{
  const Nurb *nu;
  int tot = 0;

  nu = static_cast<const Nurb *>(nurb->first);
  while (nu) {
    if (nu->bezt) {
      tot += KEYELEM_ELEM_LEN_BEZTRIPLE * nu->pntsu;
    }
    else if (nu->bp) {
      tot += KEYELEM_ELEM_LEN_BPOINT * nu->pntsu * nu->pntsv;
    }

    nu = nu->next;
  }
  return tot;
}

void BKE_keyblock_update_from_curve(const Curve * /*cu*/,
                                    KeyBlock *kb,
                                    const ListBaseT<Nurb> *nurb)
{
  BLI_assert(BKE_keyblock_curve_element_count(nurb) == kb->totelem);

  const int tot = kb->totelem;
  if (tot == 0) {
    return;
  }

  BezTriple *bezt;
  BPoint *bp;
  int a;
  float *fp = static_cast<float *>(kb->data);
  for (Nurb &nu : *nurb) {
    if (nu.bezt) {
      for (a = nu.pntsu, bezt = nu.bezt; a; a--, bezt++) {
        for (int i = 0; i < 3; i++) {
          copy_v3_v3(&fp[i * 3], bezt->vec[i]);
        }
        fp[9] = bezt->tilt;
        fp[10] = bezt->radius;
        fp += KEYELEM_FLOAT_LEN_BEZTRIPLE;
      }
    }
    else {
      for (a = nu.pntsu * nu.pntsv, bp = nu.bp; a; a--, bp++) {
        copy_v3_v3(fp, bp->vec);
        fp[3] = bp->tilt;
        fp[4] = bp->radius;
        fp += KEYELEM_FLOAT_LEN_BPOINT;
      }
    }
  }
}

void BKE_keyblock_curve_data_transform(const ListBaseT<Nurb> *nurb,
                                       const float mat[4][4],
                                       const void *src_data,
                                       void *dst_data)
{
  const float *src = static_cast<const float *>(src_data);
  float *dst = static_cast<float *>(dst_data);
  for (Nurb &nu : *nurb) {
    if (nu.bezt) {
      for (int a = nu.pntsu; a; a--) {
        for (int i = 0; i < 3; i++) {
          mul_v3_m4v3(&dst[i * 3], mat, &src[i * 3]);
        }
        dst[9] = src[9];
        dst[10] = src[10];
        src += KEYELEM_FLOAT_LEN_BEZTRIPLE;
        dst += KEYELEM_FLOAT_LEN_BEZTRIPLE;
      }
    }
    else {
      for (int a = nu.pntsu * nu.pntsv; a; a--) {
        mul_v3_m4v3(dst, mat, src);
        dst[3] = src[3];
        dst[4] = src[4];
        src += KEYELEM_FLOAT_LEN_BPOINT;
        dst += KEYELEM_FLOAT_LEN_BPOINT;
      }
    }
  }
}

void BKE_keyblock_convert_from_curve(const Curve *cu, KeyBlock *kb, const ListBaseT<Nurb> *nurb)
{
  const int tot = BKE_keyblock_curve_element_count(nurb);
  if (tot == 0) {
    return;
  }

  MEM_SAFE_DELETE_VOID(kb->data);

  kb->data = MEM_new_array_uninitialized(size_t(tot), size_t(cu->key->elemsize), __func__);
  kb->totelem = tot;

  BKE_keyblock_update_from_curve(cu, kb, nurb);
}

static void keyblock_data_convert_to_curve(const float *fp, ListBaseT<Nurb> *nurb, int totpoint)
{
  for (Nurb *nu = static_cast<Nurb *>(nurb->first); nu && totpoint > 0; nu = nu->next) {
    if (nu->bezt != nullptr) {
      BezTriple *bezt = nu->bezt;
      for (int i = nu->pntsu; i && (totpoint -= KEYELEM_ELEM_LEN_BEZTRIPLE) >= 0;
           i--, bezt++, fp += KEYELEM_FLOAT_LEN_BEZTRIPLE)
      {
        for (int j = 0; j < 3; j++) {
          copy_v3_v3(bezt->vec[j], &fp[j * 3]);
        }
        bezt->tilt = fp[9];
        bezt->radius = fp[10];
      }
    }
    else {
      BPoint *bp = nu->bp;
      for (int i = nu->pntsu * nu->pntsv; i && (totpoint -= KEYELEM_ELEM_LEN_BPOINT) >= 0;
           i--, bp++, fp += KEYELEM_FLOAT_LEN_BPOINT)
      {
        copy_v3_v3(bp->vec, fp);
        bp->tilt = fp[3];
        bp->radius = fp[4];
      }
    }
  }
}

void BKE_keyblock_convert_to_curve(KeyBlock *kb, Curve * /*cu*/, ListBaseT<Nurb> *nurb)
{
  const float *fp = static_cast<const float *>(kb->data);
  const int tot = min_ii(kb->totelem, BKE_keyblock_curve_element_count(nurb));

  keyblock_data_convert_to_curve(fp, nurb, tot);
}

/************************* Mesh ************************/

void BKE_keyblock_update_from_mesh(const Mesh *mesh, KeyBlock *kb)
{
  BLI_assert(mesh->verts_num == kb->totelem);

  const int tot = mesh->verts_num;
  if (tot == 0) {
    return;
  }

  const Span<float3> positions = mesh->vert_positions();
  memcpy(kb->data, positions.data(), sizeof(float[3]) * tot);
}

void BKE_keyblock_convert_from_mesh(const Mesh *mesh, const Key *key, KeyBlock *kb)
{
  const int len = mesh->verts_num;

  if (mesh->verts_num == 0) {
    return;
  }

  MEM_SAFE_DELETE_VOID(kb->data);

  kb->data = MEM_new_array_uninitialized(size_t(len), size_t(key->elemsize), __func__);
  kb->totelem = len;

  BKE_keyblock_update_from_mesh(mesh, kb);
}

void BKE_keyblock_convert_to_mesh(const KeyBlock *kb, MutableSpan<float3> vert_positions)
{
  vert_positions.take_front(kb->totelem).copy_from({static_cast<float3 *>(kb->data), kb->totelem});
}

void BKE_keyblock_mesh_calc_normals(const KeyBlock *kb,
                                    Mesh *mesh,
                                    float (*r_vert_normals)[3],
                                    float (*r_face_normals)[3],
                                    float (*r_loop_normals)[3])
{
  using namespace blender::bke;
  if (r_vert_normals == nullptr && r_face_normals == nullptr && r_loop_normals == nullptr) {
    return;
  }

  Array<float3> positions(mesh->vert_positions());
  BKE_keyblock_convert_to_mesh(kb, positions);
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<int> corner_edges = mesh->corner_edges();

  const bool loop_normals_needed = r_loop_normals != nullptr;
  const bool vert_normals_needed = r_vert_normals != nullptr;
  const bool face_normals_needed = r_face_normals != nullptr || vert_normals_needed ||
                                   loop_normals_needed;

  float (*vert_normals)[3] = r_vert_normals;
  float (*face_normals)[3] = r_face_normals;
  bool free_vert_normals = false;
  bool free_face_normals = false;
  if (vert_normals_needed && r_vert_normals == nullptr) {
    vert_normals = MEM_new_array_uninitialized<float[3]>(size_t(mesh->verts_num), __func__);
    free_vert_normals = true;
  }
  if (face_normals_needed && r_face_normals == nullptr) {
    face_normals = MEM_new_array_uninitialized<float[3]>(size_t(mesh->faces_num), __func__);
    free_face_normals = true;
  }

  if (face_normals_needed) {
    mesh::normals_calc_faces(
        positions, faces, corner_verts, {reinterpret_cast<float3 *>(face_normals), faces.size()});
  }
  if (vert_normals_needed) {
    mesh::normals_calc_verts(positions,
                             faces,
                             corner_verts,
                             mesh->vert_to_face_map(),
                             {reinterpret_cast<const float3 *>(face_normals), faces.size()},
                             {reinterpret_cast<float3 *>(vert_normals), mesh->verts_num});
  }
  if (loop_normals_needed) {
    const AttributeAccessor attributes = mesh->attributes();
    const VArraySpan sharp_edges = *attributes.lookup<bool>("sharp_edge", AttrDomain::Edge);
    const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);
    const VArraySpan custom_normals = *attributes.lookup<short2>("custom_normal",
                                                                 AttrDomain::Corner);
    mesh::normals_calc_corners(positions,
                               faces,
                               corner_verts,
                               corner_edges,
                               mesh->vert_to_face_map(),
                               {reinterpret_cast<float3 *>(face_normals), faces.size()},
                               sharp_edges,
                               sharp_faces,
                               custom_normals,
                               nullptr,
                               {reinterpret_cast<float3 *>(r_loop_normals), corner_verts.size()});
  }

  if (free_vert_normals) {
    MEM_delete(vert_normals);
  }
  if (free_face_normals) {
    MEM_delete(face_normals);
  }
}

/************************* raw coords ************************/

bool BKE_keyblock_move(Object *ob, int org_index, int new_index)
{
  Key *key = BKE_key_from_object(ob);
  KeyBlock *kb;
  const int act_index = ob->shapenr - 1;
  const int totkey = key->totkey;
  int i;
  bool rev, in_range = false;

  if (org_index < 0) {
    org_index = act_index;
  }

  CLAMP(new_index, 0, key->totkey - 1);
  CLAMP(org_index, 0, key->totkey - 1);

  if (new_index == org_index) {
    return false;
  }

  rev = ((new_index - org_index) < 0) ? true : false;

  /* We swap 'org' element with its previous/next neighbor (depending on direction of the move)
   * repeatedly, until we reach final position.
   * This allows us to only loop on the list once! */
  for (kb = static_cast<KeyBlock *>(rev ? key->block.last : key->block.first),
      i = (rev ? totkey - 1 : 0);
       kb;
       kb = (rev ? kb->prev : kb->next), rev ? i-- : i++)
  {
    if (i == org_index) {
      in_range = true; /* Start list items swapping... */
    }
    else if (i == new_index) {
      in_range = false; /* End list items swapping. */
    }

    if (in_range) {
      KeyBlock *other_kb = rev ? kb->prev : kb->next;

      /* Swap with previous/next list item. */
      BLI_listbase_swaplinks(&key->block, kb, other_kb);

      /* Swap absolute positions. */
      std::swap(kb->pos, other_kb->pos);

      kb = other_kb;
    }

    /* Adjust relative indices, this has to be done on the whole list! */
    if (kb->relative == org_index) {
      kb->relative = new_index;
    }
    else if (kb->relative < org_index && kb->relative >= new_index) {
      /* Remove after, insert before this index. */
      kb->relative++;
    }
    else if (kb->relative > org_index && kb->relative <= new_index) {
      /* Remove before, insert after this index. */
      kb->relative--;
    }
  }

  /* Need to update active shape number if it's affected,
   * same principle as for relative indices above. */
  if (org_index == act_index) {
    ob->shapenr = new_index + 1;
  }
  else if (act_index < org_index && act_index >= new_index) {
    ob->shapenr++;
  }
  else if (act_index > org_index && act_index <= new_index) {
    ob->shapenr--;
  }

  /* First key is always refkey, matches interface and BKE_key_sort. */
  key->refkey = static_cast<KeyBlock *>(key->block.first);

  return true;
}

bool BKE_keyblock_is_basis(const Key *key, const int index)
{
  const KeyBlock *kb;
  int i;

  if (key->type == KEY_RELATIVE) {
    for (i = 0, kb = static_cast<const KeyBlock *>(key->block.first); kb; i++, kb = kb->next) {
      if ((i != index) && (kb->relative == index)) {
        return true;
      }
    }
  }

  return false;
}

std::optional<Array<bool>> BKE_keyblock_get_dependent_keys(const Key *key, const int index)
{
  if (key->type != KEY_RELATIVE) {
    return std::nullopt;
  }

  const int count = BLI_listbase_count(&key->block);

  if (index < 0 || index >= count) {
    return std::nullopt;
  }

  /* Seed the table with the specified key. */
  Array<bool> marked(count, false);

  marked[index] = true;

  /* Iterative breadth-first search through the key list. This method minimizes
   * the number of scans through the list and is fail-safe vs reference cycles. */
  bool updated, found = false;

  do {
    updated = false;

    for (const auto [i, kb] : key->block.enumerate()) {
      if (!marked[i] && kb.relative >= 0 && kb.relative < count && marked[kb.relative]) {
        marked[i] = true;
        updated = found = true;
      }
    }
  } while (updated);

  if (!found) {
    return std::nullopt;
  }

  /* After the search is complete, exclude the original key. */
  marked[index] = false;
  return marked;
}

}  // namespace blender
