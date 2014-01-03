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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_operator_api_inline.h
 *  \ingroup bmesh
 *
 * BMesh inline operator functions.
 */

#ifndef __BMESH_OPERATOR_API_INLINE_H__
#define __BMESH_OPERATOR_API_INLINE_H__

/* tool flag API. never, ever ever should tool code put junk in
 * header flags (element->head.flag), nor should they use
 * element->head.eflag1/eflag2.  instead, use this api to set
 * flags.
 *
 * if you need to store a value per element, use a
 * ghash or a mapping slot to do it. */

/* flags 15 and 16 (1 << 14 and 1 << 15) are reserved for bmesh api use */
BLI_INLINE short _bmo_elem_flag_test(BMesh *bm, BMFlagLayer *oflags, const short oflag)
{
	return oflags[bm->stackdepth - 1].f & oflag;
}

BLI_INLINE bool _bmo_elem_flag_test_bool(BMesh *bm, BMFlagLayer *oflags, const short oflag)
{
	return (oflags[bm->stackdepth - 1].f & oflag) != 0;
}

BLI_INLINE void _bmo_elem_flag_enable(BMesh *bm, BMFlagLayer *oflags, const short oflag)
{
	oflags[bm->stackdepth - 1].f |= oflag;
}

BLI_INLINE void _bmo_elem_flag_disable(BMesh *bm, BMFlagLayer *oflags, const short oflag)
{
	oflags[bm->stackdepth - 1].f &= (short)~oflag;
}

BLI_INLINE void _bmo_elem_flag_set(BMesh *bm, BMFlagLayer *oflags, const short oflag, int val)
{
	if (val) oflags[bm->stackdepth - 1].f |= oflag;
	else     oflags[bm->stackdepth - 1].f &= (short)~oflag;
}

BLI_INLINE void _bmo_elem_flag_toggle(BMesh *bm, BMFlagLayer *oflags, const short oflag)
{
	oflags[bm->stackdepth - 1].f ^= oflag;
}

BLI_INLINE void BMO_slot_map_int_insert(BMOperator *op, BMOpSlot *slot,
                                        void *element, const int val)
{
	union { void *ptr; int val; } t = {NULL};
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_INT);
	BMO_slot_map_insert(op, slot, element, ((t.val = val), t.ptr));
}

BLI_INLINE void BMO_slot_map_bool_insert(BMOperator *op, BMOpSlot *slot,
                                        void *element, const bool val)
{
	union { void *ptr; bool val; } t = {NULL};
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_BOOL);
	BMO_slot_map_insert(op, slot, element, ((t.val = val), t.ptr));
}

BLI_INLINE void BMO_slot_map_float_insert(BMOperator *op, BMOpSlot *slot,
                                          void *element, const float val)
{
	union { void *ptr; float val; } t = {NULL};
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_FLT);
	BMO_slot_map_insert(op, slot, element, ((t.val = val), t.ptr));
}


/* pointer versions of BMO_slot_map_float_get and BMO_slot_map_float_insert.
 *
 * do NOT use these for non-operator-api-allocated memory! instead
 * use BMO_slot_map_data_get and BMO_slot_map_insert, which copies the data. */

BLI_INLINE void BMO_slot_map_ptr_insert(BMOperator *op, BMOpSlot *slot,
                                        const void *element, void *val)
{
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL);
	BMO_slot_map_insert(op, slot, element, val);
}

BLI_INLINE void BMO_slot_map_elem_insert(BMOperator *op, BMOpSlot *slot,
                                        const void *element, void *val)
{
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_ELEM);
	BMO_slot_map_insert(op, slot, element, val);
}


/* no values */
BLI_INLINE void BMO_slot_map_empty_insert(BMOperator *op, BMOpSlot *slot,
                                        const void *element)
{
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_EMPTY);
	BMO_slot_map_insert(op, slot, element, NULL);
}

BLI_INLINE bool BMO_slot_map_contains(BMOpSlot *slot, const void *element)
{
	BLI_assert(slot->slot_type == BMO_OP_SLOT_MAPPING);
	return BLI_ghash_haskey(slot->data.ghash, element);
}

BLI_INLINE void **BMO_slot_map_data_get(BMOpSlot *slot, const void *element)
{

	return BLI_ghash_lookup_p(slot->data.ghash, element);
}

BLI_INLINE float BMO_slot_map_float_get(BMOpSlot *slot, const void *element)
{
	void **data;
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_FLT);

	data = BMO_slot_map_data_get(slot, element);
	if (data) {
		return **(float **)data;
	}
	else {
		return 0.0f;
	}
}

BLI_INLINE int BMO_slot_map_int_get(BMOpSlot *slot, const void *element)
{
	void **data;
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_INT);

	data = BMO_slot_map_data_get(slot, element);
	if (data) {
		return **(int **)data;
	}
	else {
		return 0;
	}
}

BLI_INLINE bool BMO_slot_map_bool_get(BMOpSlot *slot, const void *element)
{
	void **data;
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_BOOL);

	data = BMO_slot_map_data_get(slot, element);
	if (data) {
		return **(bool **)data;
	}
	else {
		return false;
	}
}

BLI_INLINE void *BMO_slot_map_ptr_get(BMOpSlot *slot, const void *element)
{
	void **val = BMO_slot_map_data_get(slot, element);
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL);
	if (val) return *val;

	return NULL;
}

BLI_INLINE void *BMO_slot_map_elem_get(BMOpSlot *slot, const void *element)
{
	void **val = (void **) BMO_slot_map_data_get(slot, element);
	BLI_assert(slot->slot_subtype.map == BMO_OP_SLOT_SUBTYPE_MAP_ELEM);
	if (val) return *val;

	return NULL;
}

#endif /* __BMESH_OPERATOR_API_INLINE_H__ */
