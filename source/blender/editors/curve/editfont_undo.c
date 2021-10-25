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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/curve/editfont_undo.c
 *  \ingroup edcurve
 */

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_font.h"

#include "ED_curve.h"
#include "ED_util.h"

#define USE_ARRAY_STORE

#ifdef USE_ARRAY_STORE
// #  define DEBUG_PRINT
#  include "BLI_array_store.h"
#  include "BLI_array_store_utils.h"
#  include "BLI_listbase.h"
#  define ARRAY_CHUNK_SIZE 32
#endif

typedef struct UndoFont {
	wchar_t *textbuf;
	struct CharInfo *textbufinfo;

	int len, pos;

#ifdef USE_ARRAY_STORE
	struct {
		BArrayState *textbuf;
		BArrayState *textbufinfo;
	} store;
#endif
} UndoFont;


#ifdef USE_ARRAY_STORE

/** \name Array Store
 * \{ */

static struct {
	struct BArrayStore_AtSize bs_stride;
	int users;

	/* We could have the undo API pass in the previous state, for now store a local list */
	ListBase local_links;

} uf_arraystore = {{NULL}};

/**
 * \param create: When false, only free the arrays.
 * This is done since when reading from an undo state, they must be temporarily expanded.
 * then discarded afterwards, having this argument avoids having 2x code paths.
 */
static void uf_arraystore_compact_ex(
        UndoFont *uf, const UndoFont *uf_ref,
        bool create)
{
#define STATE_COMPACT(uf, id, len) \
	if ((uf)->id) { \
		BLI_assert(create == ((uf)->store.id == NULL)); \
		if (create) { \
			BArrayState *state_reference = uf_ref ? uf_ref->store.id : NULL; \
			const size_t stride = sizeof(*(uf)->id); \
			BArrayStore *bs = BLI_array_store_at_size_ensure(&uf_arraystore.bs_stride, stride, ARRAY_CHUNK_SIZE); \
			(uf)->store.id = BLI_array_store_state_add( \
			        bs, (uf)->id, (size_t)(len) * stride, state_reference); \
		} \
		/* keep uf->len for validation */ \
		MEM_freeN((uf)->id); \
		(uf)->id = NULL; \
	} ((void)0)

	STATE_COMPACT(uf, textbuf, uf->len + 1);
	STATE_COMPACT(uf, textbufinfo, uf->len + 1);

#undef STATE_COMPACT

	if (create) {
		uf_arraystore.users += 1;
	}
}

/**
 * Move data from allocated arrays to de-duplicated states and clear arrays.
 */
static void uf_arraystore_compact(UndoFont *um, const UndoFont *uf_ref)
{
	uf_arraystore_compact_ex(um, uf_ref, true);
}

static void uf_arraystore_compact_with_info(UndoFont *um, const UndoFont *uf_ref)
{
#ifdef DEBUG_PRINT
	size_t size_expanded_prev, size_compacted_prev;
	BLI_array_store_at_size_calc_memory_usage(&uf_arraystore.bs_stride, &size_expanded_prev, &size_compacted_prev);
#endif

	uf_arraystore_compact(um, uf_ref);

#ifdef DEBUG_PRINT
	{
		size_t size_expanded, size_compacted;
		BLI_array_store_at_size_calc_memory_usage(&uf_arraystore.bs_stride, &size_expanded, &size_compacted);

		const double percent_total = size_expanded ?
		        (((double)size_compacted / (double)size_expanded) * 100.0) : -1.0;

		size_t size_expanded_step = size_expanded - size_expanded_prev;
		size_t size_compacted_step = size_compacted - size_compacted_prev;
		const double percent_step = size_expanded_step ?
		        (((double)size_compacted_step / (double)size_expanded_step) * 100.0) : -1.0;

		printf("overall memory use: %.8f%% of expanded size\n", percent_total);
		printf("step memory use:    %.8f%% of expanded size\n", percent_step);
	}
#endif
}

/**
 * Remove data we only expanded for temporary use.
 */
static void uf_arraystore_expand_clear(UndoFont *um)
{
	uf_arraystore_compact_ex(um, NULL, false);
}

static void uf_arraystore_expand(UndoFont *uf)
{
#define STATE_EXPAND(uf, id, len) \
	if ((uf)->store.id) { \
		const size_t stride = sizeof(*(uf)->id); \
		BArrayState *state = (uf)->store.id; \
		size_t state_len; \
		(uf)->id = BLI_array_store_state_data_get_alloc(state, &state_len); \
		BLI_assert((len) == (state_len / stride)); \
		UNUSED_VARS_NDEBUG(stride); \
	} ((void)0)

	STATE_EXPAND(uf, textbuf, uf->len + 1);
	STATE_EXPAND(uf, textbufinfo, uf->len + 1);

#undef STATE_EXPAND
}

static void uf_arraystore_free(UndoFont *uf)
{
#define STATE_FREE(uf, id) \
	if ((uf)->store.id) { \
		const size_t stride = sizeof(*(uf)->id); \
		BArrayStore *bs = BLI_array_store_at_size_get(&uf_arraystore.bs_stride, stride); \
		BArrayState *state = (uf)->store.id; \
		BLI_array_store_state_remove(bs, state); \
		(uf)->store.id = NULL; \
	} ((void)0)

	STATE_FREE(uf, textbuf);
	STATE_FREE(uf, textbufinfo);

#undef STATE_FREE

	uf_arraystore.users -= 1;

	BLI_assert(uf_arraystore.users >= 0);

	if (uf_arraystore.users == 0) {
#ifdef DEBUG_PRINT
		printf("editfont undo store: freeing all data!\n");
#endif

		BLI_array_store_at_size_clear(&uf_arraystore.bs_stride);
	}

}

/** \} */

#endif  /* USE_ARRAY_STORE */

static void undoFont_to_editFont(void *uf_v, void *ecu, void *UNUSED(obdata))
{
	Curve *cu = (Curve *)ecu;
	EditFont *ef = cu->editfont;
	const UndoFont *uf = uf_v;

	size_t final_size;

#ifdef USE_ARRAY_STORE
	uf_arraystore_expand(uf_v);
#endif

	final_size = sizeof(wchar_t) * (uf->len + 1);
	memcpy(ef->textbuf, uf->textbuf, final_size);

	final_size = sizeof(CharInfo) * (uf->len + 1);
	memcpy(ef->textbufinfo, uf->textbufinfo, final_size);

	ef->pos = uf->pos;
	ef->len = uf->len;

	ef->selstart = ef->selend = 0;

#ifdef USE_ARRAY_STORE
	uf_arraystore_expand_clear(uf_v);
#endif
}

static void *editFont_to_undoFont(void *ecu, void *UNUSED(obdata))
{
	Curve *cu = (Curve *)ecu;
	EditFont *ef = cu->editfont;

	UndoFont *uf = MEM_callocN(sizeof(*uf), __func__);

	size_t final_size;

	final_size = sizeof(wchar_t) * (ef->len + 1);
	uf->textbuf = MEM_mallocN(final_size, __func__);
	memcpy(uf->textbuf, ef->textbuf, final_size);

	final_size = sizeof(CharInfo) * (ef->len + 1);
	uf->textbufinfo = MEM_mallocN(final_size, __func__);
	memcpy(uf->textbufinfo, ef->textbufinfo, final_size);

	uf->pos = ef->pos;
	uf->len = ef->len;

#ifdef USE_ARRAY_STORE
	{
		const UndoFont *uf_ref = uf_arraystore.local_links.last ?
		                         ((LinkData *)uf_arraystore.local_links.last)->data : NULL;

		/* add oursrlves */
		BLI_addtail(&uf_arraystore.local_links, BLI_genericNodeN(uf));

		uf_arraystore_compact_with_info(uf, uf_ref);
	}
#endif

	return uf;
}

static void free_undoFont(void *uf_v)
{
	UndoFont *uf = uf_v;

#ifdef USE_ARRAY_STORE
	{
		LinkData *link = BLI_findptr(&uf_arraystore.local_links, uf, offsetof(LinkData, data));
		BLI_remlink(&uf_arraystore.local_links, link);
		MEM_freeN(link);
	}
	uf_arraystore_free(uf);
#endif

	if (uf->textbuf) {
		MEM_freeN(uf->textbuf);
	}
	if (uf->textbufinfo) {
		MEM_freeN(uf->textbufinfo);
	}

	MEM_freeN(uf);
}

static void *get_undoFont(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_FONT) {
		return obedit->data;
	}
	return NULL;
}

/* and this is all the undo system needs to know */
void undo_push_font(bContext *C, const char *name)
{
	undo_editmode_push(C, name, get_undoFont, free_undoFont, undoFont_to_editFont, editFont_to_undoFont, NULL);
}
