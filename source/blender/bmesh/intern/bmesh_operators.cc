/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BMesh operator access.
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "bmesh.hh"
#include "intern/bmesh_private.hh"

/* forward declarations */
static void bmo_flag_layer_alloc(BMesh *bm);
static void bmo_flag_layer_free(BMesh *bm);
static void bmo_flag_layer_clear(BMesh *bm);
static int bmo_name_to_slotcode(const BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                const char *identifier);
static int bmo_name_to_slotcode_check(const BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                      const char *identifier);

const int BMO_OPSLOT_TYPEINFO[BMO_OP_SLOT_TOTAL_TYPES] = {
    0,                /* 0: #BMO_OP_SLOT_SENTINEL */
    sizeof(int),      /* 1: #BMO_OP_SLOT_BOOL */
    sizeof(int),      /* 2: #BMO_OP_SLOT_INT */
    sizeof(float),    /* 3: #BMO_OP_SLOT_FLT */
    sizeof(void *),   /* 4: #BMO_OP_SLOT_PNT */
    sizeof(void *),   /* 5: #BMO_OP_SLOT_PNT */
    0,                /* 6: unused */
    0,                /* 7: unused */
    sizeof(float[3]), /* 8: #BMO_OP_SLOT_VEC */
    sizeof(void *),   /* 9: #BMO_OP_SLOT_ELEMENT_BUF */
    sizeof(void *),   /* 10: #BMO_OP_SLOT_MAPPING */
};

/* Dummy slot so there is something to return when slot name lookup fails */
// static BMOpSlot BMOpEmptySlot = {0};

void BMO_op_flag_enable(BMesh * /*bm*/, BMOperator *op, const int op_flag)
{
  op->flag |= op_flag;
}

void BMO_op_flag_disable(BMesh * /*bm*/, BMOperator *op, const int op_flag)
{
  op->flag &= ~op_flag;
}

void BMO_push(BMesh *bm, BMOperator * /*op*/)
{
  bm->toolflag_index++;

  BLI_assert(bm->totflags > 0);

  /* add flag layer, if appropriate */
  if (bm->toolflag_index > 0) {
    bmo_flag_layer_alloc(bm);
  }
  else {
    bmo_flag_layer_clear(bm);
  }
}

void BMO_pop(BMesh *bm)
{
  if (bm->toolflag_index > 0) {
    bmo_flag_layer_free(bm);
  }

  bm->toolflag_index--;
}

/* use for both slot_types_in and slot_types_out */
static void bmo_op_slots_init(const BMOSlotType *slot_types, BMOpSlot *slot_args)
{
  BMOpSlot *slot;
  uint i;
  for (i = 0; slot_types[i].type; i++) {
    slot = &slot_args[i];
    slot->slot_name = slot_types[i].name;
    slot->slot_type = slot_types[i].type;
    slot->slot_subtype = slot_types[i].subtype;
    // slot->index = i;  // UNUSED

    switch (slot->slot_type) {
      case BMO_OP_SLOT_MAPPING:
        slot->data.ghash = BLI_ghash_ptr_new("bmesh slot map hash");
        break;
      case BMO_OP_SLOT_INT:
        if (ELEM(slot->slot_subtype.intg,
                 BMO_OP_SLOT_SUBTYPE_INT_ENUM,
                 BMO_OP_SLOT_SUBTYPE_INT_FLAG))
        {
          slot->data.enum_data.flags = slot_types[i].enum_flags;
          /* Set the first value of the enum as the default value. */
          slot->data.i = slot->data.enum_data.flags[0].value;
        }
      default:
        break;
    }
  }
}

static void bmo_op_slots_free(const BMOSlotType *slot_types, BMOpSlot *slot_args)
{
  BMOpSlot *slot;
  uint i;
  for (i = 0; slot_types[i].type; i++) {
    slot = &slot_args[i];
    switch (slot->slot_type) {
      case BMO_OP_SLOT_MAPPING:
        BLI_ghash_free(slot->data.ghash, nullptr, nullptr);
        break;
      default:
        break;
    }
  }
}

void BMO_op_init(BMesh *bm, BMOperator *op, const int flag, const char *opname)
{
  int opcode = BMO_opcode_from_opname(opname);

#ifndef NDEBUG
  BM_ELEM_INDEX_VALIDATE(bm, "pre bmo", opname);
#else
  (void)bm;
#endif

  if (opcode == -1) {
    opcode = 0; /* error!, already printed, have a better way to handle this? */
  }

  memset(op, 0, sizeof(BMOperator));
  op->type = opcode;
  op->type_flag = bmo_opdefines[opcode]->type_flag;
  op->flag = flag;

  /* initialize the operator slot types */
  bmo_op_slots_init(bmo_opdefines[opcode]->slot_types_in, op->slots_in);
  bmo_op_slots_init(bmo_opdefines[opcode]->slot_types_out, op->slots_out);

  /* callback */
  op->exec = bmo_opdefines[opcode]->exec;

  /* memarena, used for operator's slot buffers */
  op->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  BLI_memarena_use_calloc(op->arena);

  if (bmo_opdefines[opcode]->init) {
    bmo_opdefines[opcode]->init(op);
  }
}

void BMO_op_exec(BMesh *bm, BMOperator *op)
{
  /* allocate tool flags on demand */
  BM_mesh_elem_toolflags_ensure(bm);

  BMO_push(bm, op);

  if (bm->toolflag_index == 1) {
    bmesh_edit_begin(bm, op->type_flag);
  }
  op->exec(bm, op);

  if (bm->toolflag_index == 1) {
    bmesh_edit_end(bm, op->type_flag);
  }

  BMO_pop(bm);
}

void BMO_op_finish(BMesh *bm, BMOperator *op)
{
  bmo_op_slots_free(bmo_opdefines[op->type]->slot_types_in, op->slots_in);
  bmo_op_slots_free(bmo_opdefines[op->type]->slot_types_out, op->slots_out);

  BLI_memarena_free(op->arena);

#ifndef NDEBUG
  BM_ELEM_INDEX_VALIDATE(bm, "post bmo", bmo_opdefines[op->type]->opname);

  /* avoid accidental re-use */
  memset(op, 0xff, sizeof(*op));
#else
  (void)bm;
#endif
}

bool BMO_slot_exists(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier)
{
  int slot_code = bmo_name_to_slotcode(slot_args, identifier);
  return (slot_code >= 0);
}

BMOpSlot *BMO_slot_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier)
{
  int slot_code = bmo_name_to_slotcode_check(slot_args, identifier);

  if (UNLIKELY(slot_code < 0)) {
    // return &BMOpEmptySlot;
    BLI_assert(0);
    return nullptr; /* better crash */
  }

  return &slot_args[slot_code];
}

void _bmo_slot_copy(BMOpSlot slot_args_src[BMO_OP_MAX_SLOTS],
                    const char *slot_name_src,
                    BMOpSlot slot_args_dst[BMO_OP_MAX_SLOTS],
                    const char *slot_name_dst,
                    MemArena *arena_dst)
{
  BMOpSlot *slot_src = BMO_slot_get(slot_args_src, slot_name_src);
  BMOpSlot *slot_dst = BMO_slot_get(slot_args_dst, slot_name_dst);

  if (slot_src == slot_dst) {
    return;
  }

  BLI_assert(slot_src->slot_type == slot_dst->slot_type);
  if (slot_src->slot_type != slot_dst->slot_type) {
    return;
  }

  if (slot_dst->slot_type == BMO_OP_SLOT_ELEMENT_BUF) {
    /* do buffer copy */
    slot_dst->data.buf = nullptr;
    slot_dst->len = slot_src->len;
    if (slot_dst->len) {
      /* Check destination has all flags enabled that the source has. */
      const eBMOpSlotSubType_Elem src_elem_flag = eBMOpSlotSubType_Elem(
          (slot_src->slot_subtype.elem & BM_ALL_NOLOOP));
      const eBMOpSlotSubType_Elem dst_elem_flag = eBMOpSlotSubType_Elem(
          (slot_dst->slot_subtype.elem & BM_ALL_NOLOOP));

      if ((src_elem_flag | dst_elem_flag) == dst_elem_flag) {
        /* pass */
      }
      else {
        /* check types */
        const uint tot = slot_src->len;
        uint i;
        uint out = 0;
        BMElem **ele_src = (BMElem **)slot_src->data.buf;
        for (i = 0; i < tot; i++, ele_src++) {
          if ((*ele_src)->head.htype & dst_elem_flag) {
            out++;
          }
        }
        if (out != tot) {
          slot_dst->len = out;
        }
      }

      if (slot_dst->len) {
        const int slot_alloc_size = BMO_OPSLOT_TYPEINFO[slot_dst->slot_type] * slot_dst->len;
        slot_dst->data.buf = static_cast<void **>(BLI_memarena_alloc(arena_dst, slot_alloc_size));
        if (slot_src->len == slot_dst->len) {
          memcpy(slot_dst->data.buf, slot_src->data.buf, slot_alloc_size);
        }
        else {
          /* only copy compatible elements */
          const uint tot = slot_src->len;
          uint i;
          BMElem **ele_src = (BMElem **)slot_src->data.buf;
          BMElem **ele_dst = (BMElem **)slot_dst->data.buf;
          for (i = 0; i < tot; i++, ele_src++) {
            if ((*ele_src)->head.htype & dst_elem_flag) {
              *ele_dst = *ele_src;
              ele_dst++;
            }
          }
        }
      }
    }
  }
  else if (slot_dst->slot_type == BMO_OP_SLOT_MAPPING) {
    GHashIterator gh_iter;
    GHASH_ITER (gh_iter, slot_src->data.ghash) {
      void *key = BLI_ghashIterator_getKey(&gh_iter);
      void *val = BLI_ghashIterator_getValue(&gh_iter);
      BLI_ghash_insert(slot_dst->data.ghash, key, val);
    }
  }
  else {
    slot_dst->data = slot_src->data;
  }
}

/*
 * BMESH OPSTACK SET XXX
 *
 * Sets the value of a slot depending on its type
 */

void BMO_slot_float_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const float f)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_FLT);
  if (!(slot->slot_type == BMO_OP_SLOT_FLT)) {
    return;
  }

  slot->data.f = f;
}

void BMO_slot_int_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const int i)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_INT);
  if (!(slot->slot_type == BMO_OP_SLOT_INT)) {
    return;
  }

  slot->data.i = i;
}

void BMO_slot_bool_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const bool i)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_BOOL);
  if (!(slot->slot_type == BMO_OP_SLOT_BOOL)) {
    return;
  }

  slot->data.i = i;
}

void BMO_slot_mat_set(BMOperator *op,
                      BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                      const char *slot_name,
                      const float *mat,
                      int size)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_MAT);
  if (!(slot->slot_type == BMO_OP_SLOT_MAT)) {
    return;
  }

  slot->len = 4;
  slot->data.p = BLI_memarena_alloc(op->arena, sizeof(float[4][4]));

  if (size == 4) {
    copy_m4_m4(static_cast<float (*)[4]>(slot->data.p), (const float (*)[4])mat);
  }
  else if (size == 3) {
    copy_m4_m3(static_cast<float (*)[4]>(slot->data.p), (const float (*)[3])mat);
  }
  else {
    fprintf(stderr, "%s: invalid size argument %d (bmesh internal error)\n", __func__, size);

    zero_m4(static_cast<float (*)[4]>(slot->data.p));
  }
}

void BMO_slot_mat4_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                       const char *slot_name,
                       float r_mat[4][4])
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_MAT);
  if (!(slot->slot_type == BMO_OP_SLOT_MAT)) {
    return;
  }

  if (slot->data.p) {
    copy_m4_m4(r_mat, BMO_SLOT_AS_MATRIX(slot));
  }
  else {
    unit_m4(r_mat);
  }
}

void BMO_slot_mat3_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                       const char *slot_name,
                       float r_mat[3][3])
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_MAT);
  if (!(slot->slot_type == BMO_OP_SLOT_MAT)) {
    return;
  }

  if (slot->data.p) {
    copy_m3_m4(r_mat, BMO_SLOT_AS_MATRIX(slot));
  }
  else {
    unit_m3(r_mat);
  }
}

void BMO_slot_ptr_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, void *p)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_PTR);
  if (!(slot->slot_type == BMO_OP_SLOT_PTR)) {
    return;
  }

  slot->data.p = p;
}

void BMO_slot_vec_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                      const char *slot_name,
                      const float vec[3])
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_VEC);
  if (!(slot->slot_type == BMO_OP_SLOT_VEC)) {
    return;
  }

  copy_v3_v3(slot->data.vec, vec);
}

float BMO_slot_float_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_FLT);
  if (!(slot->slot_type == BMO_OP_SLOT_FLT)) {
    return 0.0f;
  }

  return slot->data.f;
}

int BMO_slot_int_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_INT);
  if (!(slot->slot_type == BMO_OP_SLOT_INT)) {
    return 0;
  }

  return slot->data.i;
}

bool BMO_slot_bool_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_BOOL);
  if (!(slot->slot_type == BMO_OP_SLOT_BOOL)) {
    return false;
  }

  return slot->data.i;
}

void *BMO_slot_as_arrayN(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, int *len)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  void **ret;

  /* could add support for mapping type */
  BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);

  ret = MEM_malloc_arrayN<void *>(slot->len, __func__);
  memcpy(ret, slot->data.buf, sizeof(void *) * slot->len);
  *len = slot->len;
  return ret;
}

void *BMO_slot_ptr_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_PTR);
  if (!(slot->slot_type == BMO_OP_SLOT_PTR)) {
    return nullptr;
  }

  return slot->data.p;
}

void BMO_slot_vec_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, float r_vec[3])
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_VEC);
  if (!(slot->slot_type == BMO_OP_SLOT_VEC)) {
    return;
  }

  copy_v3_v3(r_vec, slot->data.vec);
}

/*
 * BMO_COUNTFLAG
 *
 * Counts the number of elements of a certain type that have a
 * specific flag enabled (or disabled if test_for_enabled is false).
 */

static int bmo_mesh_flag_count(BMesh *bm,
                               const char htype,
                               const short oflag,
                               const bool test_for_enabled)
{
  int count_vert = 0, count_edge = 0, count_face = 0;

  if (htype & BM_VERT) {
    BMIter iter;
    BMVert *ele;
    BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
      if (BMO_vert_flag_test_bool(bm, ele, oflag) == test_for_enabled) {
        count_vert++;
      }
    }
  }
  if (htype & BM_EDGE) {
    BMIter iter;
    BMEdge *ele;
    BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
      if (BMO_edge_flag_test_bool(bm, ele, oflag) == test_for_enabled) {
        count_edge++;
      }
    }
  }
  if (htype & BM_FACE) {
    BMIter iter;
    BMFace *ele;
    BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
      if (BMO_face_flag_test_bool(bm, ele, oflag) == test_for_enabled) {
        count_face++;
      }
    }
  }

  return (count_vert + count_edge + count_face);
}

int BMO_mesh_enabled_flag_count(BMesh *bm, const char htype, const short oflag)
{
  return bmo_mesh_flag_count(bm, htype, oflag, true);
}

int BMO_mesh_disabled_flag_count(BMesh *bm, const char htype, const short oflag)
{
  return bmo_mesh_flag_count(bm, htype, oflag, false);
}

void BMO_mesh_flag_disable_all(BMesh *bm, BMOperator * /*op*/, const char htype, const short oflag)
{
  if (htype & BM_VERT) {
    BMIter iter;
    BMVert *ele;
    BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
      BMO_vert_flag_disable(bm, ele, oflag);
    }
  }
  if (htype & BM_EDGE) {
    BMIter iter;
    BMEdge *ele;
    BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
      BMO_edge_flag_disable(bm, ele, oflag);
    }
  }
  if (htype & BM_FACE) {
    BMIter iter;
    BMFace *ele;
    BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
      BMO_face_flag_disable(bm, ele, oflag);
    }
  }
}

void BMO_mesh_selected_remap(BMesh *bm,
                             BMOpSlot *slot_vert_map,
                             BMOpSlot *slot_edge_map,
                             BMOpSlot *slot_face_map,
                             const bool check_select)
{
  if (bm->selected.first) {
    BMEditSelection *ese, *ese_next;
    BMOpSlot *slot_elem_map;

    for (ese = static_cast<BMEditSelection *>(bm->selected.first); ese; ese = ese_next) {
      ese_next = ese->next;

      switch (ese->htype) {
        case BM_VERT:
          slot_elem_map = slot_vert_map;
          break;
        case BM_EDGE:
          slot_elem_map = slot_edge_map;
          break;
        default:
          slot_elem_map = slot_face_map;
          break;
      }

      ese->ele = static_cast<BMElem *>(BMO_slot_map_elem_get(slot_elem_map, ese->ele));

      if (UNLIKELY((ese->ele == nullptr) ||
                   (check_select && (BM_elem_flag_test(ese->ele, BM_ELEM_SELECT) == false))))
      {
        BLI_remlink(&bm->selected, ese);
        MEM_freeN(ese);
      }
    }
  }

  if (bm->act_face) {
    BMFace *f = static_cast<BMFace *>(BMO_slot_map_elem_get(slot_face_map, bm->act_face));
    if (f) {
      bm->act_face = f;
    }
  }
}

int BMO_slot_buffer_len(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);

  /* check if its actually a buffer */
  if (slot->slot_type != BMO_OP_SLOT_ELEMENT_BUF) {
    return 0;
  }

  return slot->len;
}

int BMO_slot_map_len(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_MAPPING);
  return BLI_ghash_len(slot->data.ghash);
}

void BMO_slot_map_insert(BMOperator *op, BMOpSlot *slot, const void *element, const void *data)
{
  (void)op; /* Ignored in release builds. */

  BLI_assert(slot->slot_type == BMO_OP_SLOT_MAPPING);
  BMO_ASSERT_SLOT_IN_OP(slot, op);

  BLI_ghash_insert(slot->data.ghash, (void *)element, (void *)data);
}

#if 0
void *bmo_slot_buffer_grow(BMesh *bm, BMOperator *op, int slot_code, int totadd)
{
  BMOpSlot *slot = &op->slots[slot_code];
  void *tmp;
  int64_t allocsize;

  BLI_assert(slot->slottype == BMO_OP_SLOT_ELEMENT_BUF);

  /* check if its actually a buffer */
  if (slot->slottype != BMO_OP_SLOT_ELEMENT_BUF) {
    return nullptr;
  }

  if (slot->flag & BMOS_DYNAMIC_ARRAY) {
    if (slot->len >= slot->size) {
      slot->size = (slot->size + 1 + totadd) * 2;

      allocsize = BMO_OPSLOT_TYPEINFO[bmo_opdefines[op->type]->slot_types[slot_code].type] *
                  slot->size;
      slot->data.buf = MEM_recallocN_id(slot->data.buf, allocsize, "opslot dynamic array");
    }

    slot->len += totadd;
  }
  else {
    slot->flag |= BMOS_DYNAMIC_ARRAY;
    slot->len += totadd;
    slot->size = slot->len + 2;

    allocsize = BMO_OPSLOT_TYPEINFO[bmo_opdefines[op->type]->slot_types[slot_code].type] *
                slot->len;

    tmp = slot->data.buf;
    slot->data.buf = MEM_callocN(allocsize, "opslot dynamic array");
    memcpy(slot->data.buf, tmp, allocsize);
  }

  return slot->data.buf;
}
#endif

void BMO_slot_map_to_flag(BMesh *bm,
                          BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                          const char *slot_name,
                          const char htype,
                          const short oflag)
{
  GHashIterator gh_iter;
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BMElemF *ele_f;

  BLI_assert(slot->slot_type == BMO_OP_SLOT_MAPPING);

  GHASH_ITER (gh_iter, slot->data.ghash) {
    ele_f = static_cast<BMElemF *>(BLI_ghashIterator_getKey(&gh_iter));
    if (ele_f->head.htype & htype) {
      BMO_elem_flag_enable(bm, ele_f, oflag);
    }
  }
}

void *BMO_slot_buffer_alloc(BMOperator *op,
                            BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                            const char *slot_name,
                            const int len)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);

  /* check if its actually a buffer */
  if (slot->slot_type != BMO_OP_SLOT_ELEMENT_BUF) {
    return nullptr;
  }

  slot->len = len;
  if (len) {
    slot->data.buf = static_cast<void **>(
        BLI_memarena_alloc(op->arena, BMO_OPSLOT_TYPEINFO[slot->slot_type] * len));
  }
  else {
    slot->data.buf = nullptr;
  }

  return slot->data.buf;
}

void BMO_slot_buffer_from_all(BMesh *bm,
                              BMOperator *op,
                              BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                              const char *slot_name,
                              const char htype)
{
  BMOpSlot *output = BMO_slot_get(slot_args, slot_name);
  int totelement = 0, i = 0;

  BLI_assert(output->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
  BLI_assert(((output->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);

  if (htype & BM_VERT) {
    totelement += bm->totvert;
  }
  if (htype & BM_EDGE) {
    totelement += bm->totedge;
  }
  if (htype & BM_FACE) {
    totelement += bm->totface;
  }

  if (totelement) {
    BMIter iter;
    BMHeader *ele;

    BMO_slot_buffer_alloc(op, slot_args, slot_name, totelement);

    /* TODO: collapse these loops into one. */

    if (htype & BM_VERT) {
      BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
        output->data.buf[i] = ele;
        i++;
      }
    }

    if (htype & BM_EDGE) {
      BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
        output->data.buf[i] = ele;
        i++;
      }
    }

    if (htype & BM_FACE) {
      BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
        output->data.buf[i] = ele;
        i++;
      }
    }
  }
}

/**
 * \brief BMO_HEADERFLAG_TO_SLOT
 *
 * Copies elements of a certain type, which have a certain header flag
 * enabled/disabled into a slot for an operator.
 */
static void bmo_slot_buffer_from_hflag(BMesh *bm,
                                       BMOperator *op,
                                       BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                       const char *slot_name,
                                       const char htype,
                                       const char hflag,
                                       const bool test_for_enabled)
{
  BMOpSlot *output = BMO_slot_get(slot_args, slot_name);
  int totelement = 0, i = 0;
  const bool respecthide = ((op->flag & BMO_FLAG_RESPECT_HIDE) != 0) &&
                           ((hflag & BM_ELEM_HIDDEN) == 0);

  BLI_assert(output->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
  BLI_assert(((output->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);
  BLI_assert((output->slot_subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE) == 0);

  if (test_for_enabled) {
    totelement = BM_mesh_elem_hflag_count_enabled(bm, htype, hflag, respecthide);
  }
  else {
    totelement = BM_mesh_elem_hflag_count_disabled(bm, htype, hflag, respecthide);
  }

  if (totelement) {
    BMIter iter;
    BMElem *ele;

    BMO_slot_buffer_alloc(op, slot_args, slot_name, totelement);

    /* TODO: collapse these loops into one. */

    if (htype & BM_VERT) {
      BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
        if ((!respecthide || !BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) &&
            BM_elem_flag_test_bool(ele, hflag) == test_for_enabled)
        {
          output->data.buf[i] = ele;
          i++;
        }
      }
    }

    if (htype & BM_EDGE) {
      BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
        if ((!respecthide || !BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) &&
            BM_elem_flag_test_bool(ele, hflag) == test_for_enabled)
        {
          output->data.buf[i] = ele;
          i++;
        }
      }
    }

    if (htype & BM_FACE) {
      BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
        if ((!respecthide || !BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) &&
            BM_elem_flag_test_bool(ele, hflag) == test_for_enabled)
        {
          output->data.buf[i] = ele;
          i++;
        }
      }
    }
  }
  else {
    output->len = 0;
  }
}

void BMO_slot_buffer_from_enabled_hflag(BMesh *bm,
                                        BMOperator *op,
                                        BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                        const char *slot_name,
                                        const char htype,
                                        const char hflag)
{
  bmo_slot_buffer_from_hflag(bm, op, slot_args, slot_name, htype, hflag, true);
}

void BMO_slot_buffer_from_disabled_hflag(BMesh *bm,
                                         BMOperator *op,
                                         BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                         const char *slot_name,
                                         const char htype,
                                         const char hflag)
{
  bmo_slot_buffer_from_hflag(bm, op, slot_args, slot_name, htype, hflag, false);
}

void BMO_slot_buffer_from_single(BMOperator *op, BMOpSlot *slot, BMHeader *ele)
{
  BMO_ASSERT_SLOT_IN_OP(slot, op);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
  BLI_assert(slot->slot_subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE);
  BLI_assert(ELEM(slot->len, 0, 1));

  BLI_assert(slot->slot_subtype.elem & ele->htype);

  slot->data.buf = static_cast<void **>(
      BLI_memarena_alloc(op->arena, sizeof(void *) * 4)); /* XXX, why 'x4' ? */
  slot->len = 1;
  *slot->data.buf = ele;
}

void BMO_slot_buffer_from_array(BMOperator *op,
                                BMOpSlot *slot,
                                BMHeader **ele_buffer,
                                int ele_buffer_len)
{
  BMO_ASSERT_SLOT_IN_OP(slot, op);
  BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
  BLI_assert(ELEM(slot->len, 0, ele_buffer_len));

  if (slot->data.buf == nullptr) {
    slot->data.buf = static_cast<void **>(
        BLI_memarena_alloc(op->arena, sizeof(*slot->data.buf) * ele_buffer_len));
  }

  slot->len = ele_buffer_len;
  memcpy(slot->data.buf, ele_buffer, ele_buffer_len * sizeof(*slot->data.buf));
}

void *BMO_slot_buffer_get_single(BMOpSlot *slot)
{
  BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
  BLI_assert(slot->slot_subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE);
  BLI_assert(ELEM(slot->len, 0, 1));

  return slot->len ? (BMHeader *)slot->data.buf[0] : nullptr;
}

void _bmo_slot_buffer_append(BMOpSlot slot_args_dst[BMO_OP_MAX_SLOTS],
                             const char *slot_name_dst,
                             BMOpSlot slot_args_src[BMO_OP_MAX_SLOTS],
                             const char *slot_name_src,
                             MemArena *arena_dst)
{
  BMOpSlot *slot_dst = BMO_slot_get(slot_args_dst, slot_name_dst);
  BMOpSlot *slot_src = BMO_slot_get(slot_args_src, slot_name_src);

  BLI_assert(slot_dst->slot_type == BMO_OP_SLOT_ELEMENT_BUF &&
             slot_src->slot_type == BMO_OP_SLOT_ELEMENT_BUF);

  if (slot_dst->len == 0) {
    /* output slot is empty, copy rather than append */
    _bmo_slot_copy(slot_args_src, slot_name_src, slot_args_dst, slot_name_dst, arena_dst);
  }
  else if (slot_src->len != 0) {
    int elem_size = BMO_OPSLOT_TYPEINFO[slot_dst->slot_type];
    int alloc_size = elem_size * (slot_dst->len + slot_src->len);
    /* allocate new buffer */
    void *buf = BLI_memarena_alloc(arena_dst, alloc_size);

    /* copy slot data */
    memcpy(buf, slot_dst->data.buf, elem_size * slot_dst->len);
    memcpy(
        ((char *)buf) + elem_size * slot_dst->len, slot_src->data.buf, elem_size * slot_src->len);

    slot_dst->data.buf = static_cast<void **>(buf);
    slot_dst->len += slot_src->len;
  }
}

/**
 * \brief BMO_FLAG_TO_SLOT
 *
 * Copies elements of a certain type, which have a certain flag set
 * into an output slot for an operator.
 */
static void bmo_slot_buffer_from_flag(BMesh *bm,
                                      BMOperator *op,
                                      BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                      const char *slot_name,
                                      const char htype,
                                      const short oflag,
                                      const bool test_for_enabled)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  int totelement, i = 0;

  BLI_assert(op->slots_in == slot_args || op->slots_out == slot_args);

  BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
  BLI_assert(((slot->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);
  BLI_assert((slot->slot_subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE) == 0);

  if (test_for_enabled) {
    totelement = BMO_mesh_enabled_flag_count(bm, htype, oflag);
  }
  else {
    totelement = BMO_mesh_disabled_flag_count(bm, htype, oflag);
  }

  if (totelement) {
    BMIter iter;
    BMHeader *ele;
    BMHeader **ele_array;

    BMO_slot_buffer_alloc(op, slot_args, slot_name, totelement);

    ele_array = (BMHeader **)slot->data.buf;

    /* TODO: collapse these loops into one. */

    if (htype & BM_VERT) {
      BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
        if (BMO_vert_flag_test_bool(bm, (BMVert *)ele, oflag) == test_for_enabled) {
          ele_array[i] = ele;
          i++;
        }
      }
    }

    if (htype & BM_EDGE) {
      BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
        if (BMO_edge_flag_test_bool(bm, (BMEdge *)ele, oflag) == test_for_enabled) {
          ele_array[i] = ele;
          i++;
        }
      }
    }

    if (htype & BM_FACE) {
      BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
        if (BMO_face_flag_test_bool(bm, (BMFace *)ele, oflag) == test_for_enabled) {
          ele_array[i] = ele;
          i++;
        }
      }
    }
  }
  else {
    slot->len = 0;
  }
}

void BMO_slot_buffer_from_enabled_flag(BMesh *bm,
                                       BMOperator *op,
                                       BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                       const char *slot_name,
                                       const char htype,
                                       const short oflag)
{
  bmo_slot_buffer_from_flag(bm, op, slot_args, slot_name, htype, oflag, true);
}

void BMO_slot_buffer_from_disabled_flag(BMesh *bm,
                                        BMOperator *op,
                                        BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                        const char *slot_name,
                                        const char htype,
                                        const short oflag)
{
  bmo_slot_buffer_from_flag(bm, op, slot_args, slot_name, htype, oflag, false);
}

void BMO_slot_buffer_hflag_enable(BMesh *bm,
                                  BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                  const char *slot_name,
                                  const char htype,
                                  const char hflag,
                                  const bool do_flush)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BMElem **data = (BMElem **)slot->data.buf;
  int i;
  const bool do_flush_select = (do_flush && (hflag & BM_ELEM_SELECT));
  const bool do_flush_hide = (do_flush && (hflag & BM_ELEM_HIDDEN));

  BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
  BLI_assert(((slot->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);
  BLI_assert((slot->slot_subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE) == 0);

  for (i = 0; i < slot->len; i++, data++) {
    if (!(htype & (*data)->head.htype)) {
      continue;
    }

    if (do_flush_select) {
      BM_elem_select_set(bm, *data, true);
    }

    if (do_flush_hide) {
      BM_elem_hide_set(bm, *data, false);
    }

    BM_elem_flag_enable(*data, hflag);
  }
}

void BMO_slot_buffer_hflag_disable(BMesh *bm,
                                   BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                   const char *slot_name,
                                   const char htype,
                                   const char hflag,
                                   const bool do_flush)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BMElem **data = (BMElem **)slot->data.buf;
  int i;
  const bool do_flush_select = (do_flush && (hflag & BM_ELEM_SELECT));
  const bool do_flush_hide = (do_flush && (hflag & BM_ELEM_HIDDEN));

  BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
  BLI_assert(((slot->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);

  for (i = 0; i < slot->len; i++, data++) {
    if (!(htype & (*data)->head.htype)) {
      continue;
    }

    if (do_flush_select) {
      BM_elem_select_set(bm, *data, false);
    }

    if (do_flush_hide) {
      BM_elem_hide_set(bm, *data, false);
    }

    BM_elem_flag_disable(*data, hflag);
  }
}

void BMO_slot_buffer_flag_enable(BMesh *bm,
                                 BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                 const char *slot_name,
                                 const char htype,
                                 const short oflag)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BMHeader **data = static_cast<BMHeader **>(slot->data.p);
  int i;

  BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
  BLI_assert(((slot->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);

  for (i = 0; i < slot->len; i++) {
    if (!(htype & data[i]->htype)) {
      continue;
    }

    BMO_elem_flag_enable(bm, (BMElemF *)data[i], oflag);
  }
}

void BMO_slot_buffer_flag_disable(BMesh *bm,
                                  BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                  const char *slot_name,
                                  const char htype,
                                  const short oflag)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
  BMHeader **data = (BMHeader **)slot->data.buf;
  int i;

  BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
  BLI_assert(((slot->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);

  for (i = 0; i < slot->len; i++) {
    if (!(htype & data[i]->htype)) {
      continue;
    }

    BMO_elem_flag_disable(bm, (BMElemF *)data[i], oflag);
  }
}

/**
 * \brief ALLOC/FREE FLAG LAYER
 *
 * Used by operator stack to free/allocate
 * private flag data. This is allocated
 * using a mempool so the allocation/frees
 * should be quite fast.
 *
 * BMESH_TODO:
 * Investigate not freeing flag layers until
 * all operators have been executed. This would
 * save a lot of realloc potentially.
 */
static void bmo_flag_layer_alloc(BMesh *bm)
{
  /* set the index values since we are looping over all data anyway,
   * may save time later on */

  BLI_mempool *voldpool = bm->vtoolflagpool; /* old flag pool */
  BLI_mempool *eoldpool = bm->etoolflagpool; /* old flag pool */
  BLI_mempool *foldpool = bm->ftoolflagpool; /* old flag pool */

  /* store memcpy size for reuse */
  const size_t old_totflags_size = (bm->totflags * sizeof(BMFlagLayer));

  bm->totflags++;

  bm->vtoolflagpool = BLI_mempool_create(
      sizeof(BMFlagLayer) * bm->totflags, bm->totvert, 512, BLI_MEMPOOL_NOP);
  bm->etoolflagpool = BLI_mempool_create(
      sizeof(BMFlagLayer) * bm->totflags, bm->totedge, 512, BLI_MEMPOOL_NOP);
  bm->ftoolflagpool = BLI_mempool_create(
      sizeof(BMFlagLayer) * bm->totflags, bm->totface, 512, BLI_MEMPOOL_NOP);

  /* now go through and memcpy all the flags. Loops don't get a flag layer at this time. */
  BMIter iter;
  int i;

  BMVert_OFlag *v_oflag;
  BLI_mempool *newpool = bm->vtoolflagpool;
  BM_ITER_MESH_INDEX (v_oflag, &iter, bm, BM_VERTS_OF_MESH, i) {
    void *oldflags = v_oflag->oflags;
    v_oflag->oflags = static_cast<BMFlagLayer *>(BLI_mempool_calloc(newpool));
    memcpy(v_oflag->oflags, oldflags, old_totflags_size);
    BM_elem_index_set(&v_oflag->base, i); /* set_inline */
    BM_ELEM_API_FLAG_CLEAR((BMElemF *)v_oflag);
  }

  BMEdge_OFlag *e_oflag;
  newpool = bm->etoolflagpool;
  BM_ITER_MESH_INDEX (e_oflag, &iter, bm, BM_EDGES_OF_MESH, i) {
    void *oldflags = e_oflag->oflags;
    e_oflag->oflags = static_cast<BMFlagLayer *>(BLI_mempool_calloc(newpool));
    memcpy(e_oflag->oflags, oldflags, old_totflags_size);
    BM_elem_index_set(&e_oflag->base, i); /* set_inline */
    BM_ELEM_API_FLAG_CLEAR((BMElemF *)e_oflag);
  }

  BMFace_OFlag *f_oflag;
  newpool = bm->ftoolflagpool;
  BM_ITER_MESH_INDEX (f_oflag, &iter, bm, BM_FACES_OF_MESH, i) {
    void *oldflags = f_oflag->oflags;
    f_oflag->oflags = static_cast<BMFlagLayer *>(BLI_mempool_calloc(newpool));
    memcpy(f_oflag->oflags, oldflags, old_totflags_size);
    BM_elem_index_set(&f_oflag->base, i); /* set_inline */
    BM_ELEM_API_FLAG_CLEAR((BMElemF *)f_oflag);
  }

  BLI_mempool_destroy(voldpool);
  BLI_mempool_destroy(eoldpool);
  BLI_mempool_destroy(foldpool);

  bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE | BM_FACE);
}

static void bmo_flag_layer_free(BMesh *bm)
{
  /* set the index values since we are looping over all data anyway,
   * may save time later on */

  BLI_mempool *voldpool = bm->vtoolflagpool;
  BLI_mempool *eoldpool = bm->etoolflagpool;
  BLI_mempool *foldpool = bm->ftoolflagpool;

  /* store memcpy size for reuse */
  const size_t new_totflags_size = ((bm->totflags - 1) * sizeof(BMFlagLayer));

  /* de-increment the totflags first. */
  bm->totflags--;

  bm->vtoolflagpool = BLI_mempool_create(new_totflags_size, bm->totvert, 512, BLI_MEMPOOL_NOP);
  bm->etoolflagpool = BLI_mempool_create(new_totflags_size, bm->totedge, 512, BLI_MEMPOOL_NOP);
  bm->ftoolflagpool = BLI_mempool_create(new_totflags_size, bm->totface, 512, BLI_MEMPOOL_NOP);

  /* now go through and memcpy all the flag */
  BMIter iter;
  int i;

  BMVert_OFlag *v_oflag;
  BLI_mempool *newpool = bm->vtoolflagpool;
  BM_ITER_MESH_INDEX (v_oflag, &iter, bm, BM_VERTS_OF_MESH, i) {
    void *oldflags = v_oflag->oflags;
    v_oflag->oflags = static_cast<BMFlagLayer *>(BLI_mempool_alloc(newpool));
    memcpy(v_oflag->oflags, oldflags, new_totflags_size);
    BM_elem_index_set(&v_oflag->base, i); /* set_inline */
    BM_ELEM_API_FLAG_CLEAR((BMElemF *)v_oflag);
  }

  BMEdge_OFlag *e_oflag;
  newpool = bm->etoolflagpool;
  BM_ITER_MESH_INDEX (e_oflag, &iter, bm, BM_EDGES_OF_MESH, i) {
    void *oldflags = e_oflag->oflags;
    e_oflag->oflags = static_cast<BMFlagLayer *>(BLI_mempool_alloc(newpool));
    memcpy(e_oflag->oflags, oldflags, new_totflags_size);
    BM_elem_index_set(&e_oflag->base, i); /* set_inline */
    BM_ELEM_API_FLAG_CLEAR((BMElemF *)e_oflag);
  }

  BMFace_OFlag *f_oflag;
  newpool = bm->ftoolflagpool;
  BM_ITER_MESH_INDEX (f_oflag, &iter, bm, BM_FACES_OF_MESH, i) {
    void *oldflags = f_oflag->oflags;
    f_oflag->oflags = static_cast<BMFlagLayer *>(BLI_mempool_alloc(newpool));
    memcpy(f_oflag->oflags, oldflags, new_totflags_size);
    BM_elem_index_set(&f_oflag->base, i); /* set_inline */
    BM_ELEM_API_FLAG_CLEAR((BMElemF *)f_oflag);
  }

  BLI_mempool_destroy(voldpool);
  BLI_mempool_destroy(eoldpool);
  BLI_mempool_destroy(foldpool);

  bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE | BM_FACE);
}

static void bmo_flag_layer_clear(BMesh *bm)
{
  /* set the index values since we are looping over all data anyway,
   * may save time later on */
  const BMFlagLayer zero_flag = {0};

  const int totflags_offset = bm->totflags - 1;

  /* now go through and memcpy all the flag */
  {
    BMIter iter;
    BMVert_OFlag *ele;
    int i;
    BM_ITER_MESH_INDEX (ele, &iter, bm, BM_VERTS_OF_MESH, i) {
      ele->oflags[totflags_offset] = zero_flag;
      BM_elem_index_set(&ele->base, i); /* set_inline */
    }
  }
  {
    BMIter iter;
    BMEdge_OFlag *ele;
    int i;
    BM_ITER_MESH_INDEX (ele, &iter, bm, BM_EDGES_OF_MESH, i) {
      ele->oflags[totflags_offset] = zero_flag;
      BM_elem_index_set(&ele->base, i); /* set_inline */
    }
  }
  {
    BMIter iter;
    BMFace_OFlag *ele;
    int i;
    BM_ITER_MESH_INDEX (ele, &iter, bm, BM_FACES_OF_MESH, i) {
      ele->oflags[totflags_offset] = zero_flag;
      BM_elem_index_set(&ele->base, i); /* set_inline */
    }
  }

  bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE | BM_FACE);
}

void *BMO_slot_buffer_get_first(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);

  if (slot->slot_type != BMO_OP_SLOT_ELEMENT_BUF) {
    return nullptr;
  }

  return slot->data.buf ? *slot->data.buf : nullptr;
}

void *BMO_iter_new(BMOIter *iter,
                   BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                   const char *slot_name,
                   const char restrictmask)
{
  BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);

  memset(iter, 0, sizeof(BMOIter));

  iter->slot = slot;
  iter->cur = 0;
  iter->restrictmask = restrictmask;

  if (iter->slot->slot_type == BMO_OP_SLOT_MAPPING) {
    BLI_ghashIterator_init(&iter->giter, slot->data.ghash);
  }
  else if (iter->slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF) {
    BLI_assert(restrictmask & slot->slot_subtype.elem);
  }
  else {
    BLI_assert(0);
  }

  return BMO_iter_step(iter);
}

void *BMO_iter_step(BMOIter *iter)
{
  BMOpSlot *slot = iter->slot;
  if (slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF) {
    BMHeader *ele;

    if (iter->cur >= slot->len) {
      return nullptr;
    }

    ele = static_cast<BMHeader *>(slot->data.buf[iter->cur++]);
    while (!(iter->restrictmask & ele->htype)) {
      if (iter->cur >= slot->len) {
        return nullptr;
      }

      ele = static_cast<BMHeader *>(slot->data.buf[iter->cur++]);
      BLI_assert((ele == nullptr) || (slot->slot_subtype.elem & ele->htype));
    }

    BLI_assert((ele == nullptr) || (slot->slot_subtype.elem & ele->htype));

    return ele;
  }
  if (slot->slot_type == BMO_OP_SLOT_MAPPING) {
    void *ret;

    if (BLI_ghashIterator_done(&iter->giter) == false) {
      ret = BLI_ghashIterator_getKey(&iter->giter);
      iter->val = BLI_ghashIterator_getValue_p(&iter->giter);

      BLI_ghashIterator_step(&iter->giter);
    }
    else {
      ret = nullptr;
      iter->val = nullptr;
    }

    return ret;
  }
  BLI_assert(0);

  return nullptr;
}

/* used for iterating over mappings */

void **BMO_iter_map_value_p(BMOIter *iter)
{
  return iter->val;
}

void *BMO_iter_map_value_ptr(BMOIter *iter)
{
  BLI_assert(ELEM(iter->slot->slot_subtype.map,
                  BMO_OP_SLOT_SUBTYPE_MAP_ELEM,
                  BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL));
  return iter->val ? *iter->val : nullptr;
}

float BMO_iter_map_value_float(BMOIter *iter)
{
  BLI_assert(iter->slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_FLT);
  return **((float **)iter->val);
}

int BMO_iter_map_value_int(BMOIter *iter)
{
  BLI_assert(iter->slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_INT);
  return **((int **)iter->val);
}

bool BMO_iter_map_value_bool(BMOIter *iter)
{
  BLI_assert(iter->slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_BOOL);
  return **((bool **)iter->val);
}

/* error system */
struct BMOpError {
  BMOpError *next, *prev;
  BMOperator *op;
  const char *msg;
  eBMOpErrorLevel level;
};

void BMO_error_clear(BMesh *bm)
{
  while (BMO_error_pop(bm, nullptr, nullptr, nullptr)) {
    /* pass */
  }
}

void BMO_error_raise(BMesh *bm, BMOperator *owner, eBMOpErrorLevel level, const char *msg)
{
  BMOpError *err = MEM_callocN<BMOpError>("bmop_error");

  err->msg = msg;
  err->op = owner;
  err->level = level;

  BLI_addhead(&bm->errorstack, err);
}

bool BMO_error_occurred_at_level(BMesh *bm, eBMOpErrorLevel level)
{
  for (const BMOpError *err = static_cast<const BMOpError *>(bm->errorstack.first); err;
       err = err->next)
  {
    if (err->level == level) {
      return true;
    }
  }
  return false;
}

bool BMO_error_get(BMesh *bm, const char **r_msg, BMOperator **r_op, eBMOpErrorLevel *r_level)
{
  BMOpError *err = static_cast<BMOpError *>(bm->errorstack.first);
  if (err == nullptr) {
    return false;
  }

  if (r_msg) {
    *r_msg = err->msg;
  }
  if (r_op) {
    *r_op = err->op;
  }
  if (r_level) {
    *r_level = err->level;
  }

  return true;
}

bool BMO_error_get_at_level(BMesh *bm,
                            eBMOpErrorLevel level,
                            const char **r_msg,
                            BMOperator **r_op)
{
  LISTBASE_FOREACH (BMOpError *, err, &bm->errorstack) {
    if (err->level >= level) {
      if (r_msg) {
        *r_msg = err->msg;
      }
      if (r_op) {
        *r_op = err->op;
      }
      return true;
    }
  }

  return false;
}

bool BMO_error_pop(BMesh *bm, const char **r_msg, BMOperator **r_op, eBMOpErrorLevel *r_level)
{
  bool result = BMO_error_get(bm, r_msg, r_op, r_level);

  if (result) {
    BMOpError *err = static_cast<BMOpError *>(bm->errorstack.first);

    BLI_remlink(&bm->errorstack, bm->errorstack.first);
    MEM_freeN(err);
  }

  return result;
}

#define NEXT_CHAR(fmt) ((fmt)[0] != 0 ? (fmt)[1] : 0)

static int bmo_name_to_slotcode(const BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier)
{
  int i = 0;

  while (slot_args->slot_name) {
    if (STREQLEN(identifier, slot_args->slot_name, MAX_SLOTNAME)) {
      return i;
    }
    slot_args++;
    i++;
  }

  return -1;
}

static int bmo_name_to_slotcode_check(const BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                      const char *identifier)
{
  int i = bmo_name_to_slotcode(slot_args, identifier);
  if (i < 0) {
    fprintf(stderr,
            "%s: ! could not find bmesh slot for name %s! (bmesh internal error)\n",
            __func__,
            identifier);
  }

  return i;
}

int BMO_opcode_from_opname(const char *opname)
{

  const uint tot = bmo_opdefines_total;
  uint i;
  for (i = 0; i < tot; i++) {
    if (STREQ(bmo_opdefines[i]->opname, opname)) {
      return i;
    }
  }
  return -1;
}

static int BMO_opcode_from_opname_check(const char *opname)
{
  int i = BMO_opcode_from_opname(opname);
  if (i == -1) {
    fprintf(stderr,
            "%s: could not find bmesh slot for name %s! (bmesh internal error)\n",
            __func__,
            opname);
  }
  return i;
}

bool BMO_op_vinitf(BMesh *bm, BMOperator *op, const int flag, const char *_fmt, va_list vlist)
{
  //  BMOpDefine *def;
  char *opname, *ofmt, *fmt;
  char slot_name[64] = {0};
  int i, type;
  bool noslot, state;

  /* basic useful info to help find where bmop formatting strings fail */
  const char *err_reason = "Unknown";
  int lineno = -1;

#define GOTO_ERROR(reason) \
  { \
    err_reason = reason; \
    lineno = __LINE__; \
    goto error; \
  } \
  (void)0

  /* we muck around in here, so dup it */
  fmt = ofmt = BLI_strdup(_fmt);

  /* find operator name */
  i = strcspn(fmt, " ");

  opname = fmt;
  noslot = (opname[i] == '\0');
  opname[i] = '\0';

  fmt += i + (noslot ? 0 : 1);

  i = BMO_opcode_from_opname_check(opname);

  if (i == -1) {
    MEM_freeN(ofmt);
    BLI_assert(0);
    return false;
  }

  BMO_op_init(bm, op, flag, opname);
  //  def = bmo_opdefines[i];

  i = 0;
  state = true; /* false: not inside slot_code name, true: inside slot_code name */

  while (*fmt) {
    if (state) {
      /* Jump past leading white-space. */
      i = strspn(fmt, " ");
      fmt += i;

      /* Ignore trailing white-space. */
      if (!fmt[i]) {
        break;
      }

      /* find end of slot name, only "slot=%f", can be used */
      i = strcspn(fmt, "=");
      if (!fmt[i]) {
        GOTO_ERROR("could not match end of slot name");
      }

      fmt[i] = 0;

      if (bmo_name_to_slotcode_check(op->slots_in, fmt) < 0) {
        GOTO_ERROR("name to slot code check failed");
      }

      STRNCPY(slot_name, fmt);

      state = false;
      fmt += i;
    }
    else {
      switch (*fmt) {
        case ' ':
        case '=':
        case '%':
          break;
        case 'm': {
          int size;
          const char c = NEXT_CHAR(fmt);
          fmt++;

          if (c == '3') {
            size = 3;
          }
          else if (c == '4') {
            size = 4;
          }
          else {
            GOTO_ERROR("matrix size was not 3 or 4");
          }

          BMO_slot_mat_set(op, op->slots_in, slot_name, va_arg(vlist, float *), size);
          state = true;
          break;
        }
        case 'v': {
          BMO_slot_vec_set(op->slots_in, slot_name, va_arg(vlist, float *));
          state = true;
          break;
        }
        case 'e': {
          BMOpSlot *slot = BMO_slot_get(op->slots_in, slot_name);

          if (NEXT_CHAR(fmt) == 'b') {
            BMHeader **ele_buffer = va_arg(vlist, BMHeader **);
            int ele_buffer_len = va_arg(vlist, int);

            BMO_slot_buffer_from_array(op, slot, ele_buffer, ele_buffer_len);
            fmt++;
          }
          else {
            /* single vert/edge/face */
            BMHeader *ele = va_arg(vlist, BMHeader *);

            BMO_slot_buffer_from_single(op, slot, ele);
          }

          state = true;
          break;
        }
        case 's':
        case 'S': {
          BMOperator *op_other = va_arg(vlist, BMOperator *);
          const char *slot_name_other = va_arg(vlist, char *);

          if (*fmt == 's') {
            BLI_assert(bmo_name_to_slotcode_check(op_other->slots_in, slot_name_other) != -1);
            BMO_slot_copy(op_other, slots_in, slot_name_other, op, slots_in, slot_name);
          }
          else {
            BLI_assert(bmo_name_to_slotcode_check(op_other->slots_out, slot_name_other) != -1);
            BMO_slot_copy(op_other, slots_out, slot_name_other, op, slots_in, slot_name);
          }
          state = true;
          break;
        }
        case 'i':
          BMO_slot_int_set(op->slots_in, slot_name, va_arg(vlist, int));
          state = true;
          break;
        case 'b':
          BMO_slot_bool_set(op->slots_in, slot_name, va_arg(vlist, int));
          state = true;
          break;
        case 'p':
          BMO_slot_ptr_set(op->slots_in, slot_name, va_arg(vlist, void *));
          state = true;
          break;
        case 'f':
        case 'F':
        case 'h':
        case 'H':
        case 'a':
          type = *fmt;

          if (NEXT_CHAR(fmt) == ' ' || NEXT_CHAR(fmt) == '\0') {
            BMO_slot_float_set(op->slots_in, slot_name, va_arg(vlist, double));
          }
          else {
            char htype = 0;

            while (true) {
              char htype_set;
              const char c = NEXT_CHAR(fmt);
              if (c == 'f') {
                htype_set = BM_FACE;
              }
              else if (c == 'e') {
                htype_set = BM_EDGE;
              }
              else if (c == 'v') {
                htype_set = BM_VERT;
              }
              else {
                break;
              }

              if (UNLIKELY(htype & htype_set)) {
                GOTO_ERROR("htype duplicated");
              }

              htype |= htype_set;
              fmt++;
            }

            if (type == 'h') {
              BMO_slot_buffer_from_enabled_hflag(
                  bm, op, op->slots_in, slot_name, htype, va_arg(vlist, int));
            }
            else if (type == 'H') {
              BMO_slot_buffer_from_disabled_hflag(
                  bm, op, op->slots_in, slot_name, htype, va_arg(vlist, int));
            }
            else if (type == 'a') {
              if ((op->flag & BMO_FLAG_RESPECT_HIDE) == 0) {
                BMO_slot_buffer_from_all(bm, op, op->slots_in, slot_name, htype);
              }
              else {
                BMO_slot_buffer_from_disabled_hflag(
                    bm, op, op->slots_in, slot_name, htype, BM_ELEM_HIDDEN);
              }
            }
            else if (type == 'f') {
              BMO_slot_buffer_from_enabled_flag(
                  bm, op, op->slots_in, slot_name, htype, va_arg(vlist, int));
            }
            else if (type == 'F') {
              BMO_slot_buffer_from_disabled_flag(
                  bm, op, op->slots_in, slot_name, htype, va_arg(vlist, int));
            }
          }

          state = true;
          break;
        default:
          fprintf(stderr,
                  "%s: unrecognized bmop format char: '%c', %d in '%s'\n",
                  __func__,
                  *fmt,
                  int(fmt - ofmt),
                  ofmt);
          break;
      }
    }
    fmt++;
  }

  MEM_freeN(ofmt);
  return true;
error:

  /* TODO: explain exactly what is failing (not urgent). */
  fprintf(stderr, "%s: error parsing formatting string\n", __func__);

  fprintf(stderr, "string: '%s', position %d\n", _fmt, int(fmt - ofmt));
  fprintf(stderr, "         ");
  {
    int pos = int(fmt - ofmt);
    for (i = 0; i < pos; i++) {
      fprintf(stderr, " ");
    }
    fprintf(stderr, "^\n");
  }

  fprintf(stderr, "source code:  %s:%d\n", __FILE__, lineno);

  fprintf(stderr, "reason: %s\n", err_reason);

  MEM_freeN(ofmt);

  BMO_op_finish(bm, op);
  return false;

#undef GOTO_ERROR
}

bool BMO_op_initf(BMesh *bm, BMOperator *op, const int flag, const char *fmt, ...)
{
  va_list list;

  va_start(list, fmt);
  if (!BMO_op_vinitf(bm, op, flag, fmt, list)) {
    printf("%s: failed\n", __func__);
    va_end(list);
    return false;
  }
  va_end(list);

  return true;
}

bool BMO_op_callf(BMesh *bm, const int flag, const char *fmt, ...)
{
  va_list list;
  BMOperator op;

  va_start(list, fmt);
  if (!BMO_op_vinitf(bm, &op, flag, fmt, list)) {
    printf("%s: failed, format is:\n    \"%s\"\n", __func__, fmt);
    va_end(list);
    return false;
  }

  BMO_op_exec(bm, &op);
  BMO_op_finish(bm, &op);

  va_end(list);
  return true;
}
