/*
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
 */

/** \file
 * \ingroup edcurve
 */

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_array_utils.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_font.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_object.h"
#include "ED_curve.h"
#include "ED_util.h"

#include "WM_types.h"
#include "WM_api.h"

#define USE_ARRAY_STORE

#ifdef USE_ARRAY_STORE
// #  define DEBUG_PRINT
#  include "BLI_array_store.h"
#  include "BLI_array_store_utils.h"
#  include "BLI_listbase.h"
#  define ARRAY_CHUNK_SIZE 32
#endif

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

typedef struct UndoFont {
  wchar_t *textbuf;
  struct CharInfo *textbufinfo;

  int len, pos, selstart, selend;

#ifdef USE_ARRAY_STORE
  struct {
    BArrayState *textbuf;
    BArrayState *textbufinfo;
  } store;
#endif

  size_t undo_size;
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
static void uf_arraystore_compact_ex(UndoFont *uf, const UndoFont *uf_ref, bool create)
{
#  define STATE_COMPACT(uf, id, len) \
    if ((uf)->id) { \
      BLI_assert(create == ((uf)->store.id == NULL)); \
      if (create) { \
        BArrayState *state_reference = uf_ref ? uf_ref->store.id : NULL; \
        const size_t stride = sizeof(*(uf)->id); \
        BArrayStore *bs = BLI_array_store_at_size_ensure( \
            &uf_arraystore.bs_stride, stride, ARRAY_CHUNK_SIZE); \
        (uf)->store.id = BLI_array_store_state_add( \
            bs, (uf)->id, (size_t)(len)*stride, state_reference); \
      } \
      /* keep uf->len for validation */ \
      MEM_freeN((uf)->id); \
      (uf)->id = NULL; \
    } \
    ((void)0)

  STATE_COMPACT(uf, textbuf, uf->len + 1);
  STATE_COMPACT(uf, textbufinfo, uf->len + 1);

#  undef STATE_COMPACT

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
#  ifdef DEBUG_PRINT
  size_t size_expanded_prev, size_compacted_prev;
  BLI_array_store_at_size_calc_memory_usage(
      &uf_arraystore.bs_stride, &size_expanded_prev, &size_compacted_prev);
#  endif

  uf_arraystore_compact(um, uf_ref);

#  ifdef DEBUG_PRINT
  {
    size_t size_expanded, size_compacted;
    BLI_array_store_at_size_calc_memory_usage(
        &uf_arraystore.bs_stride, &size_expanded, &size_compacted);

    const double percent_total = size_expanded ?
                                     (((double)size_compacted / (double)size_expanded) * 100.0) :
                                     -1.0;

    size_t size_expanded_step = size_expanded - size_expanded_prev;
    size_t size_compacted_step = size_compacted - size_compacted_prev;
    const double percent_step = size_expanded_step ?
                                    (((double)size_compacted_step / (double)size_expanded_step) *
                                     100.0) :
                                    -1.0;

    printf("overall memory use: %.8f%% of expanded size\n", percent_total);
    printf("step memory use:    %.8f%% of expanded size\n", percent_step);
  }
#  endif
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
#  define STATE_EXPAND(uf, id, len) \
    if ((uf)->store.id) { \
      const size_t stride = sizeof(*(uf)->id); \
      BArrayState *state = (uf)->store.id; \
      size_t state_len; \
      (uf)->id = BLI_array_store_state_data_get_alloc(state, &state_len); \
      BLI_assert((len) == (state_len / stride)); \
      UNUSED_VARS_NDEBUG(stride); \
    } \
    ((void)0)

  STATE_EXPAND(uf, textbuf, uf->len + 1);
  STATE_EXPAND(uf, textbufinfo, uf->len + 1);

#  undef STATE_EXPAND
}

static void uf_arraystore_free(UndoFont *uf)
{
#  define STATE_FREE(uf, id) \
    if ((uf)->store.id) { \
      const size_t stride = sizeof(*(uf)->id); \
      BArrayStore *bs = BLI_array_store_at_size_get(&uf_arraystore.bs_stride, stride); \
      BArrayState *state = (uf)->store.id; \
      BLI_array_store_state_remove(bs, state); \
      (uf)->store.id = NULL; \
    } \
    ((void)0)

  STATE_FREE(uf, textbuf);
  STATE_FREE(uf, textbufinfo);

#  undef STATE_FREE

  uf_arraystore.users -= 1;

  BLI_assert(uf_arraystore.users >= 0);

  if (uf_arraystore.users == 0) {
#  ifdef DEBUG_PRINT
    printf("editfont undo store: freeing all data!\n");
#  endif

    BLI_array_store_at_size_clear(&uf_arraystore.bs_stride);
  }
}

/** \} */

#endif /* USE_ARRAY_STORE */

static void undofont_to_editfont(UndoFont *uf, Curve *cu)
{
  EditFont *ef = cu->editfont;

  size_t final_size;

#ifdef USE_ARRAY_STORE
  uf_arraystore_expand(uf);
#endif

  final_size = sizeof(wchar_t) * (uf->len + 1);
  memcpy(ef->textbuf, uf->textbuf, final_size);

  final_size = sizeof(CharInfo) * (uf->len + 1);
  memcpy(ef->textbufinfo, uf->textbufinfo, final_size);

  ef->pos = uf->pos;
  ef->selstart = uf->selstart;
  ef->selend = uf->selend;
  ef->len = uf->len;

#ifdef USE_ARRAY_STORE
  uf_arraystore_expand_clear(uf);
#endif
}

static void *undofont_from_editfont(UndoFont *uf, Curve *cu)
{
  BLI_assert(BLI_array_is_zeroed(uf, 1));

  EditFont *ef = cu->editfont;

  size_t mem_used_prev = MEM_get_memory_in_use();

  size_t final_size;

  final_size = sizeof(wchar_t) * (ef->len + 1);
  uf->textbuf = MEM_mallocN(final_size, __func__);
  memcpy(uf->textbuf, ef->textbuf, final_size);

  final_size = sizeof(CharInfo) * (ef->len + 1);
  uf->textbufinfo = MEM_mallocN(final_size, __func__);
  memcpy(uf->textbufinfo, ef->textbufinfo, final_size);

  uf->pos = ef->pos;
  uf->selstart = ef->selstart;
  uf->selend = ef->selend;
  uf->len = ef->len;

#ifdef USE_ARRAY_STORE
  {
    const UndoFont *uf_ref = uf_arraystore.local_links.last ?
                                 ((LinkData *)uf_arraystore.local_links.last)->data :
                                 NULL;

    /* add oursrlves */
    BLI_addtail(&uf_arraystore.local_links, BLI_genericNodeN(uf));

    uf_arraystore_compact_with_info(uf, uf_ref);
  }
#endif

  size_t mem_used_curr = MEM_get_memory_in_use();

  uf->undo_size = mem_used_prev < mem_used_curr ? mem_used_curr - mem_used_prev : sizeof(UndoFont);

  return uf;
}

static void undofont_free_data(UndoFont *uf)
{
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
}

static Object *editfont_object_from_context(bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  if (obedit && obedit->type == OB_FONT) {
    Curve *cu = obedit->data;
    EditFont *ef = cu->editfont;
    if (ef != NULL) {
      return obedit;
    }
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct FontUndoStep {
  UndoStep step;
  /* note: will split out into list for multi-object-editmode. */
  UndoRefID_Object obedit_ref;
  UndoFont data;
} FontUndoStep;

static bool font_undosys_poll(bContext *C)
{
  return editfont_object_from_context(C) != NULL;
}

static bool font_undosys_step_encode(struct bContext *C,
                                     struct Main *UNUSED(bmain),
                                     UndoStep *us_p)
{
  FontUndoStep *us = (FontUndoStep *)us_p;
  us->obedit_ref.ptr = editfont_object_from_context(C);
  Curve *cu = us->obedit_ref.ptr->data;
  undofont_from_editfont(&us->data, cu);
  us->step.data_size = us->data.undo_size;
  return true;
}

static void font_undosys_step_decode(struct bContext *C,
                                     struct Main *UNUSED(bmain),
                                     UndoStep *us_p,
                                     int UNUSED(dir),
                                     bool UNUSED(is_final))
{
  /* TODO(campbell): undo_system: use low-level API to set mode. */
  ED_object_mode_set(C, OB_MODE_EDIT);
  BLI_assert(font_undosys_poll(C));

  FontUndoStep *us = (FontUndoStep *)us_p;
  Object *obedit = us->obedit_ref.ptr;
  Curve *cu = obedit->data;
  undofont_to_editfont(&us->data, cu);
  DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
}

static void font_undosys_step_free(UndoStep *us_p)
{
  FontUndoStep *us = (FontUndoStep *)us_p;
  undofont_free_data(&us->data);
}

static void font_undosys_foreach_ID_ref(UndoStep *us_p,
                                        UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                        void *user_data)
{
  FontUndoStep *us = (FontUndoStep *)us_p;
  foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->obedit_ref));
}

/* Export for ED_undo_sys. */
void ED_font_undosys_type(UndoType *ut)
{
  ut->name = "Edit Font";
  ut->poll = font_undosys_poll;
  ut->step_encode = font_undosys_step_encode;
  ut->step_decode = font_undosys_step_decode;
  ut->step_free = font_undosys_step_free;

  ut->step_foreach_ID_ref = font_undosys_foreach_ID_ref;

  ut->use_context = true;

  ut->step_size = sizeof(FontUndoStep);
}

/** \} */
