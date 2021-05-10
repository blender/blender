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

#include "BLI_ghash.h"

struct Editing;
struct Sequence;
struct GSet;
struct GSetIterator;

#define SEQ_ITERATOR_FOREACH(var, collection) \
  for (SeqIterator iter = {NULL}; SEQ_iterator_ensure(collection, &iter, &var) && var != NULL; \
       var = SEQ_iterator_yield(&iter))

#define SEQ_ALL_BEGIN(ed, var) \
  { \
    if (ed != NULL) { \
      SeqCollection *all_strips = SEQ_query_all_strips_recursive(&ed->seqbase); \
      GSetIterator gsi; \
      GSET_ITER (gsi, all_strips->set) { \
        var = (Sequence *)(BLI_gsetIterator_getKey(&gsi));

#define SEQ_ALL_END \
  } \
  SEQ_collection_free(all_strips); \
  } \
  } \
  ((void)0)

typedef struct SeqCollection {
  struct SeqCollection *next, *prev;
  struct GSet *set;
} SeqCollection;

typedef struct SeqIterator {
  GSetIterator gsi;
  SeqCollection *collection;
  bool iterator_initialized;
} SeqIterator;

bool SEQ_iterator_ensure(SeqCollection *collection,
                         SeqIterator *iterator,
                         struct Sequence **r_seq);
struct Sequence *SEQ_iterator_yield(SeqIterator *iterator);

SeqCollection *SEQ_collection_create(void);
bool SEQ_collection_append_strip(struct Sequence *seq, SeqCollection *data);
void SEQ_collection_free(SeqCollection *collection);
void SEQ_collection_merge(SeqCollection *collection_dst, SeqCollection *collection_src);
void SEQ_collection_expand(struct ListBase *seqbase,
                           SeqCollection *collection,
                           void query_func(struct Sequence *seq_reference,
                                           struct ListBase *seqbase,
                                           SeqCollection *collection));
SeqCollection *SEQ_query_by_reference(struct Sequence *seq_reference,
                                      struct ListBase *seqbase,
                                      void seq_query_func(struct Sequence *seq_reference,
                                                          struct ListBase *seqbase,
                                                          SeqCollection *collection));
SeqCollection *SEQ_query_selected_strips(struct ListBase *seqbase);
SeqCollection *SEQ_query_all_strips_recursive(ListBase *seqbase);

#ifdef __cplusplus
}
#endif
