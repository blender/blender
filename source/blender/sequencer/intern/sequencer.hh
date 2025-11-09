/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_span.hh"

struct Scene;
struct Strip;
struct StripProxy;

namespace blender::seq {

/**
 * Cache must be freed before calling this function
 * since it leaves the #Editing::seqbase in an invalid state.
 */
void seq_free_strip_recurse(Scene *scene, Strip *strip, bool do_id_user);
StripProxy *seq_strip_proxy_alloc();
/**
 * Find effect strips, that use strip `strip` as one of inputs.
 * If lookup hash doesn't exist, it will be created. If hash is tagged as invalid, it will be
 * rebuilt.
 *
 * \param key: pointer to Strip inside of meta strip
 *
 * \return collection of effect strips
 */
Span<Strip *> SEQ_lookup_effects_by_strip(Editing *ed, const Strip *key);

}  // namespace blender::seq
