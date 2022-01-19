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
struct Main;
struct Scene;
struct Sequence;

extern struct ListBase seqbase_clipboard;
extern struct ListBase fcurves_clipboard;
extern int seqbase_clipboard_frame;
void SEQ_clipboard_pointers_store(struct Main *bmain, struct ListBase *seqbase);
void SEQ_clipboard_pointers_restore(struct ListBase *seqbase, struct Main *bmain);
void SEQ_clipboard_free(void);
void SEQ_clipboard_active_seq_name_store(struct Scene *scene);
/**
 * Check if strip was active when it was copied. User should restrict this check to pasted strips
 * before ensuring original name, because strip name comparison is used to check.
 *
 * \param pasted_seq: Strip that is pasted(duplicated) from clipboard
 * \return true if strip was active, false otherwise
 */
bool SEQ_clipboard_pasted_seq_was_active(struct Sequence *pasted_seq);

#ifdef __cplusplus
}
#endif
