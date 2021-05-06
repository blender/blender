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

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"

#include "BKE_scene.h"

#include "SEQ_iterator.h"

/* ************************* iterator ************************** */
/* *************** (replaces old WHILE_SEQ) ********************* */
/* **************** use now SEQ_ALL_BEGIN () SEQ_ALL_END ***************** */

/* sequence strip iterator:
 * - builds a full array, recursively into meta strips
 */

static void seq_count(ListBase *seqbase, int *tot)
{
  Sequence *seq;

  for (seq = seqbase->first; seq; seq = seq->next) {
    (*tot)++;

    if (seq->seqbase.first) {
      seq_count(&seq->seqbase, tot);
    }
  }
}

static void seq_build_array(ListBase *seqbase, Sequence ***array, int depth)
{
  Sequence *seq;

  for (seq = seqbase->first; seq; seq = seq->next) {
    seq->depth = depth;

    if (seq->seqbase.first) {
      seq_build_array(&seq->seqbase, array, depth + 1);
    }

    **array = seq;
    (*array)++;
  }
}

static void seq_array(Editing *ed,
                      const bool use_current_sequences,
                      Sequence ***r_seqarray,
                      int *r_seqarray_len)
{
  Sequence **array;

  *r_seqarray = NULL;
  *r_seqarray_len = 0;

  if (ed == NULL) {
    return;
  }

  if (use_current_sequences) {
    seq_count(ed->seqbasep, r_seqarray_len);
  }
  else {
    seq_count(&ed->seqbase, r_seqarray_len);
  }

  if (*r_seqarray_len == 0) {
    return;
  }

  *r_seqarray = array = MEM_mallocN(sizeof(Sequence *) * (*r_seqarray_len), "SeqArray");
  if (use_current_sequences) {
    seq_build_array(ed->seqbasep, &array, 0);
  }
  else {
    seq_build_array(&ed->seqbase, &array, 0);
  }
}

void SEQ_iterator_begin(Editing *ed, SeqIterator *iter, const bool use_current_sequences)
{
  memset(iter, 0, sizeof(*iter));
  seq_array(ed, use_current_sequences, &iter->array, &iter->tot);

  if (iter->tot) {
    iter->cur = 0;
    iter->seq = iter->array[iter->cur];
    iter->valid = 1;
  }
}

void SEQ_iterator_next(SeqIterator *iter)
{
  if (++iter->cur < iter->tot) {
    iter->seq = iter->array[iter->cur];
  }
  else {
    iter->valid = 0;
  }
}

void SEQ_iterator_end(SeqIterator *iter)
{
  if (iter->array) {
    MEM_freeN(iter->array);
  }

  iter->valid = 0;
}
