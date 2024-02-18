/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurve
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "BLI_array_utils.h" /* For `BLI_array_is_zeroed`.  */
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_undo_system.hh"
#include "BKE_vfont.hh"

#include "DEG_depsgraph.hh"

#include "ED_curve.hh"
#include "ED_undo.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#define USE_ARRAY_STORE

#ifdef USE_ARRAY_STORE
// #  define DEBUG_PRINT
#  include "BLI_array_store.h"
#  include "BLI_array_store_utils.h"
#  include "BLI_listbase.h"
#  define ARRAY_CHUNK_SIZE 32
#endif

/** Only needed this locally. */
static CLG_LogRef LOG = {"ed.undo.font"};

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

struct UndoFont {
  char32_t *textbuf;
  CharInfo *textbufinfo;

  int len, pos, selstart, selend;

#ifdef USE_ARRAY_STORE
  struct {
    BArrayState *textbuf;
    BArrayState *textbufinfo;
  } store;
#endif

  size_t undo_size;
};

#ifdef USE_ARRAY_STORE

/* -------------------------------------------------------------------- */
/** \name Array Store
 * \{ */

static struct {
  BArrayStore_AtSize bs_stride;
  int users;

  /* We could have the undo API pass in the previous state, for now store a local list */
  ListBase local_links;

} uf_arraystore = {{nullptr}};

/**
 * \param create: When false, only free the arrays.
 * This is done since when reading from an undo state, they must be temporarily expanded.
 * then discarded afterwards, having this argument avoids having 2x code paths.
 */
static void uf_arraystore_compact_ex(UndoFont *uf, const UndoFont *uf_ref, bool create)
{
#  define STATE_COMPACT(uf, id, len) \
    if ((uf)->id) { \
      BLI_assert(create == ((uf)->store.id == nullptr)); \
      if (create) { \
        BArrayState *state_reference = uf_ref ? uf_ref->store.id : nullptr; \
        const size_t stride = sizeof(*(uf)->id); \
        BArrayStore *bs = BLI_array_store_at_size_ensure( \
            &uf_arraystore.bs_stride, stride, ARRAY_CHUNK_SIZE); \
        (uf)->store.id = BLI_array_store_state_add( \
            bs, (uf)->id, size_t(len) * stride, state_reference); \
      } \
      /* keep uf->len for validation */ \
      MEM_freeN((uf)->id); \
      (uf)->id = nullptr; \
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

/**
 * Remove data we only expanded for temporary use.
 */
static void uf_arraystore_expand_clear(UndoFont *um)
{
  uf_arraystore_compact_ex(um, nullptr, false);
}

static void uf_arraystore_expand(UndoFont *uf)
{
#  define STATE_EXPAND(uf, id, len) \
    if ((uf)->store.id) { \
      const size_t stride = sizeof(*(uf)->id); \
      BArrayState *state = (uf)->store.id; \
      size_t state_len; \
      *(void **)&(uf)->id = BLI_array_store_state_data_get_alloc(state, &state_len); \
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
      (uf)->store.id = nullptr; \
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

  final_size = sizeof(*ef->textbuf) * (uf->len + 1);
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

  BLI_assert(sizeof(*uf->textbuf) == sizeof(*ef->textbuf));
  final_size = sizeof(*uf->textbuf) * (ef->len + 1);
  uf->textbuf = static_cast<char32_t *>(MEM_mallocN(final_size, __func__));
  memcpy(uf->textbuf, ef->textbuf, final_size);

  final_size = sizeof(CharInfo) * (ef->len + 1);
  uf->textbufinfo = static_cast<CharInfo *>(MEM_mallocN(final_size, __func__));
  memcpy(uf->textbufinfo, ef->textbufinfo, final_size);

  uf->pos = ef->pos;
  uf->selstart = ef->selstart;
  uf->selend = ef->selend;
  uf->len = ef->len;

#ifdef USE_ARRAY_STORE
  {
    const UndoFont *uf_ref = static_cast<const UndoFont *>(
        uf_arraystore.local_links.last ? ((LinkData *)uf_arraystore.local_links.last)->data :
                                         nullptr);

    /* Add ourselves. */
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
    LinkData *link = static_cast<LinkData *>(
        BLI_findptr(&uf_arraystore.local_links, uf, offsetof(LinkData, data)));
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
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  if (obedit && obedit->type == OB_FONT) {
    Curve *cu = static_cast<Curve *>(obedit->data);
    EditFont *ef = cu->editfont;
    if (ef != nullptr) {
      return obedit;
    }
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

struct FontUndoStep {
  UndoStep step;
  /** See #ED_undo_object_editmode_validate_scene_from_windows code comment for details. */
  UndoRefID_Scene scene_ref;
  /* NOTE: will split out into list for multi-object-editmode. */
  UndoRefID_Object obedit_ref;
  UndoFont data;
};

static bool font_undosys_poll(bContext *C)
{
  return editfont_object_from_context(C) != nullptr;
}

static bool font_undosys_step_encode(bContext *C, Main *bmain, UndoStep *us_p)
{
  FontUndoStep *us = (FontUndoStep *)us_p;
  us->scene_ref.ptr = CTX_data_scene(C);
  us->obedit_ref.ptr = editfont_object_from_context(C);
  Curve *cu = static_cast<Curve *>(us->obedit_ref.ptr->data);
  undofont_from_editfont(&us->data, cu);
  us->step.data_size = us->data.undo_size;
  cu->editfont->needs_flush_to_id = 1;
  bmain->is_memfile_undo_flush_needed = true;

  return true;
}

static void font_undosys_step_decode(
    bContext *C, Main *bmain, UndoStep *us_p, const eUndoStepDir /*dir*/, bool /*is_final*/)
{

  FontUndoStep *us = (FontUndoStep *)us_p;
  Object *obedit = us->obedit_ref.ptr;
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* Pass in an array of 1 (typically used for multi-object edit-mode). */
  ED_undo_object_editmode_validate_scene_from_windows(
      CTX_wm_manager(C), us->scene_ref.ptr, &scene, &view_layer);
  ED_undo_object_editmode_restore_helper(scene, view_layer, &obedit, 1, sizeof(Object *));

  Curve *cu = static_cast<Curve *>(obedit->data);
  undofont_to_editfont(&us->data, cu);
  DEG_id_tag_update(&cu->id, ID_RECALC_GEOMETRY);

  ED_undo_object_set_active_or_warn(scene, view_layer, obedit, us_p->name, &LOG);

  /* Check after setting active (unless undoing into another scene). */
  BLI_assert(font_undosys_poll(C) || (scene != CTX_data_scene(C)));

  cu->editfont->needs_flush_to_id = 1;
  bmain->is_memfile_undo_flush_needed = true;
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, nullptr);
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
  foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->scene_ref));
  foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->obedit_ref));
}

void ED_font_undosys_type(UndoType *ut)
{
  ut->name = "Edit Font";
  ut->poll = font_undosys_poll;
  ut->step_encode = font_undosys_step_encode;
  ut->step_decode = font_undosys_step_decode;
  ut->step_free = font_undosys_step_free;

  ut->step_foreach_ID_ref = font_undosys_foreach_ID_ref;

  ut->flags = UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE;

  ut->step_size = sizeof(FontUndoStep);
}

/** \} */
