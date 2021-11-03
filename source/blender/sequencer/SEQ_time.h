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

struct ListBase;
struct Scene;
struct Sequence;
struct rctf;

void SEQ_timeline_init_boundbox(const struct Scene *scene, struct rctf *rect);
void SEQ_timeline_expand_boundbox(const struct ListBase *seqbase, struct rctf *rect);
void SEQ_timeline_boundbox(const struct Scene *scene,
                           const struct ListBase *seqbase,
                           struct rctf *rect);
float SEQ_time_sequence_get_fps(struct Scene *scene, struct Sequence *seq);
int SEQ_time_find_next_prev_edit(struct Scene *scene,
                                 int timeline_frame,
                                 const short side,
                                 const bool do_skip_mute,
                                 const bool do_center,
                                 const bool do_unselected);
void SEQ_time_update_sequence(struct Scene *scene, struct ListBase *seqbase, struct Sequence *seq);
bool SEQ_time_strip_intersects_frame(const struct Sequence *seq, const int timeline_frame);
void SEQ_time_update_meta_strip_range(struct Scene *scene, struct Sequence *seq_meta);

#ifdef __cplusplus
}
#endif
