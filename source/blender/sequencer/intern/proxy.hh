/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct ImBuf;
struct MovieReader;
struct SeqRenderData;
struct Strip;
struct anim;

namespace blender::seq {

struct RenderData;

#define PROXY_MAXFILE (2 * FILE_MAXDIR + FILE_MAXFILE)
ImBuf *seq_proxy_fetch(const RenderData *context, Strip *strip, int timeline_frame);
bool seq_proxy_get_custom_file_filepath(Strip *strip, char *filepath, int view_id);
void free_strip_proxy(Strip *strip);
void seq_proxy_index_dir_set(MovieReader *anim, const char *base_dir);

}  // namespace blender::seq
