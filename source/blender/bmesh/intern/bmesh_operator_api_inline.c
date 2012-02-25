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

/** \file blender/bmesh/intern/bmesh_operator_api_inline.c
 *  \ingroup bmesh
 *
 * BMesh inline operator functions.
 */

#ifndef __BMESH_OPERATOR_API_INLINE_C__
#define __BMESH_OPERATOR_API_INLINE_C__

#include "bmesh.h"

/* inserts a key/value mapping into a mapping slot.  note that it copies the
 * value, it doesn't store a reference to it. */

BM_INLINE void BMO_slot_map_insert(BMesh *UNUSED(bm), BMOperator *op, const char *slotname,
                                   void *element, void *data, int len)
{
	BMOElemMapping *mapping;
	BMOpSlot *slot = BMO_slot_get(op, slotname);

	/*sanity check*/
	if (slot->slottype != BMO_OP_SLOT_MAPPING) {
		return;
	}

	mapping = (BMOElemMapping *) BLI_memarena_alloc(op->arena, sizeof(*mapping) + len);

	mapping->element = (BMHeader*) element;
	mapping->len = len;
	memcpy(mapping + 1, data, len);

	if (!slot->data.ghash) {
		slot->data.ghash = BLI_ghash_new(BLI_ghashutil_ptrhash,
		                                 BLI_ghashutil_ptrcmp, "bmesh op");
	}

	BLI_ghash_insert(slot->data.ghash, element, mapping);
}

BM_INLINE void BMO_slot_map_int_insert(BMesh *bm, BMOperator *op, const char *slotname,
                                       void *element, int val)
{
	BMO_slot_map_insert(bm, op, slotname, element, &val, sizeof(int));
}

BM_INLINE void BMO_slot_map_float_insert(BMesh *bm, BMOperator *op, const char *slotname,
                                         void *element, float val)
{
	BMO_slot_map_insert(bm, op, slotname, element, &val, sizeof(float));
}


/* pointer versoins of BMO_slot_map_float_get and BMO_slot_map_float_insert.
 *
 * do NOT use these for non-operator-api-allocated memory! instead
 * use BMO_slot_map_data_get and BMO_slot_map_insert, which copies the data. */

BM_INLINE void BMO_slot_map_ptr_insert(BMesh *bm, BMOperator *op, const char *slotname,
                                       void *element, void *val)
{
	BMO_slot_map_insert(bm, op, slotname, element, &val, sizeof(void*));
}

BM_INLINE int BMO_slot_map_contains(BMesh *UNUSED(bm), BMOperator *op, const char *slotname, void *element)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);

	/*sanity check*/
	if (slot->slottype != BMO_OP_SLOT_MAPPING) return 0;
	if (!slot->data.ghash) return 0;

	return BLI_ghash_haskey(slot->data.ghash, element);
}

BM_INLINE void *BMO_slot_map_data_get(BMesh *UNUSED(bm), BMOperator *op, const char *slotname,
                                      void *element)
{
	BMOElemMapping *mapping;
	BMOpSlot *slot = BMO_slot_get(op, slotname);

	/*sanity check*/
	if (slot->slottype != BMO_OP_SLOT_MAPPING) return NULL;
	if (!slot->data.ghash) return NULL;

	mapping = (BMOElemMapping *)BLI_ghash_lookup(slot->data.ghash, element);

	if (!mapping) return NULL;

	return mapping + 1;
}

BM_INLINE float BMO_slot_map_float_get(BMesh *bm, BMOperator *op, const char *slotname,
                                       void *element)
{
	float *val = (float*) BMO_slot_map_data_get(bm, op, slotname, element);
	if (val) return *val;

	return 0.0f;
}

BM_INLINE int BMO_slot_map_int_get(BMesh *bm, BMOperator *op, const char *slotname,
                                   void *element)
{
	int *val = (int*) BMO_slot_map_data_get(bm, op, slotname, element);
	if (val) return *val;

	return 0;
}

BM_INLINE void *BMO_slot_map_ptr_get(BMesh *bm, BMOperator *op, const char *slotname,
                                     void *element)
{
	void **val = (void**) BMO_slot_map_data_get(bm, op, slotname, element);
	if (val) return *val;

	return NULL;
}

#endif /* __BMESH_OPERATOR_API_INLINE_C__ */
