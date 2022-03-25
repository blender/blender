/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2004 Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Scene;
struct Sequence;
struct StripProxy;
/**
 * Cache must be freed before calling this function
 * since it leaves the seqbase in an invalid state.
 */
void seq_free_sequence_recurse(struct Scene *scene, struct Sequence *seq, bool do_id_user);
struct StripProxy *seq_strip_proxy_alloc(void);

#ifdef __cplusplus
}
#endif
