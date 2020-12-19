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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Sequence;
struct Editing;

typedef struct SeqIterator {
  struct Sequence **array;
  int tot, cur;

  struct Sequence *seq;
  int valid;
} SeqIterator;

#define SEQ_ALL_BEGIN(ed, _seq) \
  { \
    SeqIterator iter_macro; \
    for (SEQ_iterator_begin(ed, &iter_macro, false); iter_macro.valid; \
         SEQ_iterator_next(&iter_macro)) { \
      _seq = iter_macro.seq;

#define SEQ_ALL_END \
  } \
  SEQ_iterator_end(&iter_macro); \
  } \
  ((void)0)

#define SEQ_CURRENT_BEGIN(_ed, _seq) \
  { \
    SeqIterator iter_macro; \
    for (SEQ_iterator_begin(_ed, &iter_macro, true); iter_macro.valid; \
         SEQ_iterator_next(&iter_macro)) { \
      _seq = iter_macro.seq;

#define SEQ_CURRENT_END SEQ_ALL_END

void SEQ_iterator_begin(struct Editing *ed, SeqIterator *iter, const bool use_current_sequences);
void SEQ_iterator_next(SeqIterator *iter);
void SEQ_iterator_end(SeqIterator *iter);
int SEQ_iterator_seqbase_recursive_apply(struct ListBase *seqbase,
                                         int (*apply_fn)(struct Sequence *seq, void *),
                                         void *arg);
int SEQ_iterator_recursive_apply(struct Sequence *seq,
                                 int (*apply_fn)(struct Sequence *, void *),
                                 void *arg);

#ifdef __cplusplus
}
#endif
