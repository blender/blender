/* SPDX-FileCopyrightText: 2004 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
struct SeqCollection;
/**
 * Cache must be freed before calling this function
 * since it leaves the seqbase in an invalid state.
 */
void seq_free_sequence_recurse(struct Scene *scene, struct Sequence *seq, bool do_id_user);
struct StripProxy *seq_strip_proxy_alloc(void);
/**
 * Find meta strip, that contains strip `key`.
 * If lookup hash doesn't exist, it will be created. If hash is tagged as invalid, it will be
 * rebuilt.
 *
 * \param scene: scene that owns lookup hash
 * \param key: pointer to Sequence inside of meta strip
 *
 * \return pointer to meta strip
 */
struct Sequence *seq_sequence_lookup_meta_by_seq(const struct Scene *scene,
                                                 const struct Sequence *key);
/**
 * Find effect strips, that use strip `seq` as one of inputs.
 * If lookup hash doesn't exist, it will be created. If hash is tagged as invalid, it will be
 * rebuilt.
 *
 * \param scene: scene that owns lookup hash
 * \param key: pointer to Sequence inside of meta strip
 *
 * \return collection of effect strips
 */
struct SeqCollection *seq_sequence_lookup_effects_by_seq(const struct Scene *scene,
                                                         const struct Sequence *key);
#ifdef __cplusplus
}
#endif
