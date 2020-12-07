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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BLI_listbase.h"

#include "BKE_main.h"
#include "BKE_movieclip.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "SEQ_sequencer.h"

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
int seqbase_clipboard_frame;

void BKE_sequencer_base_clipboard_pointers_free(struct ListBase *seqbase);

void BKE_sequencer_free_clipboard(void)
{
  Sequence *seq, *nseq;

  BKE_sequencer_base_clipboard_pointers_free(&seqbase_clipboard);

  for (seq = seqbase_clipboard.first; seq; seq = nseq) {
    nseq = seq->next;
    seq_free_sequence_recurse(NULL, seq, false);
  }
  BLI_listbase_clear(&seqbase_clipboard);
}

#define ID_PT (*id_pt)
static void seqclipboard_ptr_free(Main *UNUSED(bmain), ID **id_pt)
{
  if (ID_PT) {
    BLI_assert(ID_PT->newid != NULL);
    MEM_freeN(ID_PT);
    ID_PT = NULL;
  }
}
static void seqclipboard_ptr_store(Main *UNUSED(bmain), ID **id_pt)
{
  if (ID_PT) {
    ID *id_prev = ID_PT;
    ID_PT = MEM_dupallocN(ID_PT);
    ID_PT->newid = id_prev;
  }
}
static void seqclipboard_ptr_restore(Main *bmain, ID **id_pt)
{
  if (ID_PT) {
    const ListBase *lb = which_libbase(bmain, GS(ID_PT->name));
    void *id_restore;

    BLI_assert(ID_PT->newid != NULL);
    if (BLI_findindex(lb, (ID_PT)->newid) != -1) {
      /* the pointer is still valid */
      id_restore = (ID_PT)->newid;
    }
    else {
      /* the pointer of the same name still exists  */
      id_restore = BLI_findstring(lb, (ID_PT)->name + 2, offsetof(ID, name) + 2);
    }

    if (id_restore == NULL) {
      /* check for a data with the same filename */
      switch (GS(ID_PT->name)) {
        case ID_SO: {
          id_restore = BLI_findstring(lb, ((bSound *)ID_PT)->filepath, offsetof(bSound, filepath));
          if (id_restore == NULL) {
            id_restore = BKE_sound_new_file(bmain, ((bSound *)ID_PT)->filepath);
            (ID_PT)->newid = id_restore; /* reuse next time */
          }
          break;
        }
        case ID_MC: {
          id_restore = BLI_findstring(
              lb, ((MovieClip *)ID_PT)->filepath, offsetof(MovieClip, filepath));
          if (id_restore == NULL) {
            id_restore = BKE_movieclip_file_add(bmain, ((MovieClip *)ID_PT)->filepath);
            (ID_PT)->newid = id_restore; /* reuse next time */
          }
          break;
        }
        default:
          break;
      }
    }

    /* Replace with pointer to actual data-block. */
    seqclipboard_ptr_free(bmain, id_pt);
    ID_PT = id_restore;
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
    TextVars *text_data = seq->effectdata;
    callback(bmain, (ID **)&text_data->text_font);
  }
}

/* recursive versions of functions above */
void BKE_sequencer_base_clipboard_pointers_free(ListBase *seqbase)
{
  Sequence *seq;
  for (seq = seqbase->first; seq; seq = seq->next) {
    sequence_clipboard_pointers(NULL, seq, seqclipboard_ptr_free);
    BKE_sequencer_base_clipboard_pointers_free(&seq->seqbase);
  }
}
void BKE_sequencer_base_clipboard_pointers_store(Main *bmain, ListBase *seqbase)
{
  Sequence *seq;
  for (seq = seqbase->first; seq; seq = seq->next) {
    sequence_clipboard_pointers(bmain, seq, seqclipboard_ptr_store);
    BKE_sequencer_base_clipboard_pointers_store(bmain, &seq->seqbase);
  }
}
void BKE_sequencer_base_clipboard_pointers_restore(ListBase *seqbase, Main *bmain)
{
  Sequence *seq;
  for (seq = seqbase->first; seq; seq = seq->next) {
    sequence_clipboard_pointers(bmain, seq, seqclipboard_ptr_restore);
    BKE_sequencer_base_clipboard_pointers_restore(&seq->seqbase, bmain);
  }
}
