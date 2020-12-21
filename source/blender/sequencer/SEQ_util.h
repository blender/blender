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

#include "DNA_scene_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Scene;
struct Sequence;
struct ListBase;

void SEQ_set_scale_to_fit(const struct Sequence *seq,
                          const int image_width,
                          const int image_height,
                          const int preview_width,
                          const int preview_height,
                          const eSeqImageFitMethod fit_method);
void SEQ_sort(struct Scene *scene);
void SEQ_sequence_base_unique_name_recursive(ListBase *seqbasep, struct Sequence *seq);
const char *SEQ_sequence_give_name(struct Sequence *seq);
ListBase *SEQ_get_seqbase_from_sequence(struct Sequence *seq, int *r_offset);
const struct Sequence *SEQ_get_topmost_sequence(const struct Scene *scene, int frame);
struct ListBase *SEQ_get_seqbase_by_seq(struct ListBase *seqbase, struct Sequence *seq);
struct Sequence *SEQ_sequence_from_strip_elem(ListBase *seqbase, struct StripElem *se);
struct Sequence *SEQ_get_sequence_by_name(struct ListBase *seqbase,
                                          const char *name,
                                          bool recursive);
struct Mask *SEQ_active_mask_get(struct Scene *scene);
void SEQ_alpha_mode_from_file_extension(struct Sequence *seq);
bool SEQ_sequence_has_source(struct Sequence *seq);

#ifdef __cplusplus
}
#endif
