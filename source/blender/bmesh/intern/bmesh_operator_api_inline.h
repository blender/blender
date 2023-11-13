/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BMesh inline operator functions.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Tool Flag API: Tool code must never put junk in header flags (#BMHeader.hflag)
 * instead, use this API to set flags.
 * If you need to store a value per element, use a #GHash or a mapping slot to do it. */

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2) BLI_INLINE
    short _bmo_elem_flag_test(BMesh *bm, const BMFlagLayer *oflags, const short oflag)
{
  BLI_assert(bm->use_toolflags);
  return oflags[bm->toolflag_index].f & oflag;
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2) BLI_INLINE
    bool _bmo_elem_flag_test_bool(BMesh *bm, const BMFlagLayer *oflags, const short oflag)
{
  BLI_assert(bm->use_toolflags);
  return (oflags[bm->toolflag_index].f & oflag) != 0;
}

ATTR_NONNULL(1, 2)
BLI_INLINE void _bmo_elem_flag_enable(BMesh *bm, BMFlagLayer *oflags, const short oflag)
{
  BLI_assert(bm->use_toolflags);
  oflags[bm->toolflag_index].f |= oflag;
}

ATTR_NONNULL(1, 2)
BLI_INLINE void _bmo_elem_flag_disable(BMesh *bm, BMFlagLayer *oflags, const short oflag)
{
  BLI_assert(bm->use_toolflags);
  oflags[bm->toolflag_index].f &= (short)~oflag;
}

ATTR_NONNULL(1, 2)
BLI_INLINE void _bmo_elem_flag_set(BMesh *bm, BMFlagLayer *oflags, const short oflag, int val)
{
  BLI_assert(bm->use_toolflags);
  if (val) {
    oflags[bm->toolflag_index].f |= oflag;
  }
  else {
    oflags[bm->toolflag_index].f &= (short)~oflag;
  }
}

ATTR_NONNULL(1, 2)
BLI_INLINE void _bmo_elem_flag_toggle(BMesh *bm, BMFlagLayer *oflags, const short oflag)
{
  BLI_assert(bm->use_toolflags);
  oflags[bm->toolflag_index].f ^= oflag;
}

ATTR_NONNULL(1, 2)
BLI_INLINE void BMO_slot_map_int_insert(BMOperator *op,
                                        BMOpSlot *slot,
                                        void *element,
                                        const int val)
{
  union {
    void *ptr;
    int val;
  } t = {NULL};
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_INT);
  BMO_slot_map_insert(op, slot, element, ((void)(t.val = val), t.ptr));
}

ATTR_NONNULL(1, 2)
BLI_INLINE void BMO_slot_map_bool_insert(BMOperator *op,
                                         BMOpSlot *slot,
                                         void *element,
                                         const bool val)
{
  union {
    void *ptr;
    bool val;
  } t = {NULL};
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_BOOL);
  BMO_slot_map_insert(op, slot, element, ((void)(t.val = val), t.ptr));
}

ATTR_NONNULL(1, 2)
BLI_INLINE void BMO_slot_map_float_insert(BMOperator *op,
                                          BMOpSlot *slot,
                                          void *element,
                                          const float val)
{
  union {
    void *ptr;
    float val;
  } t = {NULL};
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_FLT);
  BMO_slot_map_insert(op, slot, element, ((void)(t.val = val), t.ptr));
}

/* pointer versions of BMO_slot_map_float_get and BMO_slot_map_float_insert.
 *
 * do NOT use these for non-operator-api-allocated memory! instead
 * use BMO_slot_map_data_get and BMO_slot_map_insert, which copies the data. */

ATTR_NONNULL(1, 2)
BLI_INLINE void BMO_slot_map_ptr_insert(BMOperator *op,
                                        BMOpSlot *slot,
                                        const void *element,
                                        void *val)
{
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL);
  BMO_slot_map_insert(op, slot, element, val);
}

ATTR_NONNULL(1, 2)
BLI_INLINE void BMO_slot_map_elem_insert(BMOperator *op,
                                         BMOpSlot *slot,
                                         const void *element,
                                         void *val)
{
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_ELEM);
  BMO_slot_map_insert(op, slot, element, val);
}

/* no values */
ATTR_NONNULL(1, 2)
BLI_INLINE void BMO_slot_map_empty_insert(BMOperator *op, BMOpSlot *slot, const void *element)
{
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_EMPTY);
  BMO_slot_map_insert(op, slot, element, NULL);
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) BLI_INLINE
    bool BMO_slot_map_contains(BMOpSlot *slot, const void *element)
{
  BLI_assert(slot->slot_type == BMO_OP_SLOT_MAPPING);
  return BLI_ghash_haskey(slot->data.ghash, element);
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) BLI_INLINE
    void **BMO_slot_map_data_get(BMOpSlot *slot, const void *element)
{

  return BLI_ghash_lookup_p(slot->data.ghash, element);
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) BLI_INLINE
    float BMO_slot_map_float_get(BMOpSlot *slot, const void *element)
{
  void **data;
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_FLT);

  data = BMO_slot_map_data_get(slot, element);
  if (data) {
    return *(float *)data;
  }
  else {
    return 0.0f;
  }
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) BLI_INLINE
    int BMO_slot_map_int_get(BMOpSlot *slot, const void *element)
{
  void **data;
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_INT);

  data = BMO_slot_map_data_get(slot, element);
  if (data) {
    return *(int *)data;
  }
  else {
    return 0;
  }
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) BLI_INLINE
    bool BMO_slot_map_bool_get(BMOpSlot *slot, const void *element)
{
  void **data;
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_BOOL);

  data = BMO_slot_map_data_get(slot, element);
  if (data) {
    return *(bool *)data;
  }
  else {
    return false;
  }
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) BLI_INLINE
    void *BMO_slot_map_ptr_get(BMOpSlot *slot, const void *element)
{
  void **val = BMO_slot_map_data_get(slot, element);
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL);
  if (val) {
    return *val;
  }

  return NULL;
}

ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) BLI_INLINE
    void *BMO_slot_map_elem_get(BMOpSlot *slot, const void *element)
{
  void **val = (void **)BMO_slot_map_data_get(slot, element);
  BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_ELEM);
  if (val) {
    return *val;
  }

  return NULL;
}

#ifdef __cplusplus
}
#endif
