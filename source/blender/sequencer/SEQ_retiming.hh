/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_span.hh"

struct Sequence;
struct SeqRetimingHandle;

blender::MutableSpan<SeqRetimingHandle> SEQ_retiming_handles_get(const Sequence *seq);
