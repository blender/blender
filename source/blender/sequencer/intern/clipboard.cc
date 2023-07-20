/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Foundation
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_fcurve.h"
#include "BKE_main.h"
#include "BKE_movieclip.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "SEQ_clipboard.h"
#include "SEQ_select.h"

#include "sequencer.h"

#ifdef WITH_AUDASPACE
#  include <AUD_Special.h>
#endif

/* -------------------------------------------------------------------- */
/* Manage pointers in the clipboard.
 * note that these pointers should _never_ be access in the sequencer,
 * they are only for storage while in the clipboard
 * notice 'newid' is used for temp pointer storage here, validate on access (this is safe usage,
 * since those data-blocks are fully out of Main lists).
 */

ListBase seqbase_clipboard;
ListBase fcurves_clipboard;
ListBase drivers_clipboard;
int seqbase_clipboard_frame;
static char seq_clipboard_active_seq_name[SEQ_NAME_MAXSTR];

void seq_clipboard_pointers_free(ListBase *seqbase);

void SEQ_clipboard_free()
{
  seq_clipboard_pointers_free(&seqbase_clipboard);

  LISTBASE_FOREACH_MUTABLE (Sequence *, seq, &seqbase_clipboard) {
    seq_free_sequence_recurse(nullptr, seq, false);
  }
  BLI_listbase_clear(&seqbase_clipboard);

  LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &fcurves_clipboard) {
    BKE_fcurve_free(fcu);
  }
  BLI_listbase_clear(&fcurves_clipboard);

  LISTBASE_FOREACH_MUTABLE (FCurve *, fcu, &drivers_clipboard) {
    BKE_fcurve_free(fcu);
  }
  BLI_listbase_clear(&drivers_clipboard);
}

#define ID_PT (*id_pt)
static void seqclipboard_ptr_free(Main * /*bmain*/, ID **id_pt)
{
  if (ID_PT) {
    BLI_assert(ID_PT->newid != nullptr);
    MEM_freeN(ID_PT);
    ID_PT = nullptr;
  }
}
static void seqclipboard_ptr_store(Main * /*bmain*/, ID **id_pt)
{
  if (ID_PT) {
    ID *id_prev = ID_PT;
    ID_PT = static_cast<ID *>(MEM_dupallocN(ID_PT));
    ID_PT->newid = id_prev;
  }
}
static void seqclipboard_ptr_restore(Main *bmain, ID **id_pt)
{
  if (ID_PT) {
    const ListBase *lb = which_libbase(bmain, GS(ID_PT->name));
    void *id_restore;

    BLI_assert(ID_PT->newid != nullptr);
    if (BLI_findindex(lb, (ID_PT)->newid) != -1) {
      /* the pointer is still valid */
      id_restore = (ID_PT)->newid;
    }
    else {
      /* The pointer of the same name still exists. */
      id_restore = BLI_findstring(lb, (ID_PT)->name + 2, offsetof(ID, name) + 2);
    }

    if (id_restore == nullptr) {
      /* check for a data with the same filename */
      switch (GS(ID_PT->name)) {
        case ID_SO: {
          id_restore = BLI_findstring(lb, ((bSound *)ID_PT)->filepath, offsetof(bSound, filepath));
          if (id_restore == nullptr) {
            id_restore = BKE_sound_new_file(bmain, ((bSound *)ID_PT)->filepath);
            (ID_PT)->newid = static_cast<ID *>(id_restore); /* reuse next time */
          }
          break;
        }
        case ID_MC: {
          id_restore = BLI_findstring(
              lb, ((MovieClip *)ID_PT)->filepath, offsetof(MovieClip, filepath));
          if (id_restore == nullptr) {
            id_restore = BKE_movieclip_file_add(bmain, ((MovieClip *)ID_PT)->filepath);
            (ID_PT)->newid = static_cast<ID *>(id_restore); /* reuse next time */
          }
          break;
        }
        default:
          break;
      }
    }

    /* Replace with pointer to actual data-block. */
    seqclipboard_ptr_free(bmain, id_pt);
    ID_PT = static_cast<ID *>(id_restore);
  }
}
#undef ID_PT

static void sequence_clipboard_pointers(Main *bmain,
                                        Sequence *seq,
                                        void (*callback)(Main *, ID **))
{
  callback(bmain, (ID **)&seq->scene);
  callback(bmain, (ID **)&seq->scene_camera);
  callback(bmain, (ID **)&seq->clip);
  callback(bmain, (ID **)&seq->mask);
  callback(bmain, (ID **)&seq->sound);

  if (seq->type == SEQ_TYPE_TEXT && seq->effectdata) {
    TextVars *text_data = static_cast<TextVars *>(seq->effectdata);
    callback(bmain, (ID **)&text_data->text_font);
  }
}

/* recursive versions of functions above */
void seq_clipboard_pointers_free(ListBase *seqbase)
{
  Sequence *seq;
  for (seq = static_cast<Sequence *>(seqbase->first); seq; seq = seq->next) {
    sequence_clipboard_pointers(nullptr, seq, seqclipboard_ptr_free);
    seq_clipboard_pointers_free(&seq->seqbase);
  }
}
void SEQ_clipboard_pointers_store(Main *bmain, ListBase *seqbase)
{
  Sequence *seq;
  for (seq = static_cast<Sequence *>(seqbase->first); seq; seq = seq->next) {
    sequence_clipboard_pointers(bmain, seq, seqclipboard_ptr_store);
    SEQ_clipboard_pointers_store(bmain, &seq->seqbase);
  }
}
void SEQ_clipboard_pointers_restore(ListBase *seqbase, Main *bmain)
{
  Sequence *seq;
  for (seq = static_cast<Sequence *>(seqbase->first); seq; seq = seq->next) {
    sequence_clipboard_pointers(bmain, seq, seqclipboard_ptr_restore);
    SEQ_clipboard_pointers_restore(&seq->seqbase, bmain);
  }
}

void SEQ_clipboard_active_seq_name_store(Scene *scene)
{
  Sequence *active_seq = SEQ_select_active_get(scene);
  if (active_seq != nullptr) {
    STRNCPY(seq_clipboard_active_seq_name, active_seq->name);
  }
  else {
    seq_clipboard_active_seq_name[0] = '\0';
  }
}

bool SEQ_clipboard_pasted_seq_was_active(Sequence *pasted_seq)
{
  return STREQ(pasted_seq->name, seq_clipboard_active_seq_name);
}
