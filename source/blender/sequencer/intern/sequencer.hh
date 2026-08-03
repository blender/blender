/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_span.hh"

namespace blender {

struct Editing;
struct Scene;
struct Strip;
struct StripProxy;

namespace seq {

/**
 * Cache must be freed before calling this function
 * since it leaves the #Editing::seqbase in an invalid state.
 */
void seq_free_strip_recurse(Scene *scene, Strip *strip, bool do_id_user);
StripProxy *seq_strip_proxy_alloc();

}  // namespace seq
}  // namespace blender
