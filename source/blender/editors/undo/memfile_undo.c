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
 * \ingroup edundo
 *
 * Wrapper between 'ED_undo.h' and 'BKE_undo_system.h' API's.
 */

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_node_types.h"
#include "DNA_object_enums.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_blender_undo.h"
#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_undo_system.h"

#include "../depsgraph/DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_undo.h"
#include "ED_util.h"

#include "../blenloader/BLO_undofile.h"

#include "undo_intern.h"

#include <stdio.h>

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct MemFileUndoStep {
  UndoStep step;
  MemFileUndoData *data;
} MemFileUndoStep;

static bool memfile_undosys_poll(bContext *C)
{
  /* other poll functions must run first, this is a catch-all. */

  if ((U.uiflag & USER_GLOBALUNDO) == 0) {
    return false;
  }

  /* Allow a single memfile undo step (the first). */
  UndoStack *ustack = ED_undo_stack_get();
  if ((ustack->step_active != NULL) && (ED_undo_is_memfile_compatible(C) == false)) {
    return false;
  }
  return true;
}

static bool memfile_undosys_step_encode(struct bContext *UNUSED(C),
                                        struct Main *bmain,
                                        UndoStep *us_p)
{
  MemFileUndoStep *us = (MemFileUndoStep *)us_p;

  /* Important we only use 'main' from the context (see: BKE_undosys_stack_init_from_main). */
  UndoStack *ustack = ED_undo_stack_get();

  if (bmain->is_memfile_undo_flush_needed) {
    ED_editors_flush_edits_ex(bmain, false, true);
  }

  /* can be NULL, use when set. */
  MemFileUndoStep *us_prev = (MemFileUndoStep *)BKE_undosys_step_find_by_type(
      ustack, BKE_UNDOSYS_TYPE_MEMFILE);
  us->data = BKE_memfile_undo_encode(bmain, us_prev ? us_prev->data : NULL);
  us->step.data_size = us->data->undo_size;

  /* Store the fact that we should not re-use old data with that undo step, and reset the Main
   * flag. */
  us->step.use_old_bmain_data = !bmain->use_memfile_full_barrier;
  bmain->use_memfile_full_barrier = false;

  return true;
}

static int memfile_undosys_step_id_reused_cb(LibraryIDLinkCallbackData *cb_data)
{
  ID *id_self = cb_data->id_self;
  ID **id_pointer = cb_data->id_pointer;
  BLI_assert((id_self->tag & LIB_TAG_UNDO_OLD_ID_REUSED) != 0);

  ID *id = *id_pointer;
  if (id != NULL && id->lib == NULL && (id->tag & LIB_TAG_UNDO_OLD_ID_REUSED) == 0) {
    bool do_stop_iter = true;
    if (GS(id_self->name) == ID_OB) {
      Object *ob_self = (Object *)id_self;
      if (ob_self->type == OB_ARMATURE) {
        if (ob_self->data == id) {
          BLI_assert(GS(id->name) == ID_AR);
          if (ob_self->pose != NULL) {
            /* We have a changed/re-read armature used by an unchanged armature object: our beloved
             * Bone pointers from the object's pose need their usual special treatment. */
            ob_self->pose->flag |= POSE_RECALC;
          }
        }
        else {
          /* Cannot stop iteration until we checked ob_self->data pointer... */
          do_stop_iter = false;
        }
      }
    }

    return do_stop_iter ? IDWALK_RET_STOP_ITER : IDWALK_RET_NOP;
  }

  return IDWALK_RET_NOP;
}

static void memfile_undosys_step_decode(struct bContext *C,
                                        struct Main *bmain,
                                        UndoStep *us_p,
                                        int undo_direction,
                                        bool UNUSED(is_final))
{
  BLI_assert(undo_direction != 0);

  bool use_old_bmain_data = true;

  if (USER_EXPERIMENTAL_TEST(&U, use_undo_legacy)) {
    use_old_bmain_data = false;
  }
  else if (undo_direction > 0) {
    /* Redo case.
     * The only time we should have to force a complete redo is when current step is tagged as a
     * redo barrier.
     * If previous step was not a memfile one should not matter here, current data in old bmain
     * should still always be valid for unchanged data-blocks. */
    if (us_p->use_old_bmain_data == false) {
      use_old_bmain_data = false;
    }
  }
  else {
    /* Undo case.
     * Here we do not care whether current step is an undo barrier, since we are coming from
     * 'the future' we can still re-use old data. However, if *next* undo step
     * (i.e. the one immediately in the future, the one we are coming from)
     * is a barrier, then we have to force a complete undo.
     * Note that non-memfile undo steps **should** not be an issue anymore, since we handle
     * fine-grained update flags now.
     */
    UndoStep *us_next = us_p->next;
    if (us_next != NULL) {
      if (us_next->use_old_bmain_data == false) {
        use_old_bmain_data = false;
      }
    }
  }

  /* Extract depsgraphs from current bmain (which may be freed during undo step reading),
   * and store them for re-use. */
  GHash *depsgraphs = NULL;
  if (use_old_bmain_data) {
    depsgraphs = BKE_scene_undo_depsgraphs_extract(bmain);
  }

  ED_editors_exit(bmain, false);

  MemFileUndoStep *us = (MemFileUndoStep *)us_p;
  BKE_memfile_undo_decode(us->data, undo_direction, use_old_bmain_data, C);

  for (UndoStep *us_iter = us_p->next; us_iter; us_iter = us_iter->next) {
    if (BKE_UNDOSYS_TYPE_IS_MEMFILE_SKIP(us_iter->type)) {
      continue;
    }
    us_iter->is_applied = false;
  }
  for (UndoStep *us_iter = us_p; us_iter; us_iter = us_iter->prev) {
    if (BKE_UNDOSYS_TYPE_IS_MEMFILE_SKIP(us_iter->type)) {
      continue;
    }
    us_iter->is_applied = true;
  }

  /* bmain has been freed. */
  bmain = CTX_data_main(C);
  ED_editors_init_for_undo(bmain);

  if (use_old_bmain_data) {
    /* Restore previous depsgraphs into current bmain. */
    BKE_scene_undo_depsgraphs_restore(bmain, depsgraphs);

    /* We need to inform depsgraph about re-used old IDs that would be using newly read
     * data-blocks, at least COW evaluated copies need to be updated... */
    ID *id = NULL;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      if (id->tag & LIB_TAG_UNDO_OLD_ID_REUSED) {
        BKE_library_foreach_ID_link(
            bmain, id, memfile_undosys_step_id_reused_cb, NULL, IDWALK_READONLY);
      }

      /* Tag depsgraph to update data-block for changes that happened between the
       * current and the target state, see direct_link_id_restore_recalc(). */
      if (id->recalc) {
        DEG_id_tag_update_ex(bmain, id, id->recalc);
      }
    }
    FOREACH_MAIN_ID_END;

    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      /* Clear temporary tag. */
      id->tag &= ~LIB_TAG_UNDO_OLD_ID_REUSED;

      /* We only start accumulating from this point, any tags set up to here
       * are already part of the current undo state. This is done in a second
       * loop because DEG_id_tag_update may set tags on other datablocks. */
      id->recalc_after_undo_push = 0;
      bNodeTree *nodetree = ntreeFromID(id);
      if (nodetree != NULL) {
        nodetree->id.recalc_after_undo_push = 0;
      }
      if (GS(id->name) == ID_SCE) {
        Scene *scene = (Scene *)id;
        if (scene->master_collection != NULL) {
          scene->master_collection->id.recalc_after_undo_push = 0;
        }
      }
    }
    FOREACH_MAIN_ID_END;
  }

  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, CTX_data_scene(C));
}

static void memfile_undosys_step_free(UndoStep *us_p)
{
  /* To avoid unnecessary slow down, free backwards
   * (so we don't need to merge when clearing all). */
  MemFileUndoStep *us = (MemFileUndoStep *)us_p;
  if (us_p->next != NULL) {
    UndoStep *us_next_p = BKE_undosys_step_same_type_next(us_p);
    if (us_next_p != NULL) {
      MemFileUndoStep *us_next = (MemFileUndoStep *)us_next_p;
      BLO_memfile_merge(&us->data->memfile, &us_next->data->memfile);
    }
  }

  BKE_memfile_undo_free(us->data);
}

/* Export for ED_undo_sys. */
void ED_memfile_undosys_type(UndoType *ut)
{
  ut->name = "Global Undo";
  ut->poll = memfile_undosys_poll;
  ut->step_encode = memfile_undosys_step_encode;
  ut->step_decode = memfile_undosys_step_decode;
  ut->step_free = memfile_undosys_step_free;

  ut->use_context = true;

  ut->step_size = sizeof(MemFileUndoStep);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

/**
 * Ideally we wouldn't need to export global undo internals,
 * there are some cases where it's needed though.
 */
static struct MemFile *ed_undosys_step_get_memfile(UndoStep *us_p)
{
  MemFileUndoStep *us = (MemFileUndoStep *)us_p;
  return &us->data->memfile;
}

struct MemFile *ED_undosys_stack_memfile_get_active(UndoStack *ustack)
{
  UndoStep *us = BKE_undosys_stack_active_with_type(ustack, BKE_UNDOSYS_TYPE_MEMFILE);
  if (us) {
    return ed_undosys_step_get_memfile(us);
  }
  return NULL;
}

/**
 * If the last undo step is a memfile one, find the first #MemFileChunk matching given ID
 * (using its session UUID), and tag it as "changed in the future".
 *
 * Since non-memfile undos cannot automatically set this flag in the previous step as done with
 * memfile ones, this has to be called manually by relevant undo code.
 *
 * \note Only current known case for this is undoing a switch from Object to Sculpt mode (see
 * T82388).
 *
 * \note Calling this ID by ID is not optimal, as it will loop over all #MemFile.chunks until it
 * finds the expected one. If this becomes an issue we'll have to add a mapping from session UUID
 * to first #MemFileChunk in #MemFile itself
 * (currently we only do that in #MemFileWriteData when writing a new step).
 */
void ED_undosys_stack_memfile_id_changed_tag(UndoStack *ustack, ID *id)
{
  UndoStep *us = ustack->step_active;
  if (id == NULL || us == NULL || us->type != BKE_UNDOSYS_TYPE_MEMFILE) {
    return;
  }

  MemFile *memfile = &((MemFileUndoStep *)us)->data->memfile;
  LISTBASE_FOREACH (MemFileChunk *, mem_chunk, &memfile->chunks) {
    if (mem_chunk->id_session_uuid == id->session_uuid) {
      mem_chunk->is_identical_future = false;
      break;
    }
  }
}

/** \} */
