/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct ImBuf;
struct SeqRenderData;
struct Sequence;
struct anim;

#define PROXY_MAXFILE (2 * FILE_MAXDIR + FILE_MAXFILE)
ImBuf *seq_proxy_fetch(const SeqRenderData *context, Sequence *seq, int timeline_frame);
bool seq_proxy_get_custom_file_filepath(Sequence *seq, char *name, int view_id);
void free_proxy_seq(Sequence *seq);
void seq_proxy_index_dir_set(ImBufAnim *anim, const char *base_dir);
