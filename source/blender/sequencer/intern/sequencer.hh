/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector_set.hh"

/** \file
 * \ingroup sequencer
 */

struct Scene;
struct Strip;
struct StripProxy;
/**
 * Cache must be freed before calling this function
 * since it leaves the #Editing::seqbase in an invalid state.
 */
void seq_free_sequence_recurse(Scene *scene, Strip *seq, bool do_id_user);
StripProxy *seq_strip_proxy_alloc();
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
Strip *seq_sequence_lookup_meta_by_seq(const Scene *scene, const Strip *key);
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
blender::Span<Strip *> seq_sequence_lookup_effects_by_seq(const Scene *scene, const Strip *key);
