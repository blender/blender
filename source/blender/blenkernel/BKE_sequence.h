/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.	
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_SEQUENCE_H
#define BKE_SEQUENCE_H

struct Editing;
struct Sequence;

/* free */

void seq_free_sequence(struct Sequence *seq);
void seq_free_editing(struct Editing *ed);

/* sequence iterator */

typedef struct SeqIterator {
	struct Sequence **array;
	int tot, cur;

	struct Sequence *seq;
	int valid;
} SeqIterator;

void seq_begin(struct Editing *ed, SeqIterator *iter);
void seq_next(SeqIterator *iter);
void seq_end(SeqIterator *iter);

void seq_array(struct Editing *ed, struct Sequence ***array, int *tot);

#define SEQ_BEGIN(ed, seq) \
	{ \
		SeqIterator iter;\
		for(seq_begin(ed, &iter); iter.valid; seq_next(&iter)) { \
			seq= iter.seq;

#define SEQ_END \
		} \
		seq_end(&iter); \
	}

#endif

