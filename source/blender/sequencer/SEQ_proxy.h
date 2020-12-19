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

struct Scene;
struct Main;
struct GSet;
struct Sequence;
struct ListBase;
struct Depsgraph;
struct SeqIndexBuildContext;
struct ListBase;

bool SEQ_proxy_rebuild_context(struct Main *bmain,
                               struct Depsgraph *depsgraph,
                               struct Scene *scene,
                               struct Sequence *seq,
                               struct GSet *file_list,
                               struct ListBase *queue);
void SEQ_proxy_rebuild(struct SeqIndexBuildContext *context,
                       short *stop,
                       short *do_update,
                       float *progress);
void SEQ_proxy_rebuild_finish(struct SeqIndexBuildContext *context, bool stop);
void SEQ_proxy_set(struct Sequence *seq, bool value);
bool SEQ_can_use_proxy(struct Sequence *seq, int psize);
int SEQ_rendersize_to_proxysize(int render_size);
double SEQ_rendersize_to_scale_factor(int size);

#ifdef __cplusplus
}
#endif
