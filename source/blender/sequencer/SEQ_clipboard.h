/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2004 Blender Foundation */

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
extern struct ListBase drivers_clipboard;
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
