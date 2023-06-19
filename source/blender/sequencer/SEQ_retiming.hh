/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_span.hh"

struct Sequence;
struct SeqRetimingHandle;

blender::MutableSpan<SeqRetimingHandle> SEQ_retiming_handles_get(const Sequence *seq);
