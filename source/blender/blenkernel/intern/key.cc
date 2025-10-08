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

using blender::float3;
using blender::float4x4;
using blender::MutableSpan;
using blender::Span;

static void shapekey_copy_data(Main * /*bmain*/,
                               std::optional<Library *> /*owner_library*/,
                               ID *id_dst,
                               const ID *id_src,
                               const int /*flag*/)
{
  Key *key_dst = (Key *)id_dst;
  const Key *key_src = (const Key *)id_src;
  BLI_duplicatelist(&key_dst->block, &key_src->block);

  KeyBlock *kb_dst, *kb_src;
  for (kb_src = static_cast<KeyBlock *>(key_src->block.first),
      kb_dst = static_cast<KeyBlock *>(key_dst->block.first);
       kb_dst;
       kb_src = kb_src->next, kb_dst = kb_dst->next)
  {
    if (kb_dst->data) {
      kb_dst->data = MEM_dupallocN(kb_dst->data);
    }
    if (kb_src == key_src->refkey) {
      key_dst->refkey = kb_dst;
    }
  }
}

static void shapekey_free_data(ID *id)
{
  Key *key = (Key *)id;
  while (KeyBlock *kb = static_cast<KeyBlock *>(BLI_pophead(&key->block))) {
    if (kb->data) {
      MEM_freeN(kb->data);
    }
    MEM_freeN(kb);
  }
}

static void shapekey_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Key *key = reinterpret_cast<Key *>(id);
  BKE_LIB_FOREACHID_PROCESS_ID(data, key->from, IDWALK_CB_LOOPBACK);
}

static ID **shapekey_owner_pointer_get(ID *id, const bool debug_relationship_assert)
{
  Key *key = (Key *)id;

  if (debug_relationship_assert) {
    BLI_assert(key->from != nullptr);
    BLI_assert(BKE_key_from_id(key->from) == key);
  }

  return &key->from;
}

static void shapekey_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Key *key = (Key *)id;
  const bool is_undo = BLO_write_is_undo(writer);

  /* write LibData */
  BLO_write_id_struct(writer, Key, id_address, &key->id);
  BKE_id_blend_write(writer, &key->id);

  /* direct data */
  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    KeyBlock tmp_kb = *kb;
    /* Do not store actual geometry data in case this is a library override ID. */
    if (ID_IS_OVERRIDE_LIBRARY(key) && !is_undo) {
      tmp_kb.totelem = 0;
      tmp_kb.data = nullptr;
    }
    BLO_write_struct_at_address(writer, KeyBlock, kb, &tmp_kb);
    if (tmp_kb.data != nullptr) {
      BLO_write_raw(writer, tmp_kb.totelem * key->elemsize, tmp_kb.data);
    }
  }
}

/* old defines from DNA_ipo_types.h for data-type, stored in DNA - don't modify! */
#define IPO_FLOAT 4
#define IPO_BEZTRIPLE 100
#define IPO_BPOINT 101

static void shapekey_blend_read_data(BlendDataReader *reader, ID *id)
{
  Key *key = (Key *)id;
  BLO_read_struct_list(reader, KeyBlock, &(key->block));

  BLO_read_struct(reader, KeyBlock, &key->refkey);

  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    BLO_read_data_address(reader, &kb->data);

    /* NOTE: this is endianness-sensitive. */
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

#define KEY_MODE_DUMMY 0 /* use where mode isn't checked for */
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
      MEM_freeN(kb->data);
    }
    MEM_freeN(kb);
  }
}

Key *BKE_key_add(Main *bmain, ID *id) /* common function */
{
  Key *key;
  char *el;

  key = BKE_id_new<Key>(bmain, "Key");

  key->type = KEY_NORMAL;
  key->from = id;

  key->uidgen = 1;

  /* XXX the code here uses some defines which will soon be deprecated... */
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

  /* locate the key which is out of position */
  for (kb = static_cast<KeyBlock *>(key->block.first); kb; kb = kb->next) {
    if ((kb->next) && (kb->pos > kb->next->pos)) {
      break;
    }
  }

  /* if we find a key, move it */
  if (kb) {
    kb = kb->next; /* next key is the out-of-order one */
    BLI_remlink(&key->block, kb);

    /* find the right location and insert before */
    LISTBASE_FOREACH (KeyBlock *, kb2, &key->block) {
      if (kb2->pos > kb->pos) {
        BLI_insertlinkafter(&key->block, kb2->prev, kb);
        break;
      }
    }
  }

  /* new rule; first key is refkey, this to match drawing channels... */
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

void key_curve_normal_weights(float t, float data[4], KeyInterpolationType type)
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

static int setkeys(float fac, ListBase *lb, KeyBlock *k[], float t[4], int cycl)
{
  /* return 1 means k[2] is the position, return 0 means interpolate */
  KeyBlock *k1, *firstkey;
  float d, dpos, ofs = 0, lastpos;
  short bsplinetype;

  firstkey = static_cast<KeyBlock *>(lb->first);
  k1 = static_cast<KeyBlock *>(lb->last);
  lastpos = k1->pos;
  dpos = lastpos - firstkey->pos;

  if (fac < firstkey->pos) {
    fac = firstkey->pos;
  }
  else if (fac > k1->pos) {
    fac = k1->pos;
  }

  k1 = k[0] = k[1] = k[2] = k[3] = firstkey;
  t[0] = t[1] = t[2] = t[3] = k1->pos;

#if 0
  if (fac < 0.0 || fac > 1.0) {
    return 1;
  }
#endif

  if (k1->next == nullptr) {
    return 1;
  }

  if (cycl) { /* pre-sort */
    k[2] = k1->next;
    k[3] = k[2]->next;
    if (k[3] == nullptr) {
      k[3] = k1;
    }
    while (k1) {
      if (k1->next == nullptr) {
        k[0] = k1;
      }
      k1 = k1->next;
    }
    // k1 = k[1]; /* UNUSED */
    t[0] = k[0]->pos;
    t[1] += dpos;
    t[2] = k[2]->pos + dpos;
    t[3] = k[3]->pos + dpos;
    fac += dpos;
    ofs = dpos;
    if (k[3] == k[1]) {
      t[3] += dpos;
      ofs = 2.0f * dpos;
    }
    if (fac < t[1]) {
      fac += dpos;
    }
    k1 = k[3];
  }
  else { /* pre-sort */
    k[2] = k1->next;
    t[2] = k[2]->pos;
    k[3] = k[2]->next;
    if (k[3] == nullptr) {
      k[3] = k[2];
    }
    t[3] = k[3]->pos;
    k1 = k[3];
  }

  while (t[2] < fac) { /* find correct location */
    if (k1->next == nullptr) {
      if (cycl) {
        k1 = firstkey;
        ofs += dpos;
      }
      else if (t[2] == t[3]) {
        break;
      }
    }
    else {
      k1 = k1->next;
    }

    t[0] = t[1];
    k[0] = k[1];
    t[1] = t[2];
    k[1] = k[2];
    t[2] = t[3];
    k[2] = k[3];
    t[3] = k1->pos + ofs;
    k[3] = k1;

    if (ofs > 2.1f + lastpos) {
      break;
    }
  }

  bsplinetype = 0;
  if (k[1]->type == KEY_BSPLINE || k[2]->type == KEY_BSPLINE) {
    bsplinetype = 1;
  }

  if (cycl == 0) {
    if (bsplinetype == 0) { /* B spline doesn't go through the control points */
      if (fac <= t[1]) {    /* fac for 1st key */
        t[2] = t[1];
        k[2] = k[1];
        return 1;
      }
      if (fac >= t[2]) { /* fac after 2nd key */
        return 1;
      }
    }
    else if (fac > t[2]) { /* last key */
      fac = t[2];
      k[3] = k[2];
      t[3] = t[2];
    }
  }

  d = t[2] - t[1];
  if (d == 0.0f) {
    if (bsplinetype == 0) {
      return 1; /* both keys equal */
    }
  }
  else {
    d = (fac - t[1]) / d;
  }

  /* interpolation */
  key_curve_position_weights(d, t, KeyInterpolationType(k[1]->type));

  if (k[1]->type != k[2]->type) {
    float t_other[4];
    key_curve_position_weights(d, t_other, KeyInterpolationType(k[2]->type));
    interp_v4_v4v4(t, t, t_other, d);
  }

  return 0;
}

static void flerp(int tot,
                  float *in,
                  const float *f0,
                  const float *f1,
                  const float *f2,
                  const float *f3,
                  const float *t)
{
  int a;

  for (a = 0; a < tot; a++) {
    in[a] = t[0] * f0[a] + t[1] * f1[a] + t[2] * f2[a] + t[3] * f3[a];
  }
}

static void rel_flerp(int tot, float *in, const float *ref, const float *out, float fac)
{
  int a;

  for (a = 0; a < tot; a++) {
    in[a] -= fac * (ref[a] - out[a]);
  }
}

static char *key_block_get_data(Key *key, KeyBlock *actkb, KeyBlock *kb, char **freedata)
{
  if (kb == actkb) {
    /* this hack makes it possible to edit shape keys in
     * edit mode with shape keys blending applied */
    if (GS(key->from->name) == ID_ME) {
      Mesh *mesh;
      BMVert *eve;
      BMIter iter;
      float (*co)[3];
      int a;

      mesh = (Mesh *)key->from;

      if (mesh->runtime->edit_mesh && mesh->runtime->edit_mesh->bm->totvert == kb->totelem) {
        a = 0;
        co = MEM_malloc_arrayN<float[3]>(size_t(mesh->runtime->edit_mesh->bm->totvert),
                                         "key_block_get_data");

        BM_ITER_MESH (eve, &iter, mesh->runtime->edit_mesh->bm, BM_VERTS_OF_MESH) {
          copy_v3_v3(co[a], eve->co);
          a++;
        }

        *freedata = (char *)co;
        return (char *)co;
      }
    }
  }

  *freedata = nullptr;
  return static_cast<char *>(kb->data);
}

/* currently only the first value of 'r_ofs' may be set. */
static bool key_pointer_size(
    const Key *key, const int mode, int *r_poinsize, int *r_ofs, int *r_step)
{
  if (key->from == nullptr) {
    return false;
  }

  *r_step = 1;

  switch (GS(key->from->name)) {
    case ID_ME:
      *r_ofs = sizeof(float[KEYELEM_FLOAT_LEN_COORD]);
      *r_poinsize = *r_ofs;
      break;
    case ID_LT:
      *r_ofs = sizeof(float[KEYELEM_FLOAT_LEN_COORD]);
      *r_poinsize = *r_ofs;
      break;
    case ID_CU_LEGACY:
      if (mode == KEY_MODE_BPOINT) {
        *r_ofs = sizeof(float[KEYELEM_FLOAT_LEN_BPOINT]);
        *r_step = KEYELEM_ELEM_LEN_BPOINT;
      }
      else {
        *r_ofs = sizeof(float[KEYELEM_FLOAT_LEN_BEZTRIPLE]);
        *r_step = KEYELEM_ELEM_LEN_BEZTRIPLE;
      }
      *r_poinsize = sizeof(float[KEYELEM_ELEM_SIZE_CURVE]);
      break;
    default:
      BLI_assert_msg(0, "invalid 'key->from' ID type");
      return false;
  }

  return true;
}

static void cp_key(const int start,
                   int end,
                   const int tot,
                   char *poin,
                   Key *key,
                   KeyBlock *actkb,
                   KeyBlock *kb,
                   float *weights,
                   const int mode)
{
  float ktot = 0.0, kd = 0.0;
  int elemsize, poinsize = 0, a, step, *ofsp, ofs[32], flagflo = 0;
  char *k1, *kref, *freek1, *freekref;
  char *cp, elemstr[8];

  /* currently always 0, in future key_pointer_size may assign */
  ofs[1] = 0;

  if (!key_pointer_size(key, mode, &poinsize, &ofs[0], &step)) {
    return;
  }

  end = std::min(end, tot);

  if (tot != kb->totelem) {
    ktot = 0.0;
    flagflo = 1;
    if (kb->totelem) {
      kd = kb->totelem / float(tot);
    }
    else {
      return;
    }
  }

  k1 = key_block_get_data(key, actkb, kb, &freek1);
  kref = key_block_get_data(key, actkb, key->refkey, &freekref);

  /* this exception is needed curves with multiple splines */
  if (start != 0) {

    poin += poinsize * start;

    if (flagflo) {
      ktot += start * kd;
      a = int(floor(ktot));
      if (a) {
        ktot -= a;
        k1 += a * key->elemsize;
      }
    }
    else {
      k1 += start * key->elemsize;
    }
  }

  if (mode == KEY_MODE_BEZTRIPLE) {
    elemstr[0] = 1;
    elemstr[1] = IPO_BEZTRIPLE;
    elemstr[2] = 0;
  }

  /* just do it here, not above! */
  elemsize = key->elemsize * step;

  for (a = start; a < end; a += step) {
    cp = key->elemstr;
    if (mode == KEY_MODE_BEZTRIPLE) {
      cp = elemstr;
    }

    ofsp = ofs;

    while (cp[0]) {

      switch (cp[1]) {
        case IPO_FLOAT:
          if (weights) {
            memcpy(poin, kref, sizeof(float[KEYELEM_FLOAT_LEN_COORD]));
            if (*weights != 0.0f) {
              rel_flerp(
                  KEYELEM_FLOAT_LEN_COORD, (float *)poin, (float *)kref, (float *)k1, *weights);
            }
            weights++;
          }
          else {
            memcpy(poin, k1, sizeof(float[KEYELEM_FLOAT_LEN_COORD]));
          }
          break;
        case IPO_BPOINT:
          memcpy(poin, k1, sizeof(float[KEYELEM_FLOAT_LEN_BPOINT]));
          break;
        case IPO_BEZTRIPLE:
          memcpy(poin, k1, sizeof(float[KEYELEM_FLOAT_LEN_BEZTRIPLE]));
          break;
        default:
          /* should never happen */
          if (freek1) {
            MEM_freeN(freek1);
          }
          if (freekref) {
            MEM_freeN(freekref);
          }
          BLI_assert_msg(0, "invalid 'cp[1]'");
          return;
      }

      poin += *ofsp;
      cp += 2;
      ofsp++;
    }

    /* are we going to be nasty? */
    if (flagflo) {
      ktot += kd;
      while (ktot >= 1.0f) {
        ktot -= 1.0f;
        k1 += elemsize;
        kref += elemsize;
      }
    }
    else {
      k1 += elemsize;
      kref += elemsize;
    }
  }

  if (freek1) {
    MEM_freeN(freek1);
  }
  if (freekref) {
    MEM_freeN(freekref);
  }
}

static void cp_cu_key(Curve *cu,
                      Key *key,
                      KeyBlock *actkb,
                      KeyBlock *kb,
                      const int start,
                      int end,
                      char *out,
                      const int tot)
{
  Nurb *nu;
  int a, step, a1, a2;

  for (a = 0, nu = static_cast<Nurb *>(cu->nurb.first); nu; nu = nu->next, a += step) {
    if (nu->bp) {
      step = KEYELEM_ELEM_LEN_BPOINT * nu->pntsu * nu->pntsv;

      a1 = max_ii(a, start);
      a2 = min_ii(a + step, end);

      if (a1 < a2) {
        cp_key(a1, a2, tot, out, key, actkb, kb, nullptr, KEY_MODE_BPOINT);
      }
    }
    else if (nu->bezt) {
      step = KEYELEM_ELEM_LEN_BEZTRIPLE * nu->pntsu;

      /* exception because keys prefer to work with complete blocks */
      a1 = max_ii(a, start);
      a2 = min_ii(a + step, end);

      if (a1 < a2) {
        cp_key(a1, a2, tot, out, key, actkb, kb, nullptr, KEY_MODE_BEZTRIPLE);
      }
    }
    else {
      step = 0;
    }
  }
}

static void key_evaluate_relative(const int start,
                                  int end,
                                  const int tot,
                                  char *basispoin,
                                  Key *key,
                                  KeyBlock *actkb,
                                  float **per_keyblock_weights,
                                  const int mode)
{
  KeyBlock *kb;
  int *ofsp, ofs[3], elemsize, b, step;
  char *cp, *poin, *reffrom, *from, elemstr[8];
  int poinsize, keyblock_index;

  /* currently always 0, in future key_pointer_size may assign */
  ofs[1] = 0;

  if (!key_pointer_size(key, mode, &poinsize, &ofs[0], &step)) {
    return;
  }

  end = std::min(end, tot);

  /* In case of Bezier-triple. */
  elemstr[0] = 1; /* Number of IPO-floats. */
  elemstr[1] = IPO_BEZTRIPLE;
  elemstr[2] = 0;

  /* just here, not above! */
  elemsize = key->elemsize * step;

  /* step 1 init */
  cp_key(start, end, tot, basispoin, key, actkb, key->refkey, nullptr, mode);

  /* step 2: do it */

  for (kb = static_cast<KeyBlock *>(key->block.first), keyblock_index = 0; kb;
       kb = kb->next, keyblock_index++)
  {
    if (kb != key->refkey) {
      float icuval = kb->curval;

      /* only with value, and no difference allowed */
      if (!(kb->flag & KEYBLOCK_MUTE) && icuval != 0.0f && kb->totelem == tot) {
        KeyBlock *refb;
        float weight,
            *weights = per_keyblock_weights ? per_keyblock_weights[keyblock_index] : nullptr;
        char *freefrom = nullptr;

        /* reference now can be any block */
        refb = static_cast<KeyBlock *>(BLI_findlink(&key->block, kb->relative));
        if (refb == nullptr) {
          continue;
        }

        poin = basispoin;
        from = key_block_get_data(key, actkb, kb, &freefrom);

        /* For meshes, use the original values instead of the bmesh values to
         * maintain a constant offset. */
        reffrom = static_cast<char *>(refb->data);

        poin += start * poinsize;
        reffrom += key->elemsize * start; /* key elemsize yes! */
        from += key->elemsize * start;

        for (b = start; b < end; b += step) {

          weight = weights ? (*weights * icuval) : icuval;

          cp = key->elemstr;
          if (mode == KEY_MODE_BEZTRIPLE) {
            cp = elemstr;
          }

          ofsp = ofs;

          while (cp[0]) { /* (cp[0] == amount) */

            switch (cp[1]) {
              case IPO_FLOAT:
                rel_flerp(KEYELEM_FLOAT_LEN_COORD,
                          (float *)poin,
                          (float *)reffrom,
                          (float *)from,
                          weight);
                break;
              case IPO_BPOINT:
                rel_flerp(KEYELEM_FLOAT_LEN_BPOINT,
                          (float *)poin,
                          (float *)reffrom,
                          (float *)from,
                          weight);
                break;
              case IPO_BEZTRIPLE:
                rel_flerp(KEYELEM_FLOAT_LEN_BEZTRIPLE,
                          (float *)poin,
                          (float *)reffrom,
                          (float *)from,
                          weight);
                break;
              default:
                /* should never happen */
                if (freefrom) {
                  MEM_freeN(freefrom);
                }
                BLI_assert_msg(0, "invalid 'cp[1]'");
                return;
            }

            poin += *ofsp;

            cp += 2;
            ofsp++;
          }

          reffrom += elemsize;
          from += elemsize;

          if (weights) {
            weights++;
          }
        }

        if (freefrom) {
          MEM_freeN(freefrom);
        }
      }
    }
  }
}

static void do_key(const int start,
                   int end,
                   const int tot,
                   char *poin,
                   Key *key,
                   KeyBlock *actkb,
                   KeyBlock **k,
                   float *t,
                   const int mode)
{
  float k1tot = 0.0, k2tot = 0.0, k3tot = 0.0, k4tot = 0.0;
  float k1d = 0.0, k2d = 0.0, k3d = 0.0, k4d = 0.0;
  int a, step, ofs[32], *ofsp;
  int flagdo = 15, flagflo = 0, elemsize, poinsize = 0;
  char *k1, *k2, *k3, *k4, *freek1, *freek2, *freek3, *freek4;
  char *cp, elemstr[8];

  /* currently always 0, in future key_pointer_size may assign */
  ofs[1] = 0;

  if (!key_pointer_size(key, mode, &poinsize, &ofs[0], &step)) {
    return;
  }

  end = std::min(end, tot);

  k1 = key_block_get_data(key, actkb, k[0], &freek1);
  k2 = key_block_get_data(key, actkb, k[1], &freek2);
  k3 = key_block_get_data(key, actkb, k[2], &freek3);
  k4 = key_block_get_data(key, actkb, k[3], &freek4);

  /* Test for more or less points (per key!) */
  if (tot != k[0]->totelem) {
    k1tot = 0.0;
    flagflo |= 1;
    if (k[0]->totelem) {
      k1d = k[0]->totelem / float(tot);
    }
    else {
      flagdo -= 1;
    }
  }
  if (tot != k[1]->totelem) {
    k2tot = 0.0;
    flagflo |= 2;
    if (k[0]->totelem) {
      k2d = k[1]->totelem / float(tot);
    }
    else {
      flagdo -= 2;
    }
  }
  if (tot != k[2]->totelem) {
    k3tot = 0.0;
    flagflo |= 4;
    if (k[0]->totelem) {
      k3d = k[2]->totelem / float(tot);
    }
    else {
      flagdo -= 4;
    }
  }
  if (tot != k[3]->totelem) {
    k4tot = 0.0;
    flagflo |= 8;
    if (k[0]->totelem) {
      k4d = k[3]->totelem / float(tot);
    }
    else {
      flagdo -= 8;
    }
  }

  /* this exception is needed for curves with multiple splines */
  if (start != 0) {

    poin += poinsize * start;

    if (flagdo & 1) {
      if (flagflo & 1) {
        k1tot += start * k1d;
        a = int(floor(k1tot));
        if (a) {
          k1tot -= a;
          k1 += a * key->elemsize;
        }
      }
      else {
        k1 += start * key->elemsize;
      }
    }
    if (flagdo & 2) {
      if (flagflo & 2) {
        k2tot += start * k2d;
        a = int(floor(k2tot));
        if (a) {
          k2tot -= a;
          k2 += a * key->elemsize;
        }
      }
      else {
        k2 += start * key->elemsize;
      }
    }
    if (flagdo & 4) {
      if (flagflo & 4) {
        k3tot += start * k3d;
        a = int(floor(k3tot));
        if (a) {
          k3tot -= a;
          k3 += a * key->elemsize;
        }
      }
      else {
        k3 += start * key->elemsize;
      }
    }
    if (flagdo & 8) {
      if (flagflo & 8) {
        k4tot += start * k4d;
        a = int(floor(k4tot));
        if (a) {
          k4tot -= a;
          k4 += a * key->elemsize;
        }
      }
      else {
        k4 += start * key->elemsize;
      }
    }
  }

  /* In case of bezier-triples. */
  elemstr[0] = 1; /* Number of IPO-floats. */
  elemstr[1] = IPO_BEZTRIPLE;
  elemstr[2] = 0;

  /* only here, not above! */
  elemsize = key->elemsize * step;

  for (a = start; a < end; a += step) {
    cp = key->elemstr;
    if (mode == KEY_MODE_BEZTRIPLE) {
      cp = elemstr;
    }

    ofsp = ofs;

    while (cp[0]) { /* (cp[0] == amount) */

      switch (cp[1]) {
        case IPO_FLOAT:
          flerp(KEYELEM_FLOAT_LEN_COORD,
                (float *)poin,
                (float *)k1,
                (float *)k2,
                (float *)k3,
                (float *)k4,
                t);
          break;
        case IPO_BPOINT:
          flerp(KEYELEM_FLOAT_LEN_BPOINT,
                (float *)poin,
                (float *)k1,
                (float *)k2,
                (float *)k3,
                (float *)k4,
                t);
          break;
        case IPO_BEZTRIPLE:
          flerp(KEYELEM_FLOAT_LEN_BEZTRIPLE,
                (float *)poin,
                (float *)k1,
                (float *)k2,
                (float *)k3,
                (float *)k4,
                t);
          break;
        default:
          /* should never happen */
          if (freek1) {
            MEM_freeN(freek1);
          }
          if (freek2) {
            MEM_freeN(freek2);
          }
          if (freek3) {
            MEM_freeN(freek3);
          }
          if (freek4) {
            MEM_freeN(freek4);
          }
          BLI_assert_msg(0, "invalid 'cp[1]'");
          return;
      }

      poin += *ofsp;
      cp += 2;
      ofsp++;
    }
    /* lets do it the difficult way: when keys have a different size */
    if (flagdo & 1) {
      if (flagflo & 1) {
        k1tot += k1d;
        while (k1tot >= 1.0f) {
          k1tot -= 1.0f;
          k1 += elemsize;
        }
      }
      else {
        k1 += elemsize;
      }
    }
    if (flagdo & 2) {
      if (flagflo & 2) {
        k2tot += k2d;
        while (k2tot >= 1.0f) {
          k2tot -= 1.0f;
          k2 += elemsize;
        }
      }
      else {
        k2 += elemsize;
      }
    }
    if (flagdo & 4) {
      if (flagflo & 4) {
        k3tot += k3d;
        while (k3tot >= 1.0f) {
          k3tot -= 1.0f;
          k3 += elemsize;
        }
      }
      else {
        k3 += elemsize;
      }
    }
    if (flagdo & 8) {
      if (flagflo & 8) {
        k4tot += k4d;
        while (k4tot >= 1.0f) {
          k4tot -= 1.0f;
          k4 += elemsize;
        }
      }
      else {
        k4 += elemsize;
      }
    }
  }

  if (freek1) {
    MEM_freeN(freek1);
  }
  if (freek2) {
    MEM_freeN(freek2);
  }
  if (freek3) {
    MEM_freeN(freek3);
  }
  if (freek4) {
    MEM_freeN(freek4);
  }
}

static float *get_weights_array(Object *ob, const char *vgroup, WeightsArrayCache *cache)
{
  const MDeformVert *dvert = nullptr;
  BMEditMesh *em = nullptr;
  BMIter iter;
  BMVert *eve;
  int totvert = 0, defgrp_index = 0;

  /* no vgroup string set? */
  if (vgroup[0] == 0) {
    return nullptr;
  }

  /* gather dvert and totvert */
  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    dvert = mesh->deform_verts().data();
    totvert = mesh->verts_num;

    if (mesh->runtime->edit_mesh && mesh->runtime->edit_mesh->bm->totvert == totvert) {
      em = mesh->runtime->edit_mesh.get();
    }
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = static_cast<Lattice *>(ob->data);
    dvert = lt->dvert;
    totvert = lt->pntsu * lt->pntsv * lt->pntsw;
  }

  if (dvert == nullptr) {
    return nullptr;
  }

  /* find the group (weak loop-in-loop) */
  defgrp_index = BKE_object_defgroup_name_index(ob, vgroup);
  if (defgrp_index != -1) {
    float *weights;

    if (cache) {
      if (cache->defgroup_weights == nullptr) {
        int num_defgroup = BKE_object_defgroup_count(ob);
        cache->defgroup_weights = MEM_calloc_arrayN<float *>(num_defgroup,
                                                             "cached defgroup weights");
        cache->num_defgroup_weights = num_defgroup;
      }

      if (cache->defgroup_weights[defgrp_index]) {
        return cache->defgroup_weights[defgrp_index];
      }
    }

    weights = MEM_malloc_arrayN<float>(size_t(totvert), "weights");

    if (em) {
      int i;
      const int cd_dvert_offset = CustomData_get_offset(&em->bm->vdata, CD_MDEFORMVERT);
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
  KeyBlock *keyblock;
  float **per_keyblock_weights;
  int keyblock_index;

  per_keyblock_weights = MEM_malloc_arrayN<float *>(size_t(key->totkey), "per keyblock weights");

  for (keyblock = static_cast<KeyBlock *>(key->block.first), keyblock_index = 0; keyblock;
       keyblock = keyblock->next, keyblock_index++)
  {
    per_keyblock_weights[keyblock_index] = get_weights_array(ob, keyblock->vgroup, cache);
  }

  return per_keyblock_weights;
}

static void keyblock_free_per_block_weights(Key *key,
                                            float **per_keyblock_weights,
                                            WeightsArrayCache *cache)
{
  int a;

  if (cache) {
    if (cache->num_defgroup_weights) {
      for (a = 0; a < cache->num_defgroup_weights; a++) {
        if (cache->defgroup_weights[a]) {
          MEM_freeN(cache->defgroup_weights[a]);
        }
      }
      MEM_freeN(cache->defgroup_weights);
    }
    cache->defgroup_weights = nullptr;
  }
  else {
    for (a = 0; a < key->totkey; a++) {
      if (per_keyblock_weights[a]) {
        MEM_freeN(per_keyblock_weights[a]);
      }
    }
  }

  MEM_freeN(per_keyblock_weights);
}

static void do_mesh_key(Object *ob, Key *key, char *out, const int tot)
{
  KeyBlock *k[4], *actkb = BKE_keyblock_from_object(ob);
  float t[4];
  int flag = 0;

  if (key->type == KEY_RELATIVE) {
    WeightsArrayCache cache = {0, nullptr};
    float **per_keyblock_weights;
    per_keyblock_weights = keyblock_get_per_block_weights(ob, key, &cache);
    key_evaluate_relative(0, tot, tot, out, key, actkb, per_keyblock_weights, KEY_MODE_DUMMY);
    keyblock_free_per_block_weights(key, per_keyblock_weights, &cache);
  }
  else {
    const float ctime_scaled = key->ctime / 100.0f;

    flag = setkeys(ctime_scaled, &key->block, k, t, 0);

    if (flag == 0) {
      do_key(0, tot, tot, out, key, actkb, k, t, KEY_MODE_DUMMY);
    }
    else {
      cp_key(0, tot, tot, out, key, actkb, k[2], nullptr, KEY_MODE_DUMMY);
    }
  }
}

static void do_cu_key(
    Curve *cu, Key *key, KeyBlock *actkb, KeyBlock **k, float *t, char *out, const int tot)
{
  Nurb *nu;
  int a, step;

  for (a = 0, nu = static_cast<Nurb *>(cu->nurb.first); nu; nu = nu->next, a += step) {
    if (nu->bp) {
      step = KEYELEM_ELEM_LEN_BPOINT * nu->pntsu * nu->pntsv;
      do_key(a, a + step, tot, out, key, actkb, k, t, KEY_MODE_BPOINT);
    }
    else if (nu->bezt) {
      step = KEYELEM_ELEM_LEN_BEZTRIPLE * nu->pntsu;
      do_key(a, a + step, tot, out, key, actkb, k, t, KEY_MODE_BEZTRIPLE);
    }
    else {
      step = 0;
    }
  }
}

static void do_rel_cu_key(Curve *cu, Key *key, KeyBlock *actkb, char *out, const int tot)
{
  Nurb *nu;
  int a, step;

  for (a = 0, nu = static_cast<Nurb *>(cu->nurb.first); nu; nu = nu->next, a += step) {
    if (nu->bp) {
      step = KEYELEM_ELEM_LEN_BPOINT * nu->pntsu * nu->pntsv;
      key_evaluate_relative(a, a + step, tot, out, key, actkb, nullptr, KEY_MODE_BPOINT);
    }
    else if (nu->bezt) {
      step = KEYELEM_ELEM_LEN_BEZTRIPLE * nu->pntsu;
      key_evaluate_relative(a, a + step, tot, out, key, actkb, nullptr, KEY_MODE_BEZTRIPLE);
    }
    else {
      step = 0;
    }
  }
}

static void do_curve_key(Object *ob, Key *key, char *out, const int tot)
{
  Curve *cu = static_cast<Curve *>(ob->data);
  KeyBlock *k[4], *actkb = BKE_keyblock_from_object(ob);
  float t[4];
  int flag = 0;

  if (key->type == KEY_RELATIVE) {
    do_rel_cu_key(cu, cu->key, actkb, out, tot);
  }
  else {
    const float ctime_scaled = key->ctime / 100.0f;

    flag = setkeys(ctime_scaled, &key->block, k, t, 0);

    if (flag == 0) {
      do_cu_key(cu, key, actkb, k, t, out, tot);
    }
    else {
      cp_cu_key(cu, key, actkb, k[2], 0, tot, out, tot);
    }
  }
}

static void do_latt_key(Object *ob, Key *key, char *out, const int tot)
{
  Lattice *lt = static_cast<Lattice *>(ob->data);
  KeyBlock *k[4], *actkb = BKE_keyblock_from_object(ob);
  float t[4];
  int flag;

  if (key->type == KEY_RELATIVE) {
    float **per_keyblock_weights;
    per_keyblock_weights = keyblock_get_per_block_weights(ob, key, nullptr);
    key_evaluate_relative(0, tot, tot, out, key, actkb, per_keyblock_weights, KEY_MODE_DUMMY);
    keyblock_free_per_block_weights(key, per_keyblock_weights, nullptr);
  }
  else {
    const float ctime_scaled = key->ctime / 100.0f;

    flag = setkeys(ctime_scaled, &key->block, k, t, 0);

    if (flag == 0) {
      do_key(0, tot, tot, out, key, actkb, k, t, KEY_MODE_DUMMY);
    }
    else {
      cp_key(0, tot, tot, out, key, actkb, k[2], nullptr, KEY_MODE_DUMMY);
    }
  }

  if (lt->flag & LT_OUTSIDE) {
    outside_lattice(lt);
  }
}

static void keyblock_data_convert_to_lattice(const float (*fp)[3],
                                             BPoint *bpoint,
                                             const int totpoint);
static void keyblock_data_convert_to_curve(const float *fp, ListBase *nurb, const int totpoint);

float *BKE_key_evaluate_object_ex(
    Object *ob, int *r_totelem, float *arr, size_t arr_size, ID *obdata)
{
  Key *key = BKE_key_from_object(ob);
  KeyBlock *actkb = BKE_keyblock_from_object(ob);
  char *out;
  int tot = 0, size = 0;

  if (key == nullptr || BLI_listbase_is_empty(&key->block)) {
    return nullptr;
  }

  /* compute size of output array */
  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);

    tot = mesh->verts_num;
    size = tot * sizeof(float[KEYELEM_FLOAT_LEN_COORD]);
  }
  else if (ob->type == OB_LATTICE) {
    Lattice *lt = static_cast<Lattice *>(ob->data);

    tot = lt->pntsu * lt->pntsv * lt->pntsw;
    size = tot * sizeof(float[KEYELEM_FLOAT_LEN_COORD]);
  }
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
    Curve *cu = static_cast<Curve *>(ob->data);

    tot = BKE_keyblock_curve_element_count(&cu->nurb);
    size = tot * sizeof(float[KEYELEM_ELEM_SIZE_CURVE]);
  }

  /* if nothing to interpolate, cancel */
  if (tot == 0 || size == 0) {
    return nullptr;
  }

  /* allocate array */
  if (arr == nullptr) {
    out = MEM_calloc_arrayN<char>(size, "BKE_key_evaluate_object out");
  }
  else {
    if (arr_size != size) {
      return nullptr;
    }

    out = (char *)arr;
  }

  if (ob->shapeflag & OB_SHAPE_LOCK) {
    /* shape locked, copy the locked shape instead of blending */
    KeyBlock *kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, ob->shapenr - 1));

    if (kb && (kb->flag & KEYBLOCK_MUTE)) {
      kb = key->refkey;
    }

    if (kb == nullptr) {
      kb = static_cast<KeyBlock *>(key->block.first);
      ob->shapenr = 1;
    }

    if (OB_TYPE_SUPPORT_VGROUP(ob->type)) {
      float *weights = get_weights_array(ob, kb->vgroup, nullptr);

      cp_key(0, tot, tot, out, key, actkb, kb, weights, 0);

      if (weights) {
        MEM_freeN(weights);
      }
    }
    else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
      cp_cu_key(static_cast<Curve *>(ob->data), key, actkb, kb, 0, tot, out, tot);
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
        Mesh *mesh = (Mesh *)obdata;
        const int totvert = min_ii(tot, mesh->verts_num);
        mesh->vert_positions_for_write().take_front(totvert).copy_from(
            {reinterpret_cast<const blender::float3 *>(out), totvert});
        mesh->tag_positions_changed();
        break;
      }
      case ID_LT: {
        Lattice *lattice = (Lattice *)obdata;
        const int totpoint = min_ii(tot, lattice->pntsu * lattice->pntsv * lattice->pntsw);
        keyblock_data_convert_to_lattice((const float (*)[3])out, lattice->def, totpoint);
        break;
      }
      case ID_CU_LEGACY: {
        Curve *curve = (Curve *)obdata;
        const int totpoint = min_ii(tot, BKE_keyblock_curve_element_count(&curve->nurb));
        keyblock_data_convert_to_curve((const float *)out, &curve->nurb, totpoint);
        break;
      }
      default:
        BLI_assert_unreachable();
    }
  }

  if (r_totelem) {
    *r_totelem = tot;
  }
  return (float *)out;
}

float *BKE_key_evaluate_object(Object *ob, int *r_totelem)
{
  return BKE_key_evaluate_object_ex(ob, r_totelem, nullptr, 0, nullptr);
}

int BKE_keyblock_element_count_from_shape(const Key *key, const int shape_index)
{
  int result = 0;
  int index = 0;
  for (const KeyBlock *kb = static_cast<const KeyBlock *>(key->block.first); kb;
       kb = kb->next, index++)
  {
    if (ELEM(shape_index, -1, index)) {
      result += kb->totelem;
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
  uint8_t *elements = (uint8_t *)arr.data();
  int index = 0;
  for (const KeyBlock *kb = static_cast<const KeyBlock *>(key->block.first); kb;
       kb = kb->next, index++)
  {
    if (ELEM(shape_index, -1, index)) {
      const int block_elem_len = kb->totelem * key->elemsize;
      memcpy(elements, kb->data, block_elem_len);
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

  int index = 0;
  for (KeyBlock *kb = static_cast<KeyBlock *>(key->block.first); kb; kb = kb->next, index++) {
    if (ELEM(shape_index, -1, index)) {
      const int block_elem_len = kb->totelem;
      float (*block_data)[3] = (float (*)[3])kb->data;
      for (int data_offset = 0; data_offset < block_elem_len; ++data_offset) {
        const float *src_data = (const float *)(elements + data_offset);
        float *dst_data = (float *)(block_data + data_offset);
        mul_v3_m4v3(dst_data, transform.ptr(), src_data);
      }
      elements += block_elem_len;
    }
  }
}

void BKE_keyblock_curve_data_set_with_mat4(Key *key,
                                           const ListBase *nurb,
                                           const int shape_index,
                                           const void *data,
                                           const float4x4 &transform)
{
  const uint8_t *elements = static_cast<const uint8_t *>(data);

  int index = 0;
  for (KeyBlock *kb = static_cast<KeyBlock *>(key->block.first); kb; kb = kb->next, index++) {
    if (ELEM(shape_index, -1, index)) {
      const int block_elem_size = kb->totelem * key->elemsize;
      BKE_keyblock_curve_data_transform(nurb, transform.ptr(), elements, kb->data);
      elements += block_elem_size;
    }
  }
}

void BKE_keyblock_data_set(Key *key, const int shape_index, const void *data)
{
  const uint8_t *elements = static_cast<const uint8_t *>(data);
  int index = 0;
  for (KeyBlock *kb = static_cast<KeyBlock *>(key->block.first); kb; kb = kb->next, index++) {
    if (ELEM(shape_index, -1, index)) {
      const int block_elem_size = kb->totelem * key->elemsize;
      memcpy(kb->data, elements, block_elem_size);
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
      Mesh *mesh = (Mesh *)id;
      return &mesh->key;
    }
    case ID_CU_LEGACY: {
      Curve *cu = (Curve *)id;
      if (cu->ob_type != OB_FONT) {
        return &cu->key;
      }
      break;
    }
    case ID_LT: {
      Lattice *lt = (Lattice *)id;
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

  return BKE_key_from_id_p(static_cast<ID *>(ob->data));
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
  KeyBlock *kb;
  float curpos = -0.1;
  int tot;

  kb = static_cast<KeyBlock *>(key->block.last);
  if (kb) {
    curpos = kb->pos;
  }

  kb = MEM_callocN<KeyBlock>("Keyblock");
  BLI_addtail(&key->block, kb);
  kb->type = KEY_LINEAR;

  tot = BLI_listbase_count(&key->block);
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

  /* \note caller may want to set this to current time, but don't do it here since we need to sort
   * which could cause problems in some cases, see #BKE_keyblock_add_ctime. */
  kb->pos = curpos + 0.1f; /* only used for absolute shape keys */

  return kb;
}

KeyBlock *BKE_keyblock_duplicate(Key *key, KeyBlock *kb_src)
{
  BLI_assert(BLI_findindex(&key->block, kb_src) != -1);
  KeyBlock *kb_dst = BKE_keyblock_add(key, kb_src->name);
  kb_dst->totelem = kb_src->totelem;
  kb_dst->data = MEM_dupallocN(kb_src->data);
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
    LISTBASE_FOREACH (KeyBlock *, it_kb, &key->block) {
      /* Use epsilon to avoid floating point precision issues.
       * 1e-3 because the position is stored as frame * 1e-2. */
      if (compare_ff(it_kb->pos, cpos, 1e-3f)) {
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
  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    if (kb->uid == uid) {
      return kb;
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
  PointerRNA ptr = RNA_pointer_create_discrete((ID *)&key->id, &RNA_ShapeKey, (KeyBlock *)kb);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "value");
  return RNA_path_from_ID_to_property(&ptr, prop);
}

/* conversion functions */

/************************* Lattice ************************/

void BKE_keyblock_update_from_lattice(const Lattice *lt, KeyBlock *kb)
{
  BPoint *bp;
  float (*fp)[3];
  int a, tot;

  BLI_assert(kb->totelem == lt->pntsu * lt->pntsv * lt->pntsw);

  tot = kb->totelem;
  if (tot == 0) {
    return;
  }

  bp = lt->def;
  fp = static_cast<float (*)[3]>(kb->data);
  for (a = 0; a < kb->totelem; a++, fp++, bp++) {
    copy_v3_v3(*fp, bp->vec);
  }
}

void BKE_keyblock_convert_from_lattice(const Lattice *lt, KeyBlock *kb)
{
  int tot;

  tot = lt->pntsu * lt->pntsv * lt->pntsw;
  if (tot == 0) {
    return;
  }

  MEM_SAFE_FREE(kb->data);

  kb->data = MEM_malloc_arrayN(size_t(tot), size_t(lt->key->elemsize), __func__);
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

int BKE_keyblock_curve_element_count(const ListBase *nurb)
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

void BKE_keyblock_update_from_curve(const Curve * /*cu*/, KeyBlock *kb, const ListBase *nurb)
{
  BezTriple *bezt;
  BPoint *bp;
  float *fp;
  int a, tot;

  /* count */
  BLI_assert(BKE_keyblock_curve_element_count(nurb) == kb->totelem);

  tot = kb->totelem;
  if (tot == 0) {
    return;
  }

  fp = static_cast<float *>(kb->data);
  LISTBASE_FOREACH (Nurb *, nu, nurb) {
    if (nu->bezt) {
      for (a = nu->pntsu, bezt = nu->bezt; a; a--, bezt++) {
        for (int i = 0; i < 3; i++) {
          copy_v3_v3(&fp[i * 3], bezt->vec[i]);
        }
        fp[9] = bezt->tilt;
        fp[10] = bezt->radius;
        fp += KEYELEM_FLOAT_LEN_BEZTRIPLE;
      }
    }
    else {
      for (a = nu->pntsu * nu->pntsv, bp = nu->bp; a; a--, bp++) {
        copy_v3_v3(fp, bp->vec);
        fp[3] = bp->tilt;
        fp[4] = bp->radius;
        fp += KEYELEM_FLOAT_LEN_BPOINT;
      }
    }
  }
}

void BKE_keyblock_curve_data_transform(const ListBase *nurb,
                                       const float mat[4][4],
                                       const void *src_data,
                                       void *dst_data)
{
  const float *src = static_cast<const float *>(src_data);
  float *dst = static_cast<float *>(dst_data);
  LISTBASE_FOREACH (Nurb *, nu, nurb) {
    if (nu->bezt) {
      for (int a = nu->pntsu; a; a--) {
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
      for (int a = nu->pntsu * nu->pntsv; a; a--) {
        mul_v3_m4v3(dst, mat, src);
        dst[3] = src[3];
        dst[4] = src[4];
        src += KEYELEM_FLOAT_LEN_BPOINT;
        dst += KEYELEM_FLOAT_LEN_BPOINT;
      }
    }
  }
}

void BKE_keyblock_convert_from_curve(const Curve *cu, KeyBlock *kb, const ListBase *nurb)
{
  int tot;

  /* count */
  tot = BKE_keyblock_curve_element_count(nurb);
  if (tot == 0) {
    return;
  }

  MEM_SAFE_FREE(kb->data);

  kb->data = MEM_malloc_arrayN(size_t(tot), size_t(cu->key->elemsize), __func__);
  kb->totelem = tot;

  BKE_keyblock_update_from_curve(cu, kb, nurb);
}

static void keyblock_data_convert_to_curve(const float *fp, ListBase *nurb, int totpoint)
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

void BKE_keyblock_convert_to_curve(KeyBlock *kb, Curve * /*cu*/, ListBase *nurb)
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

  const blender::Span<blender::float3> positions = mesh->vert_positions();
  memcpy(kb->data, positions.data(), sizeof(float[3]) * tot);
}

void BKE_keyblock_convert_from_mesh(const Mesh *mesh, const Key *key, KeyBlock *kb)
{
  const int len = mesh->verts_num;

  if (mesh->verts_num == 0) {
    return;
  }

  MEM_SAFE_FREE(kb->data);

  kb->data = MEM_malloc_arrayN(size_t(len), size_t(key->elemsize), __func__);
  kb->totelem = len;

  BKE_keyblock_update_from_mesh(mesh, kb);
}

void BKE_keyblock_convert_to_mesh(const KeyBlock *kb,
                                  blender::MutableSpan<blender::float3> vert_positions)
{
  vert_positions.take_front(kb->totelem)
      .copy_from({static_cast<blender::float3 *>(kb->data), kb->totelem});
}

void BKE_keyblock_mesh_calc_normals(const KeyBlock *kb,
                                    Mesh *mesh,
                                    float (*r_vert_normals)[3],
                                    float (*r_face_normals)[3],
                                    float (*r_loop_normals)[3])
{
  using namespace blender;
  using namespace blender::bke;
  if (r_vert_normals == nullptr && r_face_normals == nullptr && r_loop_normals == nullptr) {
    return;
  }

  blender::Array<blender::float3> positions(mesh->vert_positions());
  BKE_keyblock_convert_to_mesh(kb, positions);
  const blender::OffsetIndices faces = mesh->faces();
  const blender::Span<int> corner_verts = mesh->corner_verts();
  const blender::Span<int> corner_edges = mesh->corner_edges();

  const bool loop_normals_needed = r_loop_normals != nullptr;
  const bool vert_normals_needed = r_vert_normals != nullptr;
  const bool face_normals_needed = r_face_normals != nullptr || vert_normals_needed ||
                                   loop_normals_needed;

  float (*vert_normals)[3] = r_vert_normals;
  float (*face_normals)[3] = r_face_normals;
  bool free_vert_normals = false;
  bool free_face_normals = false;
  if (vert_normals_needed && r_vert_normals == nullptr) {
    vert_normals = MEM_malloc_arrayN<float[3]>(size_t(mesh->verts_num), __func__);
    free_vert_normals = true;
  }
  if (face_normals_needed && r_face_normals == nullptr) {
    face_normals = MEM_malloc_arrayN<float[3]>(size_t(mesh->faces_num), __func__);
    free_face_normals = true;
  }

  if (face_normals_needed) {
    mesh::normals_calc_faces(positions,
                             faces,
                             corner_verts,
                             {reinterpret_cast<blender::float3 *>(face_normals), faces.size()});
  }
  if (vert_normals_needed) {
    mesh::normals_calc_verts(
        positions,
        faces,
        corner_verts,
        mesh->vert_to_face_map(),
        {reinterpret_cast<const blender::float3 *>(face_normals), faces.size()},
        {reinterpret_cast<blender::float3 *>(vert_normals), mesh->verts_num});
  }
  if (loop_normals_needed) {
    const AttributeAccessor attributes = mesh->attributes();
    const VArraySpan sharp_edges = *attributes.lookup<bool>("sharp_edge", AttrDomain::Edge);
    const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);
    const VArraySpan custom_normals = *attributes.lookup<short2>("custom_normal",
                                                                 AttrDomain::Corner);
    mesh::normals_calc_corners(
        positions,
        faces,
        corner_verts,
        corner_edges,
        mesh->vert_to_face_map(),
        {reinterpret_cast<blender::float3 *>(face_normals), faces.size()},
        sharp_edges,
        sharp_faces,
        custom_normals,
        nullptr,
        {reinterpret_cast<blender::float3 *>(r_loop_normals), corner_verts.size()});
  }

  if (free_vert_normals) {
    MEM_freeN(vert_normals);
  }
  if (free_face_normals) {
    MEM_freeN(face_normals);
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
      /* remove after, insert before this index */
      kb->relative++;
    }
    else if (kb->relative > org_index && kb->relative <= new_index) {
      /* remove before, insert after this index */
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

  /* First key is always refkey, matches interface and BKE_key_sort */
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

std::optional<blender::Array<bool>> BKE_keyblock_get_dependent_keys(const Key *key,
                                                                    const int index)
{
  if (key->type != KEY_RELATIVE) {
    return std::nullopt;
  }

  const int count = BLI_listbase_count(&key->block);

  if (index < 0 || index >= count) {
    return std::nullopt;
  }

  /* Seed the table with the specified key. */
  blender::Array<bool> marked(count, false);

  marked[index] = true;

  /* Iterative breadth-first search through the key list. This method minimizes
   * the number of scans through the list and is fail-safe vs reference cycles. */
  bool updated, found = false;
  int i;

  do {
    updated = false;

    LISTBASE_FOREACH_INDEX (const KeyBlock *, kb, &key->block, i) {
      if (!marked[i] && kb->relative >= 0 && kb->relative < count && marked[kb->relative]) {
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
